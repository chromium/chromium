// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.feed;

import android.app.Activity;
import android.content.Intent;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.app.creator.CreatorActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.device_lock.DeviceLockActivityLauncherImpl;
import org.chromium.chrome.browser.feed.FeedActionDelegate;
import org.chromium.chrome.browser.feed.R;
import org.chromium.chrome.browser.feed.SingleWebFeedEntryPoint;
import org.chromium.chrome.browser.feed.signinbottomsheet.SigninBottomSheetCoordinator;
import org.chromium.chrome.browser.feed.webfeed.CreatorIntentConstants;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.RequestCoordinatorBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.signin.SyncConsentActivityLauncherImpl;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.Referrer;
import org.chromium.net.NetError;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;

/** Implements some actions for the Feed */
public class FeedActionDelegateImpl implements FeedActionDelegate {
    private static final String NEW_TAB_URL_HELP = "https://support.google.com/chrome/?p=new_tab";
    private final NativePageNavigationDelegate mNavigationDelegate;
    private final BookmarkModel mBookmarkModel;
    private final Activity mActivity;
    private final SnackbarManager mSnackbarManager;
    private final TabModelSelector mTabModelSelector;
    private final Profile mProfile;
    private final BottomSheetController mBottomSheetController;

    public FeedActionDelegateImpl(
            Activity activity,
            SnackbarManager snackbarManager,
            NativePageNavigationDelegate navigationDelegate,
            BookmarkModel bookmarkModel,
            TabModelSelector tabModelSelector,
            Profile profile,
            BottomSheetController bottomSheetController) {
        mActivity = activity;
        mNavigationDelegate = navigationDelegate;
        mBookmarkModel = bookmarkModel;
        mSnackbarManager = snackbarManager;
        mTabModelSelector = tabModelSelector;
        mProfile = profile;
        mBottomSheetController = bottomSheetController;
    }

    @Override
    public void downloadPage(String url) {
        RequestCoordinatorBridge.getForProfile(mProfile)
                .savePageLater(
                        url, OfflinePageBridge.NTP_SUGGESTIONS_NAMESPACE, true /* user requested*/);
    }

    @Override
    public void openSuggestionUrl(
            int disposition,
            LoadUrlParams params,
            boolean inGroup,
            int pageId,
            PageLoadObserver pageLoadObserver,
            Callback<VisitResult> onVisitComplete) {
        params.setReferrer(
                new Referrer(
                        SuggestionsConfig.getReferrerUrl(),
                        // WARNING: ReferrerPolicy.ALWAYS is assumed by other Chrome code for NTP
                        // tiles to set consider_for_ntp_most_visited.
                        org.chromium.network.mojom.ReferrerPolicy.ALWAYS));

        Tab tab =
                inGroup
                        ? mNavigationDelegate.openUrlInGroup(disposition, params)
                        : mNavigationDelegate.openUrl(disposition, params);

        NewTabPageUma.recordAction(NewTabPageUma.ACTION_OPENED_SNIPPET);

        boolean inNewTab =
                (disposition == WindowOpenDisposition.NEW_BACKGROUND_TAB
                        || disposition == WindowOpenDisposition.OFF_THE_RECORD);

        if (tab != null) {
            tab.addObserver(new FeedTabNavigationObserver(inNewTab, pageId, pageLoadObserver));
            NavigationRecorder.record(
                    tab,
                    navigationResult -> {
                        FeedActionDelegate.VisitResult result =
                                new FeedActionDelegate.VisitResult();
                        result.visitTimeMs = navigationResult.duration;
                        onVisitComplete.onResult(result);
                    });
        }

        BrowserUiUtils.recordModuleClickHistogram(ModuleTypeOnStartAndNtp.FEED);
    }

    @Override
    public void openUrl(int disposition, LoadUrlParams params) {
        mNavigationDelegate.openUrl(disposition, params);
    }

    @Override
    public void openHelpPage() {
        mNavigationDelegate.openUrl(
                WindowOpenDisposition.CURRENT_TAB,
                new LoadUrlParams(NEW_TAB_URL_HELP, PageTransition.AUTO_BOOKMARK));

        BrowserUiUtils.recordModuleClickHistogram(ModuleTypeOnStartAndNtp.FEED);
    }

    @Override
    public void addToReadingList(String title, String url) {
        mBookmarkModel.finishLoadingBookmarkModel(
                () -> {
                    assert ThreadUtils.runningOnUiThread();
                    BookmarkUtils.addToReadingList(
                            mActivity,
                            mBookmarkModel,
                            title,
                            new GURL(url),
                            mSnackbarManager,
                            mProfile,
                            mBottomSheetController);
                });
    }

    @Override
    public void openWebFeed(String webFeedName, @SingleWebFeedEntryPoint int entryPoint) {
        if (!WebFeedBridge.isCormorantEnabledForLocale()) {
            return;
        }

        assert ThreadUtils.runningOnUiThread();
        Class<?> creatorActivityClass = CreatorActivity.class;
        Intent intent = new Intent(mActivity, creatorActivityClass);
        intent.putExtra(CreatorIntentConstants.CREATOR_WEB_FEED_ID, webFeedName.getBytes());
        intent.putExtra(CreatorIntentConstants.CREATOR_ENTRY_POINT, entryPoint);
        intent.putExtra(CreatorIntentConstants.CREATOR_TAB_ID, mTabModelSelector.getCurrentTabId());
        mActivity.startActivity(intent);
    }

