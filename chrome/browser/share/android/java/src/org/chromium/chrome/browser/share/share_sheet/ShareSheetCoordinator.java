// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import android.app.Activity;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.res.Configuration;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ChromeShareExtras.DetailedContentType;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.share.ShareRankingBridge;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextCoordinator;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextCoordinator.LinkGeneration;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextMetricsHelper;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetLinkToggleCoordinator.LinkToggleState;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetLinkToggleMetricsHelper.LinkToggleMetricsDetails;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.modules.image_editor.ImageEditorModuleProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.ActivityStateObserver;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
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
    private final SettingsLauncher mSettingsLauncher;
    private final boolean mIsSyncEnabled;
    private final ImageEditorModuleProvider mImageEditorModuleProvider;
    private final BottomSheetObserver mBottomSheetObserver;
    private final LargeIconBridge mIconBridge;
    private final Tracker mFeatureEngagementTracker;

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
    private @LinkGeneration int mLinkGenerationStatusForMetrics = LinkGeneration.MAX;
    private LinkToggleMetricsDetails mLinkToggleMetricsDetails =
            new LinkToggleMetricsDetails(LinkToggleState.COUNT, DetailedContentType.NOT_SPECIFIED);

    // This same constant is used on the C++ side, in ShareRanking, to indicate
    // the position of the special "More..." target. Don't change its value
    // without also changing the C++ side.
    private static final String MORE_TARGET_NAME = "$more";

    // Don't log click indexes for usage-ranked items: the ordering is local to this client, so
    // histogramming them would have no value.
    private static final int NO_LOG_INDEX = -1;

    /**
     * Constructs a new ShareSheetCoordinator.
     *
     * @param controller The {@link BottomSheetController} for the current activity.
     * @param lifecycleDispatcher Dispatcher for activity lifecycle events, e.g. configuration
     * changes.
     * @param tabProvider Supplier for the current activity tab.
     * @param modelBuilder The {@link ShareSheetPropertyModelBuilder} for the share sheet.
     * @param imageEditorModuleProvider Image Editor module entry point if present in the APK.
     */
    // TODO(crbug/1022172): Should be package-protected once modularization is complete.
    public ShareSheetCoordinator(BottomSheetController controller,
            ActivityLifecycleDispatcher lifecycleDispatcher, Supplier<Tab> tabProvider,
            ShareSheetPropertyModelBuilder modelBuilder, Callback<Tab> printTab,
            LargeIconBridge iconBridge, SettingsLauncher settingsLauncher, boolean isSyncEnabled,
            ImageEditorModuleProvider imageEditorModuleProvider, Tracker featureEngagementTracker) {
        mBottomSheetController = controller;
        mLifecycleDispatcher = lifecycleDispatcher;
        mLifecycleDispatcher.register(this);
        mTabProvider = tabProvider;
        mPropertyModelBuilder = modelBuilder;
        mPrintTabCallback = printTab;
        mSettingsLauncher = settingsLauncher;
        mIsSyncEnabled = isSyncEnabled;
        mImageEditorModuleProvider = imageEditorModuleProvider;
        mBottomSheetObserver = new EmptyBottomSheetObserver() {
            @Override
            public void onSheetContentChanged(BottomSheetContent bottomSheet) {
                super.onSheetContentChanged(bottomSheet);
                if (bottomSheet == mBottomSheet) {
                    mBottomSheet.getContentView().addOnLayoutChangeListener(
                            ShareSheetCoordinator.this::onLayoutChange);
                } else {
                    mBottomSheet.getContentView().removeOnLayoutChangeListener(
                            ShareSheetCoordinator.this::onLayoutChange);
                }
            }
        };
        mBottomSheetController.addObserver(mBottomSheetObserver);
        mIconBridge = iconBridge;
        mFeatureEngagementTracker = featureEngagementTracker;
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
    }

    // TODO(crbug/1022172): Should be package-protected once modularization is complete.
    @Override
    public void showShareSheet(
            ShareParams params, ChromeShareExtras chromeShareExtras, long shareStartTime) {
        mShareParams = params;
        mChromeShareExtras = chromeShareExtras;
        mActivity = params.getWindow().getActivity().get();
        if (!shouldShowPreemptiveLinkToText(chromeShareExtras)) {
            mShareSheetLinkToggleCoordinator.setShareParamsAndExtras(params, chromeShareExtras);
            mShareParams = mShareSheetLinkToggleCoordinator.getDefaultShareParams();
        }
        if (mActivity == null) return;

        // Current tab information is necessary to create the first party options.
        if (!mExcludeFirstParty && (mTabProvider == null || mTabProvider.get() == null)) return;

        if (mWindowAndroid == null) {
            mWindowAndroid = params.getWindow();
            if (mWindowAndroid != null) {
                mWindowAndroid.addActivityStateObserver(this);
            }
        }

        mBottomSheet = new ShareSheetBottomSheetContent(
                mActivity, mIconBridge, this, params, mFeatureEngagementTracker);

        mShareStartTime = shareStartTime;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PREEMPTIVE_LINK_TO_TEXT_GENERATION)) {
            mLinkGenerationStatusForMetrics = mBottomSheet.getLinkGenerationState();
        }
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
        if ((!ChromeFeatureList.isEnabled(ChromeFeatureList.PREEMPTIVE_LINK_TO_TEXT_GENERATION)
                    || mLinkToTextCoordinator == null)
                && (!ChromeFeatureList.isEnabled(ChromeFeatureList.SHARING_HUB_LINK_TOGGLE)
                        || mShareSheetLinkToggleCoordinator == null)) {
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
     * If preemptive link to text generation is enable, create LinkToTextCoordinator
     * which will generate link to text, create a new share and show share sheet.
     * Otherwise show share sheet with the current share.
     *
     * @param params The {@link ShareParams} for the current share.
     * @param chromeShareExtras The {@link ChromeShareExtras} for the current share.
     * @param shareStartTime The start time of the current share.
     */
    public void showInitialShareSheet(
            ShareParams params, ChromeShareExtras chromeShareExtras, long shareStartTime) {
        if (shouldShowPreemptiveLinkToText(chromeShareExtras)) {
            if (!chromeShareExtras.isReshareHighlightedText()) {
                LinkToTextMetricsHelper.recordLinkToTextDiagnoseStatus(
                        LinkToTextMetricsHelper.LinkToTextDiagnoseStatus
                                .SHOW_SHARINGHUB_FOR_HIGHLIGHT);
            }
            String tabUrl =
                    mTabProvider.get().isInitialized() ? mTabProvider.get().getUrl().getSpec() : "";
            mLinkToTextCoordinator = new LinkToTextCoordinator(mTabProvider.get(), this,
                    chromeShareExtras, shareStartTime,
                    getUrlToShare(params, chromeShareExtras, tabUrl), params.getText());
        }
        mShareSheetLinkToggleCoordinator = new ShareSheetLinkToggleCoordinator(
                params, chromeShareExtras, mLinkToTextCoordinator);
        if (shouldShowPreemptiveLinkToText(chromeShareExtras)) {
            mLinkToTextCoordinator.shareLinkToText();
        } else {
            showShareSheet(params, chromeShareExtras, shareStartTime);
        }
    }

    private Profile getCurrentProfile() {
        if (mTabProvider != null && mTabProvider.get() != null
                && mTabProvider.get().getWebContents() != null) {
            return Profile.fromWebContents(mTabProvider.get().getWebContents());
        } else {
            return null;
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
                mTabProvider, mBottomSheetController, mBottomSheet, shareParams, mPrintTabCallback,
                mSettingsLauncher, mIsSyncEnabled, mShareStartTime, this,
                mImageEditorModuleProvider, mFeatureEngagementTracker,
                getUrlToShare(shareParams, chromeShareExtras,
                        mTabProvider.get().isInitialized() ? mTabProvider.get().getUrl().getSpec()
                                                           : ""),
                mLinkGenerationStatusForMetrics, mLinkToggleMetricsDetails);
        mIsMultiWindow = ApiCompatibilityUtils.isInMultiWindowMode(activity);

        return mChromeProvidedSharingOptionsProvider.getPropertyModels(
                contentTypes, mIsMultiWindow);
    }

    private boolean shouldShowPreemptiveLinkToText(ChromeShareExtras chromeShareExtras) {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.PREEMPTIVE_LINK_TO_TEXT_GENERATION)
                && chromeShareExtras.getDetailedContentType()
                == DetailedContentType.HIGHLIGHTED_TEXT;
    }

    private PropertyModel createMorePropertyModel(
            Activity activity, ShareParams params, boolean saveLastUsed) {
        return ShareSheetPropertyModelBuilder.createPropertyModel(
                AppCompatResources.getDrawable(activity, R.drawable.sharing_more),
                activity.getResources().getString(R.string.sharing_more_icon_label),
                /*accessibilityDescription=*/null,
                (shareParams)
                        -> {
                    recordShareMetrics("SharingHubAndroid.MoreSelected",
                            mLinkGenerationStatusForMetrics, mLinkToggleMetricsDetails);
                    mBottomSheetController.hideContent(mBottomSheet, true);
                    ShareHelper.showDefaultShareUi(params, getCurrentProfile(), saveLastUsed);
                    // Reset callback to prevent cancel() being called when the custom sheet is
                    // closed. The callback will be called by ShareHelper on actions from the
                    // default share UI.
                    params.setCallback(null);
                },
                /*displayNew*/ false);
    }

    private boolean shouldUseUsageRanking() {
        return getCurrentProfile() != null
                && ChromeFeatureList.isEnabled(ChromeFeatureList.SHARE_USAGE_RANKING);
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

        if (shouldUseUsageRanking()) {
            createThirdPartyPropertyModelsFromUsageRanking(
                    activity, params, contentTypes, saveLastUsed, callback);
            return;
        }

        List<PropertyModel> models = mPropertyModelBuilder.selectThirdPartyApps(mBottomSheet,
                contentTypes, params, saveLastUsed, mShareStartTime,
                mLinkGenerationStatusForMetrics, mLinkToggleMetricsDetails);
        models.add(createMorePropertyModel(activity, params, saveLastUsed));

        PostTask.postTask(UiThreadTaskTraits.DEFAULT, callback.bind(models));
    }

    class ResolveInfoPackageNameComparator implements Comparator<ResolveInfo> {
        @Override
        public int compare(ResolveInfo a, ResolveInfo b) {
            return a.activityInfo.packageName.compareTo(b.activityInfo.packageName);
        }
    };

    private void createThirdPartyPropertyModelsFromUsageRanking(Activity activity,
            ShareParams params, Set<Integer> contentTypes, boolean saveLastUsed,
            Callback<List<PropertyModel>> callback) {
        Profile profile = getCurrentProfile();

        assert profile != null;

        String type = contentTypesToTypeForRanking(contentTypes);

        PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();

        List<ResolveInfo> availableResolveInfos =
                pm.queryIntentActivities(ShareHelper.getShareLinkAppCompatibilityIntent(), 0);
        availableResolveInfos.addAll(pm.queryIntentActivities(
                ShareHelper.createShareFileAppCompatibilityIntent(params.getFileContentType()), 0));

        List<String> availableActivities = new ArrayList<String>();
        Map<String, ResolveInfo> resolveInfos = new HashMap<String, ResolveInfo>();

        // Sort the resolve infos by package name: on the backend, we store them by activity name,
        // but there's no particular reason activity names would be unique, and when we get them
        // from the system they're in arbitrary order. Here we sort them by package name (which *is*
        // unique) so that the user always gets a consistent option in a given slot.
        Collections.sort(availableResolveInfos, new ResolveInfoPackageNameComparator());

        // Accumulate the ResolveInfos for every package available on the system, but do not
        // construct their PropertyModels yet - there may be many packages but we will only show a
        // handful of them, and constructing a PropertyModel involves multiple synchronous calls to
        // the PackageManager which can be quite slow.
        for (ResolveInfo r : availableResolveInfos) {
            String name = r.activityInfo.packageName + "/" + r.activityInfo.name;
            availableActivities.add(name);
            resolveInfos.put(name, r);
        }

        int length = numberOf3PTilesToShow(activity);

        // TODO(ellyjones): Does !saveLastUsed always imply that we shouldn't incorporate the share
        // into our ranking?
        boolean persist = !profile.isOffTheRecord() && saveLastUsed;

        ShareRankingBridge.rank(profile, type, availableActivities, length, persist, ranking -> {
            onThirdPartyShareTargetsReceived(
                    callback, resolveInfos, activity, params, saveLastUsed, ranking);
        });
    }

    private int numberOf3PTilesToShow(Activity activity) {
        final boolean shouldFixMore =
                ChromeFeatureList.isEnabled(ChromeFeatureList.SHARE_USAGE_RANKING_FIXED_MORE);

        if (!shouldFixMore) return ShareSheetPropertyModelBuilder.MAX_NUM_APPS;

        int screenWidth =
                ContextUtils.getApplicationContext().getResources().getDisplayMetrics().widthPixels;
        int tileWidth =
                activity.getResources().getDimensionPixelSize(R.dimen.sharing_hub_tile_width);
        int tileMargin =
                activity.getResources().getDimensionPixelSize(R.dimen.sharing_hub_tile_margin);
        // In 'fix more' mode, ask for as many tiles as can fit; this will probably end up looking a
        // bit strange since there will likely be an uneven amount of padding on the right edge.
        // When not in that mode, the default is 10 tiles.
        //
        // Each tile has margin on both sides, so:
        int tileVisualWidth = (2 * tileMargin) + tileWidth;
        return (screenWidth - (2 * tileMargin)) / tileVisualWidth;
    }

    private String contentTypesToTypeForRanking(Set<Integer> contentTypes) {
        // TODO(ellyjones): Once we have field data, check whether the split into image vs not image
        // is sufficient (i.e. is share ranking is performing well with a split this coarse).
        if (contentTypes.contains(ShareSheetPropertyModelBuilder.ContentType.IMAGE)) {
            return "image";
        } else {
            return "other";
        }
    }

    private void onThirdPartyShareTargetsReceived(Callback<List<PropertyModel>> callback,
            Map<String, ResolveInfo> resolveInfos, Activity activity, ShareParams params,
            boolean saveLastUsed, List<String> targets) {
        // Build PropertyModels for all the ResolveInfos that correspond to
        // actual targets, in the order that we're going to show them.
        List<PropertyModel> models = new ArrayList<PropertyModel>();
        for (String target : targets) {
            if (target.equals(MORE_TARGET_NAME)) {
                models.add(createMorePropertyModel(activity, params, saveLastUsed));
            } else if (!target.equals("")) {
                assert resolveInfos.get(target) != null;
                models.add(mPropertyModelBuilder.buildThirdPartyAppModel(mBottomSheet, params,
                        resolveInfos.get(target), saveLastUsed, mShareStartTime, NO_LOG_INDEX,
                        mLinkGenerationStatusForMetrics, mLinkToggleMetricsDetails));
            }
        }
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, callback.bind(models));
    }

    static void recordShareMetrics(String featureName, @LinkGeneration int linkGenerationStatus,
            LinkToggleMetricsDetails linkToggleMetricsDetails, long shareStartTime) {
        recordShareMetrics(featureName, linkGenerationStatus, linkToggleMetricsDetails);
        recordTimeToShare(shareStartTime);
    }

    private static void recordShareMetrics(String featureName,
            @LinkGeneration int linkGenerationStatus,
            LinkToggleMetricsDetails linkToggleMetricsDetails) {
        RecordUserAction.record(featureName);
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PREEMPTIVE_LINK_TO_TEXT_GENERATION)) {
            LinkToTextMetricsHelper.recordSharedHighlightStateMetrics(linkGenerationStatus);
        }
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SHARING_HUB_LINK_TOGGLE)) {
            ShareSheetLinkToggleMetricsHelper.recordLinkToggleSharedStateMetric(
                    linkToggleMetricsDetails);
        }
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
    private String getUrlToShare(
            ShareParams shareParams, ChromeShareExtras chromeShareExtras, String tabUrl) {
        if (!TextUtils.isEmpty(shareParams.getUrl())) {
            return shareParams.getUrl();
        } else if (!chromeShareExtras.getImageSrcUrl().isEmpty()) {
            return chromeShareExtras.getImageSrcUrl().getSpec();
        }
        return tabUrl;
    }

    // ActivityStateObserver
    @Override
    public void onActivityResumed() {}

    @Override
    public void onActivityPaused() {
        if (mBottomSheet != null) {
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
                mChromeProvidedSharingOptionsProvider.getPropertyModels(
                        mContentTypes, mIsMultiWindow));
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
        mBottomSheet.getFirstPartyView().requestLayout();
        mBottomSheet.getThirdPartyView().invalidate();
        mBottomSheet.getThirdPartyView().requestLayout();
    }

}
