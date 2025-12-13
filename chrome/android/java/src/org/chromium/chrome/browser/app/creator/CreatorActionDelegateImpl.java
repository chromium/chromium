// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.creator;

import android.app.Activity;
import android.content.Intent;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerOpenerImpl;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.creator.CreatorCoordinator;
import org.chromium.chrome.browser.feed.FeedActionDelegate;
import org.chromium.chrome.browser.feed.R;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManagerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.AsyncTabCreationParams;
import org.chromium.chrome.browser.tabmodel.document.ChromeAsyncTabLauncher;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;

/** Implements some actions for the Feed */
@NullMarked
public class CreatorActionDelegateImpl implements FeedActionDelegate {
    private static final String TAG = "Cormorant";

    private final Activity mActivity;
    private final Profile mProfile;
    private final SnackbarManager mSnackbarManager;
    private final CreatorCoordinator mCreatorCoordinator;
    private final int mParentId;
    private final BottomSheetController mBottomSheetController;

    public CreatorActionDelegateImpl(
            Activity activity,
            Profile profile,
            SnackbarManager snackbarManager,
            CreatorCoordinator creatorCoordinator,
            int parentId,
            BottomSheetController bottomSheetController) {
        mActivity = activity;
        mProfile = profile;
        mSnackbarManager = snackbarManager;
        mCreatorCoordinator = creatorCoordinator;
        mParentId = parentId;
        mBottomSheetController = bottomSheetController;
    }

    @Override
    public void openSuggestionUrl(
            int disposition,
            LoadUrlParams params,
            boolean inGroup,
            int pageId,
            PageLoadObserver pageLoadObserver,
            Callback<VisitResult> onVisitComplete) {
        // Back-of-card actions
        if (disposition == WindowOpenDisposition.NEW_FOREGROUND_TAB
                || disposition == WindowOpenDisposition.NEW_BACKGROUND_TAB
                || disposition == WindowOpenDisposition.OFF_THE_RECORD) {
            boolean offTheRecord = (disposition == WindowOpenDisposition.OFF_THE_RECORD);
            if (inGroup) {
                AsyncTabCreationParams asyncParams = new AsyncTabCreationParams(params);
                new ChromeAsyncTabLauncher(offTheRecord)
                        .launchNewTab(asyncParams, TabLaunchType.FROM_LINK, mParentId);

            } else {
                new ChromeAsyncTabLauncher(offTheRecord)
                        .launchNewTab(params, TabLaunchType.FROM_LINK, null);
            }
            return;
        } else if (disposition == WindowOpenDisposition.CURRENT_TAB) {
            mCreatorCoordinator.requestOpenSheet(new GURL(params.getUrl()));
            return;
        }
        // TODO(crbug.com/40882120) open in ephemeral tab or thin web view.
        Log.w(TAG, "OpenSuggestionUrl: Unhandled disposition " + disposition);
    }

    @Override
    public void addToReadingList(String title, String url) {
        // TODO(crbug.com/40883240) Eliminate code duplication with
        //     FeedActionDelegateImpl
        BookmarkModel bookmarkModel = BookmarkModel.getForProfile(mProfile);
        bookmarkModel.finishLoadingBookmarkModel(
                () -> {
                    assert ThreadUtils.runningOnUiThread();
                    BookmarkUtils.addToReadingList(
                            mActivity,
                            bookmarkModel,
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
        AccountPickerBottomSheetStrings strings =
                new AccountPickerBottomSheetStrings.Builder(
                                mActivity.getString(
                                        R.string.signin_account_picker_bottom_sheet_title))
                        .build();

        BottomSheetSigninAndHistorySyncConfig config =
                new BottomSheetSigninAndHistorySyncConfig.Builder(
                                strings,
                                NoAccountSigninMode.BOTTOM_SHEET,
                                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                HistorySyncConfig.OptInMode.NONE,
                                mActivity.getString(R.string.history_sync_title),
                                mActivity.getString(R.string.history_sync_subtitle))
                        .build();

        Intent intent =
                SigninAndHistorySyncActivityLauncherImpl.get()
                        .createBottomSheetSigninIntentOrShowError(
                                mActivity, mProfile, config, signinAccessPoint);
        if (intent != null) {
            mActivity.startActivity(intent);
        }
    }

    @Override
    public void showSignInInterstitial(
            @SigninAccessPoint int signinAccessPoint,
            @Nullable BottomSheetController mBottomSheetController) {
        AccountPickerBottomSheetStrings strings =
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
                                strings,
                                NoAccountSigninMode.BOTTOM_SHEET,
                                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET,
                                HistorySyncConfig.OptInMode.NONE,
                                mActivity.getString(R.string.history_sync_title),
                                mActivity.getString(R.string.history_sync_subtitle))
                        .build();

        Intent intent =
                SigninAndHistorySyncActivityLauncherImpl.get()
                        .createBottomSheetSigninIntentOrShowError(
                                mActivity, mProfile, config, signinAccessPoint);
        if (intent != null) {
            mActivity.startActivity(intent);
        }
    }
}
