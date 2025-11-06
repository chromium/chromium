// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.annotation.VisibleForTesting.PRIVATE;
import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_DARK;
import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_LIGHT;

import static org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant.PRICE_INSIGHTS;
import static org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant.PRICE_TRACKING;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Browser;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.app.tab_activity_glue.PopupCreator;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchController;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchControllerFactory;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchMetrics;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.browserservices.intents.SessionHolder;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.FinishReason;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.features.CustomTabNavigationBarController;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabHistoryIphController;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.firstrun.FirstRunSignInProcessor;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.history.HistoryManager;
import org.chromium.chrome.browser.history.HistoryManagerUtils;
import org.chromium.chrome.browser.history.HistoryTabHelper;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.page_info.ChromePageInfo;
import org.chromium.chrome.browser.page_info.ChromePageInfoHighlight;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TrustedCdn;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarCoordinator;
import org.chromium.chrome.browser.ui.web_app_header.WebAppHeaderUtils;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
import org.chromium.components.page_info.PageInfoController.OpenedFromSource;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.util.ColorUtils;

/** The activity for custom tabs. It will be launched on top of a client's task. */
public class CustomTabActivity extends BaseCustomTabActivity {
    private static final String TAG = "CustomTab";
    private SessionHolder<?> mSession;

    private final CustomTabsConnection mConnection = CustomTabsConnection.getInstance();
    private int mNumOmniboxNavigationEventsPerSession;

    /** Prevents Tapjacking on T-. See crbug.com/1430867 */
    private static final boolean sPreventTouches =
            Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU;

    private CustomTabsOpenTimeRecorder mOpenTimeRecorder;

    private static final boolean sBlockTouchesDuringEnterAnimation =
            ChromeFeatureList.sCctBlockTouchesDuringEnterAnimation.isEnabled();
    private boolean mIsEnterAnimationCompleted;
    private @Nullable AuxiliarySearchController mAuxiliarySearchController;
    private CustomTabActivityTimeoutHandler mTimeoutHandler;
    private static Runnable sOnFinishCallbackForTesting;
    private final CustomTabActivityTabProvider.Observer mTabChangeObserver =
            new CustomTabActivityTabProvider.Observer() {
                @Override
                public void onInitialTabCreated(@NonNull Tab tab, int mode) {
                    onTabInitOrSwapped(tab);
                }

                @Override
                public void onTabSwapped(@NonNull Tab tab) {
                    onTabInitOrSwapped(tab);
                }

                @Override
                public void onAllTabsClosed() {
                    resetPostMessageHandlersForCurrentSession();
                }
            };

    private void onTabInitOrSwapped(@Nullable Tab tab) {
        resetPostMessageHandlersForCurrentSession();
        if (tab != null) maybeCreateHistoryTabHelper(tab);
    }

    private void maybeCreateHistoryTabHelper(Tab tab) {
        if (!HistoryManager.isAppSpecificHistoryEnabled() || getIntentDataProvider().isAuthTab()) {
            return;
        }
        String appId = getIntentDataProvider().getClientPackageNameIdentitySharing();
        if (appId != null) HistoryTabHelper.from(tab).setAppId(appId, tab.getWebContents());
    }

    @Override
    protected Drawable getBackgroundDrawable() {
        int initialBackgroundColor =
                getIntentDataProvider().getColorProvider().getInitialBackgroundColor();
        if (getIntentDataProvider().isTrustedIntent()
                && initialBackgroundColor != Color.TRANSPARENT) {
            return new ColorDrawable(initialBackgroundColor);
        } else {
            return super.getBackgroundDrawable();
        }
    }

    @Override
    protected void changeBackgroundColorForResizing() {
        if (!getBaseCustomTabRootUiCoordinator().changeBackgroundColorForResizing()) {
            super.changeBackgroundColorForResizing();
        }
    }

