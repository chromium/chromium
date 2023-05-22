// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

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
import android.os.Bundle;
import android.provider.Browser;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.BackupSigninProcessor;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.dependency_injection.BaseCustomTabActivityComponent;
import org.chromium.chrome.browser.customtabs.features.CustomTabNavigationBarController;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityCommonsModule;
import org.chromium.chrome.browser.firstrun.FirstRunSignInProcessor;
import org.chromium.chrome.browser.flags.AllCachedFieldTrialParameters;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fonts.FontPreloader;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.page_info.ChromePageInfo;
import org.chromium.chrome.browser.page_info.ChromePageInfoHighlight;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TrustedCdn;
import org.chromium.components.page_info.PageInfoController.OpenedFromSource;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.util.ColorUtils;

/**
 * The activity for custom tabs. It will be launched on top of a client's task.
 */
public class CustomTabActivity extends BaseCustomTabActivity {
    private static final String TAG = "CustomTabActivity";

    private CustomTabsSessionToken mSession;

    private final CustomTabsConnection mConnection = CustomTabsConnection.getInstance();

    /**
     * Contains all the parameters of the EXPERIMENTS_FOR_AGSA feature.
     */
    public static final AllCachedFieldTrialParameters EXPERIMENTS_FOR_AGSA_PARAMS =
            new AllCachedFieldTrialParameters(ChromeFeatureList.EXPERIMENTS_FOR_AGSA);

    private CustomTabsOpenTimeRecorder mOpenTimeRecorder;

    private CustomTabActivityTabProvider.Observer mTabChangeObserver =
            new CustomTabActivityTabProvider.Observer() {
        @Override
        public void onInitialTabCreated(@NonNull Tab tab, int mode) {
            resetPostMessageHandlersForCurrentSession();
        }

        @Override
        public void onTabSwapped(@NonNull Tab tab) {
            resetPostMessageHandlersForCurrentSession();
        }

        @Override
        public void onAllTabsClosed() {
            resetPostMessageHandlersForCurrentSession();
        }
    };

    @Override
    protected BaseCustomTabActivityComponent createComponent(
            ChromeActivityCommonsModule commonsModule) {
        BaseCustomTabActivityComponent component = super.createComponent(commonsModule);
        mOpenTimeRecorder = new CustomTabsOpenTimeRecorder(getLifecycleDispatcher(),
                mNavigationController, this::isFinishing, mIntentDataProvider);
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
        resetPostMessageHandlersForCurrentSession();

        mSession = mIntentDataProvider.getSession();

        CustomTabNavigationBarController.update(getWindow(), mIntentDataProvider, this);
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
            ViewGroup bottomContainer = (ViewGroup) findViewById(R.id.bottom_container);
            InfoBarContainer.get(mTabProvider.getTab()).setParentView(bottomContainer);
        }

        // Setting task title and icon to be null will preserve the client app's title and icon.
        setTaskDescription(new ActivityManager.TaskDescription(
                null, null, mIntentDataProvider.getColorProvider().getToolbarColor()));

