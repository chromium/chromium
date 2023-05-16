// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.feed;

import android.content.Context;
import android.content.Intent;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.app.creator.CreatorActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.device_lock.DeviceLockActivityLauncherImpl;
import org.chromium.chrome.browser.feed.FeedActionDelegate;
import org.chromium.chrome.browser.feed.SingleWebFeedEntryPoint;
import org.chromium.chrome.browser.feed.signinbottomsheet.SigninBottomSheetCoordinator;
import org.chromium.chrome.browser.feed.webfeed.CreatorIntentConstants;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.RequestCoordinatorBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.SyncConsentActivityLauncherImpl;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.Referrer;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;

/** Implements some actions for the Feed */
public class FeedActionDelegateImpl implements FeedActionDelegate {
    private static final String NEW_TAB_URL_HELP = "https://support.google.com/chrome/?p=new_tab";
    private final NativePageNavigationDelegate mNavigationDelegate;
    private final BookmarkModel mBookmarkModel;
    private final Context mActivityContext;
    private final SnackbarManager mSnackbarManager;
    private final TabModelSelector mTabModelSelector;

    @BrowserUiUtils.HostSurface
    private int mHostSurface;

    public FeedActionDelegateImpl(Context activityContext, SnackbarManager snackbarManager,
            NativePageNavigationDelegate navigationDelegate, BookmarkModel bookmarkModel,
            @BrowserUiUtils.HostSurface int hostSurface, TabModelSelector tabModelSelector) {
        mActivityContext = activityContext;
        mNavigationDelegate = navigationDelegate;
        mBookmarkModel = bookmarkModel;
        mSnackbarManager = snackbarManager;
        mHostSurface = hostSurface;
        mTabModelSelector = tabModelSelector;
    }
    @Override
    public void downloadPage(String url) {
        RequestCoordinatorBridge.getForProfile(Profile.getLastUsedRegularProfile())
                .savePageLater(
                        url, OfflinePageBridge.NTP_SUGGESTIONS_NAMESPACE, true /* user requested*/);
    }

    @Override
    public void openSuggestionUrl(int disposition, LoadUrlParams params, boolean inGroup,
            Runnable onPageLoaded, Callback<VisitResult> onVisitComplete) {
        params.setReferrer(
                new Referrer(SuggestionsConfig.getReferrerUrl(ChromeFeatureList.INTEREST_FEED_V2),
                        // WARNING: ReferrerPolicy.ALWAYS is assumed by other Chrome code for NTP
                        // tiles to set consider_for_ntp_most_visited.
                        org.chromium.network.mojom.ReferrerPolicy.ALWAYS));

        Tab tab = inGroup ? mNavigationDelegate.openUrlInGroup(disposition, params)
                          : mNavigationDelegate.openUrl(disposition, params);

        NewTabPageUma.recordAction(NewTabPageUma.ACTION_OPENED_SNIPPET);

        boolean inNewTab = (disposition == WindowOpenDisposition.NEW_BACKGROUND_TAB
                || disposition == WindowOpenDisposition.OFF_THE_RECORD);

        if (tab != null) {
            tab.addObserver(new FeedTabNavigationObserver(inNewTab, onPageLoaded));
            NavigationRecorder.record(tab, navigationResult -> {
                FeedActionDelegate.VisitResult result = new FeedActionDelegate.VisitResult();
                result.visitTimeMs = navigationResult.duration;
                onVisitComplete.onResult(result);
            });
        }

        BrowserUiUtils.recordModuleClickHistogram(
                mHostSurface, BrowserUiUtils.ModuleTypeOnStartAndNTP.FEED);
    }

    @Override
    public void openUrl(int disposition, LoadUrlParams params) {
        mNavigationDelegate.openUrl(disposition, params);
    }

    @Override
    public void openHelpPage() {
        mNavigationDelegate.openUrl(WindowOpenDisposition.CURRENT_TAB,
                new LoadUrlParams(NEW_TAB_URL_HELP, PageTransition.AUTO_BOOKMARK));

        BrowserUiUtils.recordModuleClickHistogram(
                mHostSurface, BrowserUiUtils.ModuleTypeOnStartAndNTP.FEED);
    }

    @Override
    public void addToReadingList(String title, String url) {
        mBookmarkModel.finishLoadingBookmarkModel(() -> {
            assert ThreadUtils.runningOnUiThread();
            BookmarkUtils.addToReadingList(
                    new GURL(url), title, mSnackbarManager, mBookmarkModel, mActivityContext);
        });
    }

    @Override
    public void openWebFeed(String webFeedName, @SingleWebFeedEntryPoint int entryPoint) {
        if (!FeatureList.isInitialized()
                || !ChromeFeatureList.isEnabled(ChromeFeatureList.CORMORANT)) {
            return;
        }

        assert ThreadUtils.runningOnUiThread();
        Class<?> creatorActivityClass = CreatorActivity.class;
        Intent intent = new Intent(mActivityContext, creatorActivityClass);
        intent.putExtra(CreatorIntentConstants.CREATOR_WEB_FEED_ID, webFeedName.getBytes());
        intent.putExtra(CreatorIntentConstants.CREATOR_ENTRY_POINT, entryPoint);
        intent.putExtra(CreatorIntentConstants.CREATOR_TAB_ID, mTabModelSelector.getCurrentTabId());
        mActivityContext.startActivity(intent);
    }

    @Override
    public void showSyncConsentActivity(@SigninAccessPoint int signinAccessPoint) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_SHOW_SIGN_IN_COMMAND)) {
            SyncConsentActivityLauncherImpl.get().launchActivityIfAllowed(
                    mActivityContext, signinAccessPoint);
        }
    }

    @Override
    public void showSignInInterstitial(@SigninAccessPoint int signinAccessPoint,
            BottomSheetController bottomSheetController, WindowAndroid windowAndroid) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.FEED_BOC_SIGN_IN_INTERSTITIAL)) {
            SigninMetricsUtils.logSigninStartAccessPoint(signinAccessPoint);
            SigninMetricsUtils.logSigninUserActionForAccessPoint(signinAccessPoint);
            SigninBottomSheetCoordinator signinCoordinator = new SigninBottomSheetCoordinator(
                    windowAndroid, DeviceLockActivityLauncherImpl.get(), bottomSheetController,
                    Profile.getLastUsedRegularProfile(), null, null, signinAccessPoint);
            signinCoordinator.show();
        }
    }

    /**
     * A {@link TabObserver} that observes navigation related events that originate from Feed
     * interactions. Calls reportPageLoaded when navigation completes.
     */
    private class FeedTabNavigationObserver extends EmptyTabObserver {
        private final Runnable mCallback;

        FeedTabNavigationObserver(boolean inNewTab, Runnable callback) {
            mCallback = callback;
        }

        @Override
        public void onPageLoadFinished(Tab tab, GURL url) {
            // TODO(jianli): onPageLoadFinished is called on successful load, and if a user manually
            // stops the page load. We should only capture successful page loads.
            mCallback.run();
            tab.removeObserver(this);
        }

        @Override
        public void onPageLoadFailed(Tab tab, int errorCode) {
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
    }
}