    @Override
    public void showSyncConsentActivity(@SigninAccessPoint int signinAccessPoint) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_SHOW_SIGN_IN_COMMAND)) {
            SyncConsentActivityLauncherImpl.get()
                    .launchActivityIfAllowed(mActivity, signinAccessPoint);
        }
    }

    @Override
    public void startSigninFlow(@SigninAccessPoint int signinAccessPoint) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_SHOW_SIGN_IN_COMMAND)) {
            return;
        }
        AccountPickerBottomSheetStrings bottomSheetStrings =
                new AccountPickerBottomSheetStrings.Builder(
                                R.string.signin_account_picker_bottom_sheet_title)
                        .build();
        SigninAndHistorySyncActivityLauncherImpl.get()
                .launchActivityIfAllowed(
                        mActivity,
                        mProfile,
                        bottomSheetStrings,
                        SigninAndHistorySyncCoordinator.NoAccountSigninMode.BOTTOM_SHEET,
                        SigninAndHistorySyncCoordinator.WithAccountSigninMode
                                .DEFAULT_ACCOUNT_BOTTOM_SHEET,
                        SigninAndHistorySyncCoordinator.HistoryOptInMode.NONE,
                        signinAccessPoint);
    }

    @Override
    public void showSignInInterstitial(
            @SigninAccessPoint int signinAccessPoint,
            BottomSheetController bottomSheetController,
            WindowAndroid windowAndroid) {
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            AccountPickerBottomSheetStrings bottomSheetStrings =
                    new AccountPickerBottomSheetStrings.Builder(
                                    R.string
                                            .signin_account_picker_bottom_sheet_title_for_back_of_card_menu_signin)
                            .setSubtitleStringId(
                                    R.string
                                            .signin_account_picker_bottom_sheet_subtitle_for_back_of_card_menu_signin)
                            .setDismissButtonStringId(R.string.cancel)
                            .build();
            SigninAndHistorySyncActivityLauncherImpl.get()
                    .launchActivityIfAllowed(
                            mActivity,
                            mProfile,
                            bottomSheetStrings,
                            SigninAndHistorySyncCoordinator.NoAccountSigninMode.BOTTOM_SHEET,
                            SigninAndHistorySyncCoordinator.WithAccountSigninMode
                                    .DEFAULT_ACCOUNT_BOTTOM_SHEET,
                            SigninAndHistorySyncCoordinator.HistoryOptInMode.NONE,
                            signinAccessPoint);
            return;
        }
        AccountPickerBottomSheetStrings bottomSheetStrings =
                new AccountPickerBottomSheetStrings.Builder(
                                R.string
                                        .signin_account_picker_bottom_sheet_title_for_back_of_card_menu_signin)
                        .setSubtitleStringId(
                                R.string
                                        .signin_account_picker_bottom_sheet_subtitle_for_back_of_card_menu_signin_old)
                        .setDismissButtonStringId(R.string.close)
                        .build();
        SigninMetricsUtils.logSigninStarted(signinAccessPoint);
        SigninMetricsUtils.logSigninUserActionForAccessPoint(signinAccessPoint);
        SigninBottomSheetCoordinator signinCoordinator =
                new SigninBottomSheetCoordinator(
                        windowAndroid,
                        DeviceLockActivityLauncherImpl.get(),
                        bottomSheetController,
                        mProfile,
                        bottomSheetStrings,
                        null,
                        signinAccessPoint);
        signinCoordinator.show();
    }

    /**
     * A {@link TabObserver} that observes navigation related events that originate from Feed
     * interactions. Calls reportPageLoaded when navigation completes.
     */
    private static class FeedTabNavigationObserver extends EmptyTabObserver {
        private final boolean mInNewTab;
        private final int mPageId;
        private final PageLoadObserver mPageLoadObserver;

        FeedTabNavigationObserver(boolean inNewTab, int pageId, PageLoadObserver pageLoadObserver) {
            mInNewTab = inNewTab;
            mPageId = pageId;
            mPageLoadObserver = pageLoadObserver;
        }

        @Override
        public void onPageLoadStarted(Tab tab, GURL url) {
            mPageLoadObserver.onPageLoadStarted(mPageId);
        }

        @Override
        public void onPageLoadFinished(Tab tab, GURL url) {
            // TODO(jianli): onPageLoadFinished is called on successful load, and if a user manually
            // stops the page load. We should only capture successful page loads.
            mPageLoadObserver.onPageLoadFinished(mPageId, mInNewTab);
            tab.removeObserver(this);
        }

        @Override
        public void onPageLoadFailed(Tab tab, @NetError int errorCode) {
            mPageLoadObserver.onPageLoadFailed(mPageId, errorCode);
            tab.removeObserver(this);
        }

        @Override
        public void onCrash(Tab tab) {
            tab.removeObserver(this);
        }

        @Override
        public void onDestroyed(Tab tab) {
            tab.removeObserver(this);
        }

        @Override
        public void didFirstVisuallyNonEmptyPaint(Tab tab) {
            mPageLoadObserver.onPageFirstContentfulPaint(mPageId);
        }
    }
}