    @Override
    public void performPreInflationStartup() {
        super.performPreInflationStartup();
        var savedInstanceState = getSavedInstanceState();
        mTimeoutHandler = new CustomTabActivityTimeoutHandler(this::finish, getIntent());

        // If the activity is being recreated, #onEnterAnimationComplete() doesn't get called.
        // So, we need to manually set mIsEnterAnimationCompleted to true. See crbug.com/399194973.
        if (sBlockTouchesDuringEnterAnimation) {
            if (savedInstanceState != null) {
                mIsEnterAnimationCompleted = true;
            }
        }
        mOpenTimeRecorder =
                new CustomTabsOpenTimeRecorder(
                        getLifecycleDispatcher(),
                        getCustomTabActivityNavigationController(),
                        this::isFinishing,
                        getIntentDataProvider());
        getCustomTabActivityTabProvider().addObserver(mTabChangeObserver);
        // We might have missed an onInitialTabCreated event.
        onTabInitOrSwapped(getCustomTabActivityTabProvider().getTab());

        mSession = getIntentDataProvider().getSession();

        boolean drawEdgeToEdge =
                mEdgeToEdgeControllerSupplier.get() != null
                        && mEdgeToEdgeControllerSupplier.get().isDrawingToEdge()
                        && mEdgeToEdgeControllerSupplier.get().isPageOptedIntoEdgeToEdge();
        var systemBarColorHelper =
                getEdgeToEdgeManager() != null
                        ? getEdgeToEdgeManager().getEdgeToEdgeSystemBarColorHelper()
                        : null;
        CustomTabNavigationBarController.update(
                getWindow(), getIntentDataProvider(), this, drawEdgeToEdge, systemBarColorHelper);

        mTimeoutHandler.restoreInstanceState(savedInstanceState);
    }

    @Override
    public void performPostInflationStartup() {
        super.performPostInflationStartup();

        mRootUiCoordinator.getStatusBarColorController().updateStatusBarColor();

        // Properly attach tab's InfoBarContainer to the view hierarchy if the tab is already
        // attached to a ChromeActivity, as the main tab might have been initialized prior to
        // inflation.
        if (getCustomTabActivityTabProvider().getTab() != null) {
            ViewGroup bottomContainer = findViewById(R.id.bottom_container);
            InfoBarContainer.get(getCustomTabActivityTabProvider().getTab())
                    .setParentView(bottomContainer);
        }

        int toolbarColor = getIntentDataProvider().getColorProvider().getToolbarColor();
        // Not setting the task title and icon or setting them to null (pre-Android T) will preserve
        // the client app's title and icon.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            var taskDescription =
                    new ActivityManager.TaskDescription.Builder()
                            .setPrimaryColor(toolbarColor)
                            .setStatusBarColor(toolbarColor)
                            .build();
            setTaskDescription(taskDescription);
        } else {
            setTaskDescription(new ActivityManager.TaskDescription(null, null, toolbarColor));
        }

        GoogleBottomBarCoordinator googleBottomBarCoordinator =
                getBaseCustomTabRootUiCoordinator().getGoogleBottomBarCoordinator();

        if (googleBottomBarCoordinator != null) {
            View googleBottomBarView = googleBottomBarCoordinator.createGoogleBottomBarView();
            CustomTabBottomBarDelegate delegate = getCustomTabBottomBarDelegate();
            delegate.setBottomBarHeight(googleBottomBarCoordinator.getBottomBarHeightInPx());
            delegate.setKeepContentView(true);
            delegate.setBottomBarContentView(googleBottomBarView);
            delegate.setCustomButtonsUpdater(googleBottomBarCoordinator::updateBottomBarButton);
        }