        getComponent().resolveBottomBarDelegate().showBottomBarIfNecessary();
    }

    @Override
    protected void onFirstDrawComplete() {
        super.onFirstDrawComplete();

        FontPreloader.getInstance().onFirstDrawCustomTabActivity();
    }

    @Override
    protected boolean isPageInsightsHubEnabled() {
        // TODO(b/282739536): Add supplemental Web and App activity(sWAA) user setting.
        return ChromeFeatureList.isEnabled(ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB)
                && CustomTabsConnection.getInstance().shouldEnablePageInsightsForIntent(
                        mIntentDataProvider);
    }

    @Override
    public void finishNativeInitialization() {
        if (!mIntentDataProvider.isInfoPage()) {
            FirstRunSignInProcessor.openSyncSettingsIfScheduled(this);
            BackupSigninProcessor.start(this);
        }

        mConnection.showSignInToastIfNecessary(mSession, getIntent());

        new CustomTabTrustedCdnPublisherUrlVisibility(
                getWindowAndroid(), getLifecycleDispatcher(), () -> {
                    String urlPackage = mConnection.getTrustedCdnPublisherUrlPackage();
                    return urlPackage != null
                            && urlPackage.equals(
                                    mConnection.getClientPackageNameForSession(mSession));
                });

        super.finishNativeInitialization();
    }

    @Override
    protected void handleFinishAndClose() {
        mOpenTimeRecorder.updateCloseCause();
        super.handleFinishAndClose();
    }

    @Override
    protected void onUserLeaveHint() {
        mOpenTimeRecorder.onUserLeaveHint();
        super.onUserLeaveHint();
    }

    private void resetPostMessageHandlersForCurrentSession() {
        Tab tab = mTabProvider.getTab();
        WebContents webContents = tab == null ? null : tab.getWebContents();
        mConnection.resetPostMessageHandlerForSession(
                mIntentDataProvider.getSession(), webContents);
    }

    @Override
    public void createContextualSearchTab(String searchUrl) {
        if (getActivityTab() == null) return;
        getActivityTab().loadUrl(new LoadUrlParams(searchUrl));
    }

    @Override
    public String getPackageName() {
        if (mShouldOverridePackage && mIntentDataProvider instanceof CustomTabIntentDataProvider) {
            return ((CustomTabIntentDataProvider) mIntentDataProvider)
                    .getInsecureClientPackageNameForOnFinishAnimation();
        }
        return super.getPackageName();
    }

    @Override
    public boolean onOptionsItemSelected(int itemId, @Nullable Bundle menuItemData) {
        int menuIndex =
                CustomTabAppMenuPropertiesDelegate.getIndexOfMenuItemFromBundle(menuItemData);
        if (menuIndex >= 0) {
            ((CustomTabIntentDataProvider) mIntentDataProvider)
                    .clickMenuItemWithUrlAndTitle(this, menuIndex,
                            getActivityTab().getUrl().getSpec(), getActivityTab().getTitle());
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
            if (mNavigationController.openCurrentUrlInBrowser(false)) {
                RecordUserAction.record("CustomTabsMenuOpenInChrome");
                WebContents webContents = tab == null ? null : tab.getWebContents();
                mConnection.notifyOpenInBrowser(mSession, webContents);
            }
            return true;
        } else if (id == R.id.info_menu_id) {
            Tab tab = getTabModelSelector().getCurrentTab();
            if (tab == null) return false;
            String publisher = TrustedCdn.getContentPublisher(tab);
            new ChromePageInfo(getModalDialogManagerSupplier(), publisher, OpenedFromSource.MENU,
                    mRootUiCoordinator.getMerchantTrustSignalsCoordinatorSupplier()::get,
                    mRootUiCoordinator.getEphemeralTabCoordinatorSupplier())
                    .show(tab, ChromePageInfoHighlight.noHighlight());
            return true;
        } else if (id == R.id.page_insights_id) {
            // TODO(b/282739536): Open PageInsights Hub.
            return true;
        }
        return super.onMenuOrKeyboardAction(id, fromMenu);
    }

    @Override
    protected BrowserServicesIntentDataProvider buildIntentDataProvider(
            Intent intent, @CustomTabsIntent.ColorScheme int colorScheme) {
        if (IncognitoCustomTabIntentDataProvider.isValidIncognitoIntent(intent)) {
            return new IncognitoCustomTabIntentDataProvider(intent, this, colorScheme);
        }
        return new CustomTabIntentDataProvider(intent, this, colorScheme);
    }

    /**
     * Show the web page with CustomTabActivity, without any navigation control.
     * Used in showing the terms of services page or help pages for Chrome.
     * @param context The current activity context.
     * @param url The url of the web page.
     */
    public static void showInfoPage(Context context, String url) {
        // TODO(xingliu): The title text will be the html document title, figure out if we want to
        // use Chrome strings here as EmbedContentViewActivity does.
        CustomTabsIntent customTabIntent =
                new CustomTabsIntent.Builder()
                        .setShowTitle(true)
                        .setColorScheme(ColorUtils.inNightMode(context) ? COLOR_SCHEME_DARK
                                                                        : COLOR_SCHEME_LIGHT)
                        .build();
        customTabIntent.intent.setData(Uri.parse(url));

        Intent intent = LaunchIntentDispatcher.createCustomTabActivityIntent(
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
                && IntentUtils.safeGetIntExtra(intent, CustomTabIntentDataProvider.EXTRA_UI_TYPE,
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

    @VisibleForTesting
    public NightModeStateProvider getNightModeStateProviderForTesting() {
        return super.getNightModeStateProvider();
    }

    @Override
    protected void setDefaultTaskDescription() {
        // mIntentDataProvider is not ready when the super calls this method. So, we skip setting
        // the task description here, and do it in #performPostInflationStartup();
    }
}
