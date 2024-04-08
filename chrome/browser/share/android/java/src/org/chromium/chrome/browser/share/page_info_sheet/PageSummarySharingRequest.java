// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.page_info_sheet;

import android.app.Activity;
import android.content.Context;
import android.os.SystemClock;
import android.text.TextUtils;

import androidx.annotation.NonNull;

import com.google.common.base.Strings;
import com.google.common.collect.ImmutableList;

import org.chromium.base.lifetime.DestroyChecker;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.page_info_sheet.PageInfoBottomSheetCoordinator.Delegate;
import org.chromium.chrome.browser.share.page_info_sheet.PageInfoBottomSheetCoordinator.PageInfoContents;
import org.chromium.chrome.browser.share.page_info_sheet.feedback.FeedbackSheetCoordinator;
import org.chromium.chrome.browser.share.page_info_sheet.feedback.FeedbackSheetCoordinator.FeedbackOption;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

class PageSummarySharingRequest {

    // TODO(salg): Replace with a channel specific type.
    private static final String FEEDBACK_REPORT_TYPE =
            "com.chrome.canary.summarization.USER_INITIATED_FEEDBACK_REPORT";

    private static final String FEATURE_TYPE_KEY = "genai_type";
    private static final String FEATURE_TYPE_VALUE = "summarization";
    private static final String FEEDBACK_TYPE_KEY = "genai_chip";

    @NonNull private final Context mContext;
    @NonNull private final HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;
    @NonNull private final BottomSheetController mBottomSheetController;
    @NonNull private final Tab mTab;
    @NonNull private final ChromeOptionShareCallback mChromeOptionShareCallback;
    @NonNull private final Runnable mDestroyCallback;
    @NonNull private final ObservableSupplierImpl<PageInfoContents> mPageInfoSupplier;
    private final DestroyChecker mDestroyChecker = new DestroyChecker();
    private PageInfoBottomSheetCoordinator mPageSummaryLoadingUiCoordinator;
    private FeedbackSheetCoordinator mFeedbackUiCoordinator;

    public PageSummarySharingRequest(
            @NonNull Context context,
            @NonNull Tab tab,
            @NonNull ChromeOptionShareCallback chromeOptionShareCallback,
            @NonNull HelpAndFeedbackLauncher helpAndFeedbackLauncher,
            @NonNull ObservableSupplierImpl<PageInfoContents> pageInfoSupplier,
            @NonNull Runnable destroyCallback,
            @NonNull BottomSheetController bottomSheetController) {
        mContext = context;
        mTab = tab;
        mChromeOptionShareCallback = chromeOptionShareCallback;
        mHelpAndFeedbackLauncher = helpAndFeedbackLauncher;
        mDestroyCallback = destroyCallback;
        mPageInfoSupplier = pageInfoSupplier;
        mBottomSheetController = bottomSheetController;
    }

    /**
     * Opens a bottom sheet showing the progress of loading the summary from pageInfoSupplier. When
     * loaded successfully this sheet lets users attach the summary to a share sheet and provide
     * feedback.
     */
    public void openPageSummarySheet() {
        mDestroyChecker.checkNotDestroyed();
        if (mPageSummaryLoadingUiCoordinator != null) return;

        mPageSummaryLoadingUiCoordinator =
                new PageInfoBottomSheetCoordinator(
                        mContext,
                        new Delegate() {
                            @Override
                            public void onAccept() {
                                attachSummaryToShareSheet();
                            }

                            @Override
                            public void onCancel() {
                                destroy();
                            }

                            @Override
                            public void onPositiveFeedback() {
                                // TODO(salg): Record a histogram.
                            }

                            @Override
                            public void onNegativeFeedback() {
                                openFeedbackSheet();
                            }

                            @Override
                            public ObservableSupplier<PageInfoContents> getContentSupplier() {
                                return mPageInfoSupplier;
                            }
                        },
                        mBottomSheetController);
        mPageSummaryLoadingUiCoordinator.requestShowContent();
    }