        getCustomTabBottomBarDelegate().showBottomBarIfNecessary();
    }

    @Override
    public void finishNativeInitialization() {
        if (!getIntentDataProvider().isInfoPage()) {
            FirstRunSignInProcessor.openSyncSettingsIfScheduled(this);
        }

        mConnection.showSignInToastIfNecessary(mSession, getIntent(), getProfileProviderSupplier());

        new CustomTabTrustedCdnPublisherUrlVisibility(
                getWindowAndroid(),
                getLifecycleDispatcher(),
                () -> {
                    if (ChromeFeatureList.isEnabled(
                            ChromeFeatureList.CCT_EXTEND_TRUSTED_CDN_PUBLISHER)) {
                        return mConnection.isTrustedCdnPublisherUrlPackage(
                                getIntentDataProvider().getClientPackageName());
                    }
                    String urlPackage = mConnection.getTrustedCdnPublisherUrlPackage();
                    return urlPackage != null
                            && urlPackage.equals(
                                    mConnection.getClientPackageNameForSession(mSession));
                });
        super.finishNativeInitialization();

        // Window bounds adjustments are called here because we probe WebContents' width and height
        // from the native object.
        // The condition including {@link #getSavedInstanceState} should be false iff the Activity
        // has been recreated. If the Activity has been recreated, we should ignore the window
        // features requested in the Intent as they should apply only for the initial launch of the
        // Activity.
        if (getIntentDataProvider().getUiType() == CustomTabsUiType.POPUP
                && getSavedInstanceState() == null
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.ANDROID_WINDOW_POPUP_RESIZE_AFTER_SPAWN)) {
            PopupCreator.adjustWindowBoundsToRequested(
                    this, getIntentDataProvider().getRequestedWindowFeatures());
        }
    }

    @Override
    protected void handleFinishAndClose(@FinishReason int reason, boolean warmupOnFinish) {
        if (mOpenTimeRecorder != null) mOpenTimeRecorder.updateCloseCause();
        super.handleFinishAndClose(reason, warmupOnFinish);
    }

    @Override
    protected void onDeferredStartup() {
        super.onDeferredStartup();

        if (isActivityFinishingOrDestroyed()) return;

        if (ChromeFeatureList.sAndroidAppIntegrationMultiDataSource.isEnabled()) {
            // TODO(https://crbug.com/397457989): Removes this log once the feature is launched.
            Log.i(TAG, "To create AuxiliarySearchController on deferred startup.");
            AuxiliarySearchControllerFactory.getInstance()
                    .setIsTablet(DeviceFormFactor.isWindowOnTablet(getWindowAndroid()));
            mAuxiliarySearchController =
                    AuxiliarySearchControllerFactory.getInstance()
                            .createAuxiliarySearchController(
                                    CustomTabActivity.this,
                                    mTabModelProfileSupplier.get(),
                                    null,
                                    AuxiliarySearchController.AuxiliarySearchHostType.CUSTOM_TAB);
            if (mAuxiliarySearchController != null) {
                AuxiliarySearchMetrics.recordTimeToCreateControllerInCustomTab(
                        TimeUtils.elapsedRealtimeMillis() - getOnCreateTimestampMs());
            }
        }
    }

    @Override
    public void onResume() {
        if (mTimeoutHandler != null) mTimeoutHandler.onResume();
        super.onResume();
    }

    @Override
    public void onPause() {
        super.onPause();

        if (mAuxiliarySearchController != null) {
            Tab tab = getCustomTabActivityTabProvider().getTab();
            if (tab != null) {
                mAuxiliarySearchController.donateCustomTabs(tab.getUrl(), tab.getTimestampMillis());
            }
        }
    }

    @Override
    public void onSaveInstanceState(@NonNull Bundle outState) {
        if (mTimeoutHandler != null) mTimeoutHandler.onSaveInstanceState(outState);
        super.onSaveInstanceState(outState);
    }

    @Override
    protected void onUserLeaveHint() {
        if (mTimeoutHandler != null) mTimeoutHandler.onUserLeaveHint();
        if (mOpenTimeRecorder != null) mOpenTimeRecorder.onUserLeaveHint();
        super.onUserLeaveHint();
    }

    private void resetPostMessageHandlersForCurrentSession() {
        Tab tab = getCustomTabActivityTabProvider().getTab();
        WebContents webContents = tab == null ? null : tab.getWebContents();
        mConnection.resetPostMessageHandlerForSession(
                getIntentDataProvider().getSession(), webContents);
    }

    @Override
    public String getPackageName() {
        if (mShouldOverridePackage
                && getIntentDataProvider()
                        instanceof CustomTabIntentDataProvider intentDataProvider) {
            return intentDataProvider.getInsecureClientPackageNameForOnFinishAnimation();
        }
        return super.getPackageName();
    }

    @Override
    public boolean onOptionsItemSelected(
            int itemId, @Nullable Bundle menuItemData, @Nullable MotionEventInfo triggeringMotion) {
        int menuIndex =
                CustomTabAppMenuPropertiesDelegate.getIndexOfMenuItemFromBundle(menuItemData);
        if (menuIndex >= 0) {
            ((CustomTabIntentDataProvider) getIntentDataProvider())
                    .clickMenuItemWithUrlAndTitle(
                            this,
                            menuIndex,
                            getActivityTab().getUrl().getSpec(),
                            getActivityTab().getTitle());
            RecordUserAction.record("CustomTabsMenuCustomMenuItem");
            return true;
        }

        return super.onOptionsItemSelected(itemId, menuItemData, triggeringMotion);
    }

    @Override
    public boolean onMenuOrKeyboardAction(
            int id, boolean fromMenu, @Nullable MotionEventInfo triggeringMotion) {
        if (id == R.id.bookmark_this_page_id) {
            mTabBookmarkerSupplier.get().addOrEditBookmark(getActivityTab());
            RecordUserAction.record("MobileMenuAddToBookmarks");
            return true;
        } else if (id == R.id.open_in_browser_id) {
            // Need to get tab before calling openCurrentUrlInBrowser or else it will be null.
            Tab tab = getCustomTabActivityTabProvider().getTab();
            if (tab != null) {
                RecordUserAction.record("CustomTabsMenuOpenInChrome");
                // Need to notify *before* opening in browser, to ensure engagement signal will be
                // fired correctly.
                mConnection.notifyOpenInBrowser(mSession, tab);
                getCustomTabActivityNavigationController().openCurrentUrlInBrowser();
            }
            return true;
        } else if (id == R.id.info_menu_id) {
            Tab tab = getTabModelSelector().getCurrentTab();
            if (tab == null) return false;
            String publisher = TrustedCdn.getContentPublisher(tab);
            ChromePageInfo pageInfo =
                    new ChromePageInfo(
                            getModalDialogManagerSupplier(),
                            publisher,
                            OpenedFromSource.MENU,
                            mRootUiCoordinator.getMerchantTrustSignalsCoordinatorSupplier()::get,
                            mRootUiCoordinator.getEphemeralTabCoordinatorSupplier(),
                            getTabCreator(getCurrentTabModel().isIncognito()));
            boolean isMinimalUiVisible =
                    WebAppHeaderUtils.isMinimalUiVisible(
                            getIntentDataProvider(),
                            getBaseCustomTabRootUiCoordinator().getDesktopWindowStateManager());
            boolean isTWA = getIntentDataProvider().isTrustedWebActivity();
            if (ChromeFeatureList.sAndroidWebAppMenuButton.isEnabled()
                    && isTWA
                    && isMinimalUiVisible) {
                String packageName = getIntentDataProvider().getClientPackageName();
                pageInfo.show(tab, ChromePageInfoHighlight.noHighlight(), packageName);
                return true;
            }
            pageInfo.show(tab, ChromePageInfoHighlight.noHighlight());
            return true;
        } else if (id == R.id.price_insights_menu_id) {
            getBaseCustomTabRootUiCoordinator().runPriceInsightsAction();
            RecordUserAction.record("CustomTabsMenuPriceInsights");
            CustomTabToolbar toolbar = findViewById(R.id.toolbar);
            toolbar.maybeRecordHistogramForAdaptiveToolbarButtonFallbackUi(PRICE_INSIGHTS);
            return true;
        } else if (id == R.id.enable_price_tracking_menu_id) {
            CustomTabToolbar toolbar = findViewById(R.id.toolbar);
            toolbar.maybeRecordHistogramForAdaptiveToolbarButtonFallbackUi(PRICE_TRACKING);
            // Let the flow proceed to superclass for processing the action.
        } else if (id == R.id.open_history_menu_id) {
            // The menu is visible only when the app-specific history is enabled. Assert that.
            assert HistoryManager.isAppSpecificHistoryEnabled();
            HistoryManagerUtils.showAppSpecificHistoryManager(
                    this,
                    getTabModelSelector().getCurrentModel().getProfile(),
                    getIntentDataProvider().getClientPackageNameIdentitySharing());

            CustomTabHistoryIphController historyIph =
                    getBaseCustomTabRootUiCoordinator().getHistoryIphController();
            if (historyIph != null) {
                historyIph.notifyUserEngaged();
            }
            return true;
        }
        return super.onMenuOrKeyboardAction(id, fromMenu, triggeringMotion);
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
        // We should block touches while the enter animation is still running. An enter animation
        // that makes the Activity "appear" transparent for a long time may lead users to touch
        // elements on the webpage that's loaded within a currently invisible CCT.
        if (sBlockTouchesDuringEnterAnimation && !mIsEnterAnimationCompleted) {
            return true;
        }
        if (sPreventTouches && shouldPreventTouch()) {
            // Discard the events which may be trickling down from an overlay activity above.
            return true;
        }
        return super.dispatchTouchEvent(ev);
    }

    @Override
    public void finish() {
        if (sOnFinishCallbackForTesting != null) sOnFinishCallbackForTesting.run();

        RecordHistogram.recordLinearCountHistogram(
                "CustomTabs.Omnibox.NumNavigationsPerSession",
                mNumOmniboxNavigationEventsPerSession,
                1,
                10,
                10);

        super.finish();
    }

    @VisibleForTesting(otherwise = PRIVATE)
    boolean shouldPreventTouch() {
        if (ApplicationStatus.getStateForActivity(this) == ActivityState.RESUMED) return false;
        return true;
    }

    /**
     * Show the web page with CustomTabActivity, without any navigation control. Used in showing the
     * terms of services page or help pages for Chrome.
     *
     * @param context The current activity context.
     * @param url The url of the web page.
     */
    public static void showInfoPage(Context context, String url) {
        // TODO(xingliu): The title text will be the html document title, figure out if we want to
        // use Chrome strings here as EmbedContentViewActivity does.
        CustomTabsIntent customTabIntent =
                new CustomTabsIntent.Builder()
                        .setShowTitle(true)
                        .setColorScheme(
                                ColorUtils.inNightMode(context)
                                        ? COLOR_SCHEME_DARK
                                        : COLOR_SCHEME_LIGHT)
                        .build();
        customTabIntent.intent.setData(Uri.parse(url));

        Intent intent =
                LaunchIntentDispatcher.createCustomTabActivityIntent(
                        context, customTabIntent.intent);
        intent.setPackage(context.getPackageName());
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_UI_TYPE, CustomTabsUiType.INFO_PAGE);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        if (!(context instanceof Activity)) intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        IntentUtils.addTrustedIntentExtras(intent);

        context.startActivity(intent);
    }

    @Override
    protected boolean requiresFirstRunToBeCompleted(Intent intent) {
        // Custom Tabs can be used to open Chrome help pages before the ToS has been accepted.
        if (CustomTabIntentDataProvider.isTrustedCustomTab(intent, mSession)
                && IntentUtils.safeGetIntExtra(
                                intent,
                                CustomTabIntentDataProvider.EXTRA_UI_TYPE,
                                CustomTabIntentDataProvider.CustomTabsUiType.DEFAULT)
                        == CustomTabIntentDataProvider.CustomTabsUiType.INFO_PAGE) {
            return false;
        }

        return super.requiresFirstRunToBeCompleted(intent);
    }

    @Override
    protected LaunchCauseMetrics createLaunchCauseMetrics() {
        return new CustomTabLaunchCauseMetrics(this);
    }

    public NightModeStateProvider getNightModeStateProviderForTesting() {
        return super.getNightModeStateProvider();
    }

    @Override
    protected void setDefaultTaskDescription() {
        // getIntentDataProvider() is not ready when the super calls this method. So, we skip
        // setting the task description here, and do it in #performPostInflationStartup();
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if (resultCode != Activity.RESULT_OK) return;

        var searchClient = getBaseCustomTabRootUiCoordinator().getCustomTabSearchClient();
        if (searchClient.isOmniboxResult(requestCode, data)) {
            LoadUrlParams params = searchClient.getOmniboxResult(requestCode, resultCode, data);

            RecordHistogram.recordBooleanHistogram(
                    "CustomTabs.Omnibox.FocusResultedInNavigation", params != null);

            if (params == null) return;

            mNumOmniboxNavigationEventsPerSession++;
            // Yield to give the called activity time to close.
            // Loading URL directly will result in Activity closing after URL loading completes.
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    () -> getCustomTabActivityTabProvider().getTab().loadUrl(params));
        }

        if (HistoryManager.isAppSpecificHistoryEnabled()
                && requestCode == HistoryManagerUtils.HISTORY_REQUEST_CODE) {
            LoadUrlParams params =
                    new LoadUrlParams(
                            data.getData().toString(),
                            IntentHandler.getTransitionTypeFromIntent(data, PageTransition.LINK));
            getCustomTabActivityTabProvider().getTab().loadUrl(params);
        }
    }

    @Override
    public void onEnterAnimationComplete() {
        super.onEnterAnimationComplete();

        mIsEnterAnimationCompleted = true;
    }

    @VisibleForTesting
    public static void setOnFinishCallbackForTesting(Runnable callback) {
        sOnFinishCallbackForTesting = callback;
        ResettersForTesting.register(() -> sOnFinishCallbackForTesting = null);
    }
}
