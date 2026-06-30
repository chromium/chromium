// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.feed;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Intent;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerOpenerImpl;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.feed.FeedActionDelegate;
import org.chromium.chrome.browser.feed.R;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.offlinepages.RequestCoordinatorBridge;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManagerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.device_lock.DeviceLockActivityLauncher;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.Referrer;
import org.chromium.net.NetError;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.Map;

/** Implements some actions for the Feed */
@NullMarked
public class FeedActionDelegateImpl
        implements FeedActionDelegate, BottomSheetSigninAndHistorySyncCoordinator.Delegate {
    private static final String NEW_TAB_URL_HELP = "https://support.google.com/chrome/?p=new_tab";
    private final NativePageNavigationDelegate mNavigationDelegate;
    private final BookmarkModel mBookmarkModel;
    private final Activity mActivity;
    private final SnackbarManager mSnackbarManager;
    private final Profile mProfile;
    private final BottomSheetController mBottomSheetController;

    private static final @SigninAccessPoint int[] FEED_ACTION_ACCESS_POINTS =
            new int[] {
                SigninAccessPoint.NTP_FEED_BOTTOM_PROMO, SigninAccessPoint.NTP_FEED_CARD_MENU_PROMO
            };
    private final Map<@SigninAccessPoint Integer, BottomSheetSigninAndHistorySyncCoordinator>
            mSigninCoordinators = new HashMap<>();

    public FeedActionDelegateImpl(
            Activity activity,
            WindowAndroid windowAndroid,
            ActivityResultTracker activityResultTracker,
            SigninAndHistorySyncActivityLauncher signinLauncher,
            DeviceLockActivityLauncher deviceLockActivityLauncher,
            SnackbarManager snackbarManager,
            ModalDialogManager modalDialogManager,
            NativePageNavigationDelegate navigationDelegate,
            BookmarkModel bookmarkModel,
            Profile profile,
            BottomSheetController bottomSheetController) {
        mActivity = activity;
        mNavigationDelegate = navigationDelegate;
        mBookmarkModel = bookmarkModel;
        mSnackbarManager = snackbarManager;
        mProfile = profile;
        mBottomSheetController = bottomSheetController;

        if (SigninFeatureMap.getInstance().isActivitylessSigninAllEntryPointEnabled()) {
            for (@SigninAccessPoint int accessPoint : FEED_ACTION_ACCESS_POINTS) {
                OneshotSupplierImpl<Profile> profileSupplier = new OneshotSupplierImpl<>();
                profileSupplier.set(profile);
                mSigninCoordinators.put(
                        accessPoint,
                        signinLauncher.createBottomSheetSigninCoordinatorAndObserveAddAccountResult(
                                windowAndroid,
                                activity,
                                activityResultTracker,
                                this,
                                deviceLockActivityLauncher,
                                profileSupplier,
                                () -> bottomSheetController,
                                modalDialogManager,
                                snackbarManager,
                                accessPoint));
            }
        }
    }

    @Override
    public void destroy() {
        for (BottomSheetSigninAndHistorySyncCoordinator coordinator :
                mSigninCoordinators.values()) {
            coordinator.destroy();
        }
        mSigninCoordinators.clear();
    }

    @Override
    public void downloadPage(String url) {
        RequestCoordinatorBridge requestCoordinatorBridge =
                RequestCoordinatorBridge.getForProfile(mProfile);
        assert requestCoordinatorBridge != null;
        requestCoordinatorBridge.savePageLater(
                url, OfflinePageBridge.NTP_SUGGESTIONS_NAMESPACE, true /* user requested*/);
    }

    @Override
    public void openSuggestionUrl(
            int disposition,
            LoadUrlParams params,
            boolean inGroup,
            int pageId,
            PageLoadObserver pageLoadObserver,
            int surfaceId) {
        params.setReferrer(
                new Referrer(
                        SuggestionsConfig.getReferrerUrl(),
                        // WARNING: ReferrerPolicy.ALWAYS is assumed by other Chrome code for NTP
                        // tiles to set consider_for_ntp_most_visited.
                        ReferrerPolicy.ALWAYS));

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
            Profile profile = mProfile;
            NavigationRecorder.record(tab, profile, surfaceId);
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
                            mBottomSheetController,
                            new BookmarkManagerOpenerImpl(),
                            PriceDropNotificationManagerFactory.create(mProfile));
                });
    }

    @Override
    public void startSigninFlow(@SigninAccessPoint int signinAccessPoint) {
        AccountPickerBottomSheetStrings bottomSheetStrings =
                new AccountPickerBottomSheetStrings.Builder(
                                mActivity.getString(
                                        R.string.signin_account_picker_bottom_sheet_title))
                        .build();
        BottomSheetSigninAndHistorySyncConfig config =
                new BottomSheetSigninAndHistorySyncConfig.Builder(
                                bottomSheetStrings,
                                NoAccountSigninMode.BOTTOM_SHEET,
                                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                HistorySyncConfig.OptInMode.NONE,
                                mActivity.getString(R.string.history_sync_title),
                                mActivity.getString(R.string.history_sync_subtitle))
                        .build();

        if (SigninFeatureMap.getInstance().isActivitylessSigninAllEntryPointEnabled()) {
            BottomSheetSigninAndHistorySyncCoordinator coordinator =
                    mSigninCoordinators.get(signinAccessPoint);
            assert coordinator != null : "Unexpected SigninAccessPoint: " + signinAccessPoint;
            coordinator.startSigninFlow(config);
        } else {
            @Nullable Intent intent =
                    SigninAndHistorySyncActivityLauncherImpl.get()
                            .createBottomSheetSigninIntentOrShowError(
                                    mActivity, mProfile, config, signinAccessPoint);
            if (intent != null) {
                mActivity.startActivity(intent);
            }
        }
    }

    @Override
    public void showSignInInterstitial(
            @SigninAccessPoint int signinAccessPoint, BottomSheetController bottomSheetController) {
        AccountPickerBottomSheetStrings bottomSheetStrings =
                new AccountPickerBottomSheetStrings.Builder(
                                mActivity.getString(
                                        R.string
                                                .signin_account_picker_bottom_sheet_title_for_back_of_card_menu_signin))
                        .setSubtitleString(
                                mActivity.getString(
                                        R.string
                                                .signin_account_picker_bottom_sheet_subtitle_for_back_of_card_menu_signin))
                        .setDismissButtonString(mActivity.getString(R.string.cancel))
                        .build();
        BottomSheetSigninAndHistorySyncConfig config =
                new BottomSheetSigninAndHistorySyncConfig.Builder(
                                bottomSheetStrings,
                                NoAccountSigninMode.BOTTOM_SHEET,
                                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                HistorySyncConfig.OptInMode.NONE,
                                mActivity.getString(R.string.history_sync_title),
                                mActivity.getString(R.string.history_sync_subtitle))
                        .build();

        if (SigninFeatureMap.getInstance().isActivitylessSigninAllEntryPointEnabled()) {
            BottomSheetSigninAndHistorySyncCoordinator coordinator =
                    mSigninCoordinators.get(signinAccessPoint);
            assert coordinator != null : "Unexpected SigninAccessPoint: " + signinAccessPoint;
            coordinator.startSigninFlow(config);
        } else {
            @Nullable Intent intent =
                    SigninAndHistorySyncActivityLauncherImpl.get()
                            .createBottomSheetSigninIntentOrShowError(
                                    mActivity, mProfile, config, signinAccessPoint);
            if (intent != null) {
                mActivity.startActivity(intent);
            }
        }
    }

    /**
     * A {@link TabObserver} that observes navigation related events that originate from Feed
     * interactions. Calls reportPageLoaded when navigation completes.
     */
    private static class FeedTabNavigationObserver extends EmptyTabObserver {
        private final boolean mInNewTab;
        private final int mPageId;
        private @Nullable PageLoadObserver mPageLoadObserver;

        FeedTabNavigationObserver(boolean inNewTab, int pageId, PageLoadObserver pageLoadObserver) {
            mInNewTab = inNewTab;
            mPageId = pageId;
            mPageLoadObserver = pageLoadObserver;

            assertNonNull(mPageLoadObserver);
        }

        @Override
        public void onPageLoadStarted(Tab tab, GURL url) {
            assumeNonNull(mPageLoadObserver).onPageLoadStarted(mPageId);
        }

        @Override
        public void onPageLoadFinished(Tab tab, GURL url) {
            // TODO(jianli): onPageLoadFinished is called on successful load, and if a user manually
            // stops the page load. We should only capture successful page loads.
            assumeNonNull(mPageLoadObserver).onPageLoadFinished(mPageId, mInNewTab);
            destroy(tab);
        }

        @Override
        public void onPageLoadFailed(Tab tab, @NetError int errorCode) {
            assumeNonNull(mPageLoadObserver).onPageLoadFailed(mPageId, errorCode);
            destroy(tab);
        }

        @Override
        public void onCrash(Tab tab) {
            destroy(tab);
        }

        @Override
        public void onDestroyed(Tab tab) {
            destroy(tab);
        }

        @Override
        public void didFirstVisuallyNonEmptyPaint(Tab tab) {
            assumeNonNull(mPageLoadObserver).onPageFirstContentfulPaint(mPageId);
        }

        private void destroy(Tab tab) {
            if (mPageLoadObserver == null) return;

            tab.removeObserver(this);
            mPageLoadObserver = null;
        }
    }
}