    /**
     * Opens a bottom sheet containing a list of feedback topics, once the user selects one of them
     * we open the system feedback flow.
     */
    private void openFeedbackSheet() {
        destroyLoadingSheet();
        if (mFeedbackUiCoordinator != null) return;

        mFeedbackUiCoordinator =
                new FeedbackSheetCoordinator(
                        mContext,
                        new FeedbackSheetCoordinator.Delegate() {
                            @Override
                            public void onAccepted(String selectedType) {
                                openSystemFeedbackSheet(selectedType);
                                destroy();
                            }

                            @Override
                            public void onCanceled() {
                                destroyFeedbackSheet();
                                openPageSummarySheet();
                            }

                            @Override
                            public List<FeedbackOption> getAvailableOptions() {
                                return getFeedbackTypes();
                            }
                        },
                        mBottomSheetController);

        mFeedbackUiCoordinator.requestShowContent();
    }

    private void openSystemFeedbackSheet(String feedbackTypeValue) {
        Map<String, String> feedbackDataMap = new HashMap<>();
        feedbackDataMap.put(FEATURE_TYPE_KEY, FEATURE_TYPE_VALUE);
        if (!Strings.isNullOrEmpty(feedbackTypeValue)) {
            feedbackDataMap.put(FEEDBACK_TYPE_KEY, feedbackTypeValue);
        }
        mHelpAndFeedbackLauncher.showFeedback(
                (Activity) mContext,
                mTab.getUrl() == null ? null : mTab.getUrl().getSpec(),
                FEEDBACK_REPORT_TYPE,
                feedbackDataMap);
    }

    private void attachSummaryToShareSheet() {
        if (mDestroyChecker.isDestroyed()) return;

        if (!mPageInfoSupplier.hasValue()) {
            assert false : "attachSummaryToShareSheet should be disabled while loading.";
            return;
        }
        assert mTab.getUrl() != null : "Trying to share a tab with no URL";

        var pageInfo = mPageInfoSupplier.get();
        if (!TextUtils.isEmpty(pageInfo.errorMessage)
                || pageInfo.isLoading
                || TextUtils.isEmpty(pageInfo.resultContents)) {
            assert false
                    : "attachSummaryToShareSheet should be disabled while loading or in case of an"
                            + " error.";
            return;
        }

        var chromeShareExtras =
                new ChromeShareExtras.Builder()
                        .setDetailedContentType(DetailedContentType.PAGE_INFO)
                        .build();
        ShareParams shareParams =
                new ShareParams.Builder(
                                mTab.getWindowAndroid(), mTab.getTitle(), mTab.getUrl().getSpec())
                        .setText(pageInfo.resultContents)
                        .build();

        mChromeOptionShareCallback.showShareSheet(
                shareParams, chromeShareExtras, SystemClock.elapsedRealtime());

        destroy();
    }

    @NonNull
    private static ImmutableList<FeedbackOption> getFeedbackTypes() {
        return new ImmutableList.Builder<FeedbackOption>()
                .add(
                        new FeedbackOption(
                                "offensive", R.string.share_with_summary_feedback_label_offensive))
                .add(
                        new FeedbackOption(
                                "inaccurate",
                                R.string.share_with_summary_feedback_label_inaccurate))
                .add(
                        new FeedbackOption(
                                "repetitive",
                                R.string.share_with_summary_feedback_label_repetitive))
                .add(
                        new FeedbackOption(
                                "missing_information",
                                R.string.share_with_summary_feedback_label_missing_info))
                .add(new FeedbackOption("other", R.string.share_with_summary_feedback_label_other))
                .build();
    }

    private void destroyLoadingSheet() {
        if (mPageSummaryLoadingUiCoordinator == null) return;
        mPageSummaryLoadingUiCoordinator.destroy();
        mPageSummaryLoadingUiCoordinator = null;
    }

    private void destroyFeedbackSheet() {
        if (mFeedbackUiCoordinator == null) return;
        mFeedbackUiCoordinator.destroy();
        mFeedbackUiCoordinator = null;
    }

    private void destroy() {
        mDestroyChecker.destroy();
        destroyLoadingSheet();
        destroyFeedbackSheet();
        mDestroyCallback.run();
    }
}
