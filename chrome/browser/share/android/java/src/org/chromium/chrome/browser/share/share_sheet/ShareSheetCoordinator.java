// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import android.app.Activity;
import android.content.pm.ResolveInfo;
import android.content.res.Configuration;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextCoordinator;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextCoordinator.LinkGeneration;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextMetricsHelper;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetLinkToggleCoordinator.LinkToggleState;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetLinkToggleMetricsHelper.LinkToggleMetricsDetails;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.modules.image_editor.ImageEditorModuleProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.ActivityStateObserver;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.Set;

/**
 * Coordinator for displaying the share sheet.
 */
// TODO(crbug/1022172): Should be package-protected once modularization is complete.
public class ShareSheetCoordinator implements ActivityStateObserver, ChromeOptionShareCallback,
                                              ConfigurationChangedObserver,
                                              View.OnLayoutChangeListener {
    private final BottomSheetController mBottomSheetController;
    private final Supplier<Tab> mTabProvider;
    private final ShareSheetPropertyModelBuilder mPropertyModelBuilder;
    private final Callback<Tab> mPrintTabCallback;
    private final boolean mIsIncognito;
    private final ImageEditorModuleProvider mImageEditorModuleProvider;
    private final BottomSheetObserver mBottomSheetObserver;
    private final LargeIconBridge mIconBridge;
    private final Tracker mFeatureEngagementTracker;
    private final Profile mProfile;

    private long mShareStartTime;
    private boolean mExcludeFirstParty;
    private boolean mIsMultiWindow;
    private Set<Integer> mContentTypes;
    private Activity mActivity;
    private ActivityLifecycleDispatcher mLifecycleDispatcher;
    private ChromeProvidedSharingOptionsProvider mChromeProvidedSharingOptionsProvider;
    private ShareParams mShareParams;
    private ShareSheetBottomSheetContent mBottomSheet;
    private WindowAndroid mWindowAndroid;
    private ChromeShareExtras mChromeShareExtras;
    private LinkToTextCoordinator mLinkToTextCoordinator;
    private ShareSheetLinkToggleCoordinator mShareSheetLinkToggleCoordinator;
    private ShareSheetUsageRankingHelper mShareSheetUsageRankingHelper;
    private @LinkGeneration int mLinkGenerationStatusForMetrics = LinkGeneration.MAX;
    private LinkToggleMetricsDetails mLinkToggleMetricsDetails =
            new LinkToggleMetricsDetails(LinkToggleState.COUNT, DetailedContentType.NOT_SPECIFIED);

    /**
     * Constructs a new ShareSheetCoordinator.
     *
     * @param controller The {@link BottomSheetController} for the current activity.
     * @param lifecycleDispatcher Dispatcher for activity lifecycle events, e.g. configuration
     * changes.
     * @param tabProvider Supplier for the current activity tab.
     * @param modelBuilder The {@link ShareSheetPropertyModelBuilder} for the share sheet.
     * @param isIncognito Whether the share sheet was opened in incognito mode or not.
     * @param imageEditorModuleProvider Image Editor module entry point if present in the APK.
     * @param profile The current profile of the User.
     */
    // TODO(crbug/1022172): Should be package-protected once modularization is complete.
    public ShareSheetCoordinator(BottomSheetController controller,
            ActivityLifecycleDispatcher lifecycleDispatcher, Supplier<Tab> tabProvider,
            ShareSheetPropertyModelBuilder modelBuilder, Callback<Tab> printTab,
            LargeIconBridge iconBridge, boolean isIncognito,
            ImageEditorModuleProvider imageEditorModuleProvider, Tracker featureEngagementTracker,
            Profile profile) {
        mBottomSheetController = controller;
        mLifecycleDispatcher = lifecycleDispatcher;
        mLifecycleDispatcher.register(this);
        mTabProvider = tabProvider;
        mPropertyModelBuilder = modelBuilder;
        mPrintTabCallback = printTab;
        mIsIncognito = isIncognito;
        mImageEditorModuleProvider = imageEditorModuleProvider;
        mBottomSheetObserver = new EmptyBottomSheetObserver() {
            @Override
            public void onSheetContentChanged(BottomSheetContent bottomSheet) {
                super.onSheetContentChanged(bottomSheet);
                if (mBottomSheet == null) {
                    return;
                }
                if (bottomSheet == mBottomSheet) {
                    mBottomSheet.getContentView().addOnLayoutChangeListener(
                            ShareSheetCoordinator.this::onLayoutChange);
                } else {
                    mBottomSheet.getContentView().removeOnLayoutChangeListener(
                            ShareSheetCoordinator.this::onLayoutChange);
                }
            }

            @Override
            public void onSheetStateChanged(@SheetState int state, @StateChangeReason int reason) {
                if (state == SheetState.HIDDEN) {
                    RecordHistogram.recordEnumeratedHistogram(
                            "Sharing.SharingHubAndroid.CloseReason", reason,
                            StateChangeReason.MAX_VALUE + 1);
                }
            }
        };
        mBottomSheetController.addObserver(mBottomSheetObserver);
        mIconBridge = iconBridge;
        mFeatureEngagementTracker = featureEngagementTracker;
        mProfile = profile;
        mShareSheetUsageRankingHelper = new ShareSheetUsageRankingHelper(mBottomSheetController,
                mBottomSheet, mShareStartTime, mLinkGenerationStatusForMetrics,
                mLinkToggleMetricsDetails, mPropertyModelBuilder, mProfile);
    }

    protected void destroy() {
        if (mShareParams != null) {
            ShareParams.TargetChosenCallback callback = mShareParams.getCallback();
            if (callback != null) {
                callback.onCancel();
            }
        }
        if (mWindowAndroid != null) {
            mWindowAndroid.removeActivityStateObserver(this);
            mWindowAndroid = null;
        }
        if (mLifecycleDispatcher != null) {
            mLifecycleDispatcher.unregister(this);
            mLifecycleDispatcher = null;
        }
        if (mBottomSheetController != null) {
            mBottomSheetController.removeObserver(mBottomSheetObserver);
        }
    }

    // TODO(crbug/1022172): Should be package-protected once modularization is complete.
    @Override
    public void showShareSheet(
            ShareParams params, ChromeShareExtras chromeShareExtras, long shareStartTime) {
        mShareParams = params;
        mChromeShareExtras = chromeShareExtras;
        mActivity = params.getWindow().getActivity().get();
        if (!shouldShowLinkToText(chromeShareExtras)) {
            mShareSheetLinkToggleCoordinator.setShareParamsAndExtras(params, chromeShareExtras);
            mShareParams = mShareSheetLinkToggleCoordinator.getDefaultShareParams();
        }
        if (mActivity == null) return;

        if (mWindowAndroid == null) {
            mWindowAndroid = params.getWindow();
            if (mWindowAndroid != null) {
                mWindowAndroid.addActivityStateObserver(this);
            }
        }

        mBottomSheet = new ShareSheetBottomSheetContent(
                mActivity, mIconBridge, this, params, mFeatureEngagementTracker);

        mShareStartTime = shareStartTime;
        mLinkGenerationStatusForMetrics = mBottomSheet.getLinkGenerationState();

        updateShareSheet(this::finishShowShareSheet);
    }

    /**
     * Updates {@code mShareParams} from the {@link LinkToggleState}.
     * Called when toggling between link/no link options.
     *
     * @param linkToggleMetricsDetails {@link LinkToggleMetricsDetails} to record link toggle
     *         metrics, and contains the {@link LinkToggleState} to update to.
     * @param linkGenerationState {@link LinkGeneration} to record LinkToText metrics.
     */
    void updateShareSheetForLinkToggle(LinkToggleMetricsDetails linkToggleMetricsDetails,
            @LinkGeneration int linkGenerationState) {
        if (mLinkToTextCoordinator == null && mShareSheetLinkToggleCoordinator == null) {
            return;
        }

        mShareParams = mShareSheetLinkToggleCoordinator.getShareParams(
                linkToggleMetricsDetails.mLinkToggleState);
        mBottomSheet.updateShareParams(mShareParams);
        mLinkGenerationStatusForMetrics = linkGenerationState;
        mLinkToggleMetricsDetails = linkToggleMetricsDetails;
        updateShareSheet(null);
    }

    private void updateShareSheet(Runnable onUpdateFinished) {
        mContentTypes =
                ShareSheetPropertyModelBuilder.getContentTypes(mShareParams, mChromeShareExtras);
        List<PropertyModel> firstPartyApps = createFirstPartyPropertyModels(
                mActivity, mShareParams, mChromeShareExtras, mContentTypes);
        createThirdPartyPropertyModels(mActivity, mShareParams, mContentTypes,
                mChromeShareExtras.saveLastUsed(), thirdPartyApps -> {
                    finishUpdateShareSheet(firstPartyApps, thirdPartyApps, onUpdateFinished);
                });
    }

    private void finishUpdateShareSheet(List<PropertyModel> firstPartyApps,
            List<PropertyModel> thirdPartyApps, @Nullable Runnable onUpdateFinished) {
        mBottomSheet.createRecyclerViews(firstPartyApps, thirdPartyApps, mContentTypes,
                mShareParams.getFileContentType(), mChromeShareExtras.getDetailedContentType(),
                mShareSheetLinkToggleCoordinator);
        if (onUpdateFinished != null) {
            onUpdateFinished.run();
        }
    }

    private void finishShowShareSheet() {
        boolean shown = mBottomSheetController.requestShowContent(mBottomSheet, true);
        if (shown) {
            long delta = System.currentTimeMillis() - mShareStartTime;
            RecordHistogram.recordMediumTimesHistogram(
                    "Sharing.SharingHubAndroid.TimeToShowShareSheet", delta);
        }
    }

    /**
     * Displays the initial share sheet. If sharing was triggered for sharing
     * text, a LinkToTextCoordinator will be created which will generate a link
     * to text.
     *
     * @param params The {@link ShareParams} for the current share.
     * @param chromeShareExtras The {@link ChromeShareExtras} for the current share.
     * @param shareStartTime The start time of the current share.
     */
    public void showInitialShareSheet(
            ShareParams params, ChromeShareExtras chromeShareExtras, long shareStartTime) {
        if (shouldShowLinkToText(chromeShareExtras)) {
            if (!chromeShareExtras.isReshareHighlightedText()) {
                LinkToTextMetricsHelper.recordLinkToTextDiagnoseStatus(
                        LinkToTextMetricsHelper.LinkToTextDiagnoseStatus
                                .SHOW_SHARINGHUB_FOR_HIGHLIGHT);
            }
            mLinkToTextCoordinator = new LinkToTextCoordinator(mTabProvider.get(), this,
                    chromeShareExtras, shareStartTime, getUrlToShare(params, chromeShareExtras),
                    params.getText());
        }
        mShareSheetLinkToggleCoordinator = new ShareSheetLinkToggleCoordinator(
                params, chromeShareExtras, mLinkToTextCoordinator);
        if (shouldShowLinkToText(chromeShareExtras)) {
            mLinkToTextCoordinator.shareLinkToText();
        } else {
            showShareSheet(params, chromeShareExtras, shareStartTime);
        }
    }

    // Used by first party features to share with only non-chrome apps.
    @Override
    public void showThirdPartyShareSheet(
            ShareParams params, ChromeShareExtras chromeShareExtras, long shareStartTime) {
        mExcludeFirstParty = true;
        showShareSheet(params, chromeShareExtras, shareStartTime);
    }

    List<PropertyModel> createFirstPartyPropertyModels(Activity activity, ShareParams shareParams,
            ChromeShareExtras chromeShareExtras, Set<Integer> contentTypes) {
        if (mExcludeFirstParty) {
            return new ArrayList<>();
        }
        mChromeProvidedSharingOptionsProvider = new ChromeProvidedSharingOptionsProvider(activity,
                mWindowAndroid, mTabProvider, mBottomSheetController, mBottomSheet, shareParams,
                mPrintTabCallback, mIsIncognito, mShareStartTime, this, mImageEditorModuleProvider,
                mFeatureEngagementTracker, getUrlToShare(shareParams, chromeShareExtras),
                mLinkGenerationStatusForMetrics, mLinkToggleMetricsDetails, mProfile);
        mIsMultiWindow = ApiCompatibilityUtils.isInMultiWindowMode(activity);

        return mChromeProvidedSharingOptionsProvider.getPropertyModels(
                contentTypes, chromeShareExtras.getDetailedContentType(), mIsMultiWindow);
    }

    private boolean shouldShowLinkToText(ChromeShareExtras chromeShareExtras) {
        return chromeShareExtras.getDetailedContentType() == DetailedContentType.HIGHLIGHTED_TEXT
                && mTabProvider != null && mTabProvider.hasValue();
    }

    /**
     * Create third-party property models.
     *
     * <p>
     * This method delivers its result asynchronously through {@code callback},
     * to allow for the upcoming ShareRanking backend, which is asynchronous.
     * The existing backend is synchronous, but this method is an asynchronous
     * wrapper around it so that the design of the rest of this class won't need
     * to change when ShareRanking is hooked up.
     * TODO(https://crbug.com/1217186)
     * </p>
     */
    @VisibleForTesting
    void createThirdPartyPropertyModels(Activity activity, ShareParams params,
            Set<Integer> contentTypes, boolean saveLastUsed,
            Callback<List<PropertyModel>> callback) {
        if (params == null) {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, callback.bind(null));
            return;
        }

        mShareSheetUsageRankingHelper.createThirdPartyPropertyModelsFromUsageRanking(
                activity, params, contentTypes, saveLastUsed, callback);
    }

    class ResolveInfoPackageNameComparator implements Comparator<ResolveInfo> {
        @Override
        public int compare(ResolveInfo a, ResolveInfo b) {
            return a.activityInfo.packageName.compareTo(b.activityInfo.packageName);
        }
    };

    static void recordShareMetrics(String featureName, @LinkGeneration int linkGenerationStatus,
            LinkToggleMetricsDetails linkToggleMetricsDetails, long shareStartTime,
            Profile profile) {
        recordShareMetrics(featureName, linkGenerationStatus, linkToggleMetricsDetails, profile);
        recordTimeToShare(shareStartTime);
    }

    private static void recordSharedHighlightingUsage(Profile profile) {
        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        tracker.notifyEvent(EventConstants.IPH_SHARED_HIGHLIGHTING_USED);
    }

    private static void recordShareMetrics(String featureName,
            @LinkGeneration int linkGenerationStatus,
            LinkToggleMetricsDetails linkToggleMetricsDetails, Profile profile) {
        RecordUserAction.record(featureName);
        LinkToTextMetricsHelper.recordSharedHighlightStateMetrics(linkGenerationStatus);

        if (linkGenerationStatus == LinkGeneration.LINK
                || linkGenerationStatus == LinkGeneration.TEXT) {
            // Record usage for Shared Highlighting promo
            recordSharedHighlightingUsage(profile);
        }

        ShareSheetLinkToggleMetricsHelper.recordLinkToggleSharedStateMetric(
                linkToggleMetricsDetails);
    }

    private static void recordTimeToShare(long shareStartTime) {
        RecordHistogram.recordMediumTimesHistogram("Sharing.SharingHubAndroid.TimeToShare",
                System.currentTimeMillis() - shareStartTime);
    }

    @VisibleForTesting
    protected void disableFirstPartyFeaturesForTesting() {
        mExcludeFirstParty = true;
    }

    /**
     * Returns the url to share.
     *
     * <p>This prioritizes the URL in {@link ShareParams}, but if it does not exist, we look for an
     * image source URL from {@link ChromeShareExtras}. The image source URL is not contained in
     * {@link ShareParams#getUrl()} because we do not want to share the image URL with the image
     * file in third-party app shares. If both are empty then current tab URL is used. This is
     * useful for {@link LinkToTextCoordinator} that needs URL but it cannot be provided through
     * {@link ShareParams}.
     */
    private String getUrlToShare(ShareParams shareParams, ChromeShareExtras chromeShareExtras) {
        if (!TextUtils.isEmpty(shareParams.getUrl())) {
            return shareParams.getUrl();
        } else if (!chromeShareExtras.getImageSrcUrl().isEmpty()) {
            return chromeShareExtras.getImageSrcUrl().getSpec();
        } else if (mTabProvider.hasValue() && mTabProvider.get().isInitialized()) {
            return mTabProvider.get().getUrl().getSpec();
        }
        return "";
    }

    // ActivityStateObserver
    @Override
    public void onActivityResumed() {}

    @Override
    public void onActivityPaused() {
        boolean persistOnPause =
                ChromeFeatureList.isEnabled(ChromeFeatureList.PERSIST_SHARE_HUB_ON_APP_SWITCH);
        if (mBottomSheet != null && !persistOnPause) {
            mBottomSheetController.hideContent(mBottomSheet, true);
        }
    }

    @Override
    public void onActivityDestroyed() {}

    // ConfigurationChangedObserver
    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        if (mActivity == null) {
            return;
        }
        boolean isMultiWindow = ApiCompatibilityUtils.isInMultiWindowMode(mActivity);
        // mContentTypes is null if Chrome features should not be shown.
        if (mIsMultiWindow == isMultiWindow || mContentTypes == null) {
            return;
        }

        mIsMultiWindow = isMultiWindow;
        mBottomSheet.createFirstPartyRecyclerViews(
                mChromeProvidedSharingOptionsProvider.getPropertyModels(mContentTypes,
                        mChromeShareExtras.getDetailedContentType(), mIsMultiWindow));
        mBottomSheetController.requestShowContent(mBottomSheet, /*animate=*/false);
    }

    // View.OnLayoutChangeListener
    @Override
    public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        if ((oldRight - oldLeft) == (right - left)) {
            return;
        }
        mBottomSheet.getFirstPartyView().invalidate();
        ViewUtils.requestLayout(mBottomSheet.getFirstPartyView(),
                "ShareSheetCoordinator.onLayoutChange first party view");
        mBottomSheet.getThirdPartyView().invalidate();
        ViewUtils.requestLayout(mBottomSheet.getThirdPartyView(),
                "ShareSheetCoordinator.onLayoutChange third party view");
    }
}
