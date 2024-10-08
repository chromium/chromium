// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.annotation.VisibleForTesting.PRIVATE;
import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_DARK;
import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_LIGHT;

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
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.backup.BackupSigninProcessor;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.dependency_injection.BaseCustomTabActivityComponent;
import org.chromium.chrome.browser.customtabs.features.CustomTabNavigationBarController;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabHistoryIPHController;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityCommonsModule;
import org.chromium.chrome.browser.firstrun.FirstRunSignInProcessor;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fonts.FontPreloader;
import org.chromium.chrome.browser.history.HistoryManager;
import org.chromium.chrome.browser.history.HistoryManagerUtils;
import org.chromium.chrome.browser.history.HistoryTabHelper;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.page_info.ChromePageInfo;
import org.chromium.chrome.browser.page_info.ChromePageInfoHighlight;
import org.chromium.chrome.browser.searchwidget.SearchActivityClientImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TrustedCdn;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarCoordinator;
import org.chromium.components.cached_flags.AllCachedFieldTrialParameters;
import org.chromium.components.page_info.PageInfoController.OpenedFromSource;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.util.ColorUtils;

/** The activity for custom tabs. It will be launched on top of a client's task. */
public class CustomTabActivity extends BaseCustomTabActivity {
    private CustomTabsSessionToken mSession;

    private final CustomTabsConnection mConnection = CustomTabsConnection.getInstance();
    private int mNumOmniboxNavigationEventsPerSession;

    /** Contains all the parameters of the EXPERIMENTS_FOR_AGSA feature. */
    public static final AllCachedFieldTrialParameters EXPERIMENTS_FOR_AGSA_PARAMS =
            ChromeFeatureList.newAllCachedFieldTrialParameters(
                    ChromeFeatureList.EXPERIMENTS_FOR_AGSA);

    /** Prevents Tapjacking on T-. See crbug.com/1430867 */
    private static final boolean sPreventTouches =
            Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU;

    private CustomTabsOpenTimeRecorder mOpenTimeRecorder;

    /**
     * The last MotionEvent object blocked due to the activity being in paused state.  We're
     * interested in MotionEvent#ACTION_DOWN which is likely the very first event received when
     * multi-window mode is entered. We inject this one after the activity is resumed (or
     * it regains the focus) in order to recover the corresponding user gesture which otherwise
     * would have gone missing.
     */
    private MotionEvent mBlockedEvent;

    private CustomTabActivityTabProvider.Observer mTabChangeObserver =
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
        if (!HistoryManager.isAppSpecificHistoryEnabled() || mIntentDataProvider.isAuthTab()) {
            return;
        }
        String appId = mIntentDataProvider.getClientPackageNameIdentitySharing();
        if (appId != null) HistoryTabHelper.from(tab).setAppId(appId, tab.getWebContents());
    }

    @Override
    protected BaseCustomTabActivityComponent createComponent(
            ChromeActivityCommonsModule commonsModule) {
        BaseCustomTabActivityComponent component = super.createComponent(commonsModule);
        mOpenTimeRecorder =
                new CustomTabsOpenTimeRecorder(
                        getLifecycleDispatcher(),
                        mNavigationController,
                        this::isFinishing,
                        mIntentDataProvider);
        return component;
    }

    @Override
    protected Drawable getBackgroundDrawable() {
        int initialBackgroundColor =
                mIntentDataProvider.getColorProvider().getInitialBackgroundColor();
        if (mIntentDataProvider.isTrustedIntent() && initialBackgroundColor != Color.TRANSPARENT) {
            return new ColorDrawable(initialBackgroundColor);
        } else {
            return super.getBackgroundDrawable();
        }
    }

    @Override
    protected void changeBackgroundColorForResizing() {
        if (!mBaseCustomTabRootUiCoordinator.changeBackgroundColorForResizing()) {
            super.changeBackgroundColorForResizing();
        }
    }

    @Override
    public void performPreInflationStartup() {
        super.performPreInflationStartup();
        mTabProvider.addObserver(mTabChangeObserver);
        // We might have missed an onInitialTabCreated event.
        onTabInitOrSwapped(mTabProvider.getTab());

        mSession = mIntentDataProvider.getSession();

        boolean drawEdgeToEdge =
                mEdgeToEdgeControllerSupplier.get() != null
                        && mEdgeToEdgeControllerSupplier.get().isDrawingToEdge()
                        && mEdgeToEdgeControllerSupplier.get().isPageOptedIntoEdgeToEdge();
        CustomTabNavigationBarController.update(
                getWindow(), mIntentDataProvider, this, drawEdgeToEdge);
    }

    @Override
    public void performPostInflationStartup() {
        super.performPostInflationStartup();

        FontPreloader.getInstance().onPostInflationStartupCustomTabActivity();

        mRootUiCoordinator.getStatusBarColorController().updateStatusBarColor();

        // Properly attach tab's InfoBarContainer to the view hierarchy if the tab is already
        // attached to a ChromeActivity, as the main tab might have been initialized prior to
        // inflation.
        if (mTabProvider.getTab() != null) {
            ViewGroup bottomContainer = findViewById(R.id.bottom_container);
            InfoBarContainer.get(mTabProvider.getTab()).setParentView(bottomContainer);
        }

        // Setting task title and icon to be null will preserve the client app's title and icon.
        setTaskDescription(
                new ActivityManager.TaskDescription(
                        null, null, mIntentDataProvider.getColorProvider().getToolbarColor()));

        GoogleBottomBarCoordinator googleBottomBarCoordinator =
                mBaseCustomTabRootUiCoordinator.getGoogleBottomBarCoordinator();

        if (googleBottomBarCoordinator != null) {
            View googleBottomBarView = googleBottomBarCoordinator.createGoogleBottomBarView();
            CustomTabBottomBarDelegate delegate = getComponent().resolveBottomBarDelegate();
            delegate.setBottomBarHeight(googleBottomBarCoordinator.getBottomBarHeightInPx());
            delegate.setKeepContentView(true);
            delegate.setBottomBarContentView(googleBottomBarView);
            delegate.setCustomButtonsUpdater(googleBottomBarCoordinator::updateBottomBarButton);
        }

        getComponent().resolveBottomBarDelegate().showBottomBarIfNecessary();
    }

    @Override
    protected void onFirstDrawComplete() {
        super.onFirstDrawComplete();

        FontPreloader.getInstance().onFirstDrawCustomTabActivity();
    }

    @Override
    public void finishNativeInitialization() {
        if (!mIntentDataProvider.isInfoPage()) {
            FirstRunSignInProcessor.openSyncSettingsIfScheduled(this);
            BackupSigninProcessor.start(this);
        }

        mConnection.showSignInToastIfNecessary(mSession, getIntent(), getProfileProviderSupplier());

        new CustomTabTrustedCdnPublisherUrlVisibility(
                getWindowAndroid(),
                getLifecycleDispatcher(),
                () -> {
                    if (ChromeFeatureList.isEnabled(
                            ChromeFeatureList.CCT_EXTEND_TRUSTED_CDN_PUBLISHER)) {
                        return mConnection.isTrustedCdnPublisherUrlPackage(
                                mIntentDataProvider.getClientPackageName());
                    }
                    String urlPackage = mConnection.getTrustedCdnPublisherUrlPackage();
                    return urlPackage != null
                            && urlPackage.equals(
                                    mConnection.getClientPackageNameForSession(mSession));
                });
        super.finishNativeInitialization();
        if (SigninFeatureMap.isEnabled(SigninFeatures.CCT_SIGN_IN_PROMPT)) {
            mConnection.maybeShowAccountMismatchNotification(
                    mSession, getIntent(), getWindowAndroid(), getProfileProviderSupplier());
        }
    }

    @Override
    protected void handleFinishAndClose(boolean warmupOnFinish) {
        if (mOpenTimeRecorder != null) mOpenTimeRecorder.updateCloseCause();
        super.handleFinishAndClose(warmupOnFinish);
    }

    @Override
    protected void onUserLeaveHint() {
        if (mOpenTimeRecorder != null) mOpenTimeRecorder.onUserLeaveHint();
        super.onUserLeaveHint();
    }

    private void resetPostMessageHandlersForCurrentSession() {
        Tab tab = mTabProvider.getTab();
        WebContents webContents = tab == null ? null : tab.getWebContents();
        mConnection.resetPostMessageHandlerForSession(
                mIntentDataProvider.getSession(), webContents);
    }

    @Override
    public String getPackageName() {
        if (mShouldOverridePackage
                && mIntentDataProvider instanceof CustomTabIntentDataProvider intentDataProvider) {
            return intentDataProvider.getInsecureClientPackageNameForOnFinishAnimation();
        }
        return super.getPackageName();
    }

    @Override
    public boolean onOptionsItemSelected(int itemId, @Nullable Bundle menuItemData) {
        int menuIndex =
                CustomTabAppMenuPropertiesDelegate.getIndexOfMenuItemFromBundle(menuItemData);
        if (menuIndex >= 0) {
            ((CustomTabIntentDataProvider) mIntentDataProvider)
                    .clickMenuItemWithUrlAndTitle(
                            this,
                            menuIndex,
                            getActivityTab().getUrl().getSpec(),
                            getActivityTab().getTitle());
            RecordUserAction.record("CustomTabsMenuCustomMenuItem");
            return true;
        }

        return super.onOptionsItemSelected(itemId, menuItemData);
    }

    @Override
    public boolean onMenuOrKeyboardAction(int id, boolean fromMenu) {
        if (id == R.id.bookmark_this_page_id) {
            mTabBookmarkerSupplier.get().addOrEditBookmark(getActivityTab());
            RecordUserAction.record("MobileMenuAddToBookmarks");
            return true;
        } else if (id == R.id.open_in_browser_id) {
            // Need to get tab before calling openCurrentUrlInBrowser or else it will be null.
            Tab tab = mTabProvider.getTab();
            if (mNavigationController.openCurrentUrlInBrowser()) {
                RecordUserAction.record("CustomTabsMenuOpenInChrome");
                WebContents webContents = tab == null ? null : tab.getWebContents();
                mConnection.notifyOpenInBrowser(mSession, webContents);
            }
            return true;
        } else if (id == R.id.info_menu_id) {
            Tab tab = getTabModelSelector().getCurrentTab();
            if (tab == null) return false;
            String publisher = TrustedCdn.getContentPublisher(tab);
            new ChromePageInfo(
                            getModalDialogManagerSupplier(),
                            publisher,
                            OpenedFromSource.MENU,
                            mRootUiCoordinator.getMerchantTrustSignalsCoordinatorSupplier()::get,
                            mRootUiCoordinator.getEphemeralTabCoordinatorSupplier(),
                            getTabCreator(getCurrentTabModel().isIncognito()))
                    .show(tab, ChromePageInfoHighlight.noHighlight());
            return true;
        } else if (id == R.id.open_history_menu_id) {
            // The menu is visible only when the app-specific history is enabled. Assert that.
            assert HistoryManager.isAppSpecificHistoryEnabled();
            HistoryManagerUtils.showAppSpecificHistoryManager(
                    this,
                    getTabModelSelector().isIncognitoSelected(),
                    mIntentDataProvider.getClientPackageNameIdentitySharing());

            CustomTabHistoryIPHController historyIPH =
                    mBaseCustomTabRootUiCoordinator.getHistoryIPHController();
            if (historyIPH != null) {
                historyIPH.notifyUserEngaged();
            }
            return true;
        }
        return super.onMenuOrKeyboardAction(id, fromMenu);
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
        if (sPreventTouches && shouldPreventTouch(ev)) {
            // Discard the events which may be trickling down from an overlay activity above.
            return true;
        }
        return super.dispatchTouchEvent(ev);
    }

    @Override
    public void finish() {
        RecordHistogram.recordLinearCountHistogram(
                "CustomTabs.Omnibox.NumNavigationsPerSession",
                mNumOmniboxNavigationEventsPerSession,
                1,
                10,
                10);

        super.finish();
    }

    @VisibleForTesting(otherwise = PRIVATE)
    boolean shouldPreventTouch(MotionEvent ev) {
        if (ApplicationStatus.getStateForActivity(this) == ActivityState.RESUMED) return false;
        mBlockedEvent = ev;
        return true;
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        // No need to do the following from Q and onward where multi-resume state is supported
        // in split screen mode.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) return;

        if (hasFocus
                && mBlockedEvent != null
                && MultiWindowUtils.getInstance().isInMultiWindowMode(this)) {
            mBlockedEvent.setAction(MotionEvent.ACTION_DOWN);
            super.dispatchTouchEvent(mBlockedEvent); // Inject the blocked event
            mBlockedEvent = null;
        }
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
        // mIntentDataProvider is not ready when the super calls this method. So, we skip setting
        // the task description here, and do it in #performPostInflationStartup();
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if (resultCode != Activity.RESULT_OK) return;

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SEARCH_IN_CCT)
                && SearchActivityClientImpl.isOmniboxResult(requestCode, data)) {
            LoadUrlParams params =
                    SearchActivityClientImpl.getOmniboxResult(requestCode, resultCode, data);

            RecordHistogram.recordBooleanHistogram(
                    "CustomTabs.Omnibox.FocusResultedInNavigation", params != null);

            if (params == null) return;

            mNumOmniboxNavigationEventsPerSession++;
            // Yield to give the called activity time to close.
            // Loading URL directly will result in Activity closing after URL loading completes.
            PostTask.postTask(TaskTraits.UI_DEFAULT, () -> mTabProvider.getTab().loadUrl(params));
        }

        if (HistoryManager.isAppSpecificHistoryEnabled()
                && requestCode == HistoryManagerUtils.HISTORY_REQUEST_CODE) {
            LoadUrlParams params =
                    new LoadUrlParams(
                            data.getData().toString(),
                            IntentHandler.getTransitionTypeFromIntent(data, PageTransition.LINK));
            mTabProvider.getTab().loadUrl(params);
        }
    }
}
