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
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
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
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.feature_engagement.EventConstants;
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
    // Knobs to allow for overriding the layout behavior of the share sheet row,
    // as used for deciding how to rank share targets. These are here to allow
    // tests not to depend on either the real physical dimensions of the test
    // device or the real layout values, which are in the resource bundle and
    // may vary depending on screen DPI.
    public static int FORCED_SCREEN_WIDTH_FOR_TEST;
    public static int FORCED_TILE_WIDTH_FOR_TEST;
    public static int FORCED_TILE_MARGIN_FOR_TEST;

    private final BottomSheetController mBottomSheetController;
    private final Supplier<Tab> mTabProvider;
    private final ShareSheetPropertyModelBuilder mPropertyModelBuilder;
    private final Callback<Tab> mPrintTabCallback;
    private final boolean mIsIncognito;
    private final ImageEditorModuleProvider mImageEditorModuleProvider;
    private final BottomSheetObserver mBottomSheetObserver;
    private final LargeIconBridge mIconBridge;
    private final Tracker mFeatureEngagementTracker;
    private final Supplier<Profile> mProfileSupplier;

    private long mShareStartTime;
    private boolean mExcludeFirstParty;
    private boolean mIsMultiWindow;
    private boolean mDisableUsageRankingForTesting;
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
     * @param isIncognito Whether the share sheet was opened in incognito mode or not.
     * @param imageEditorModuleProvider Image Editor module entry point if present in the APK.
     * @param profileSupplier A profile supplier to pull the current profile of the User.
     */
    // TODO(crbug/1022172): Should be package-protected once modularization is complete.
    public ShareSheetCoordinator(BottomSheetController controller,
            ActivityLifecycleDispatcher lifecycleDispatcher, Supplier<Tab> tabProvider,
            ShareSheetPropertyModelBuilder modelBuilder, Callback<Tab> printTab,
            LargeIconBridge iconBridge, boolean isIncognito,
            ImageEditorModuleProvider imageEditorModuleProvider, Tracker featureEngagementTracker,
            Supplier<Profile> profileSupplier) {
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
        mProfileSupplier = profileSupplier;
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
                mLinkGenerationStatusForMetrics, mLinkToggleMetricsDetails, mProfileSupplier);
        mIsMultiWindow = ApiCompatibilityUtils.isInMultiWindowMode(activity);

        return mChromeProvidedSharingOptionsProvider.getPropertyModels(
                contentTypes, chromeShareExtras.getDetailedContentType(), mIsMultiWindow);
    }

    private boolean shouldShowLinkToText(ChromeShareExtras chromeShareExtras) {
        return chromeShareExtras.getDetailedContentType() == DetailedContentType.HIGHLIGHTED_TEXT
                && mTabProvider != null && mTabProvider.hasValue();
    }

    private PropertyModel createMorePropertyModel(
            Activity activity, ShareParams params, boolean saveLastUsed) {
        return ShareSheetPropertyModelBuilder.createPropertyModel(
                AppCompatResources.getDrawable(activity, R.drawable.sharing_more),
                activity.getResources().getString(R.string.sharing_more_icon_label),
                /*accessibilityDescription=*/null,
                (shareParams)
                        -> {
                    Profile profile = mProfileSupplier.get();
                    recordShareMetrics("SharingHubAndroid.MoreSelected",
                            mLinkGenerationStatusForMetrics, mLinkToggleMetricsDetails, profile);
                    mBottomSheetController.hideContent(mBottomSheet, true);
                    ShareHelper.showDefaultShareUi(params, profile, saveLastUsed);
                    // Reset callback to prevent cancel() being called when the custom sheet is
                    // closed. The callback will be called by ShareHelper on actions from the
                    // default share UI.
                    params.setCallback(null);
                },
                /*displayNew*/ false);
    }

    @VisibleForTesting
    void setDisableUsageRankingForTesting(boolean shouldDisableUsageRanking) {
        mDisableUsageRankingForTesting = shouldDisableUsageRanking;
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

        if (!mDisableUsageRankingForTesting) {
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
        Profile profile = mProfileSupplier.get();
        assert profile != null;

        String type = contentTypesToTypeForRanking(contentTypes);

        PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();

        List<ResolveInfo> availableResolveInfos =
                pm.queryIntentActivities(ShareHelper.getShareLinkAppCompatibilityIntent(), 0);
        availableResolveInfos.addAll(pm.queryIntentActivities(
                ShareHelper.createShareFileAppCompatibilityIntent(params.getFileContentType()), 0));

        List<String> availableActivities = new ArrayList<String>();
        Map<String, ResolveInfo> resolveInfos = new HashMap<String, ResolveInfo>();

        // The system can return ResolveInfos which refer to activities exported
        // by Chrome - especially the Print activity. We don't want to offer
        // these as "third party" targets, so filter them out.
        availableResolveInfos = filterOutOwnResolveInfos(availableResolveInfos);

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

        int fold = numberOf3PTilesThatFitOnScreen(activity);
        int length = fold;

        // TODO(ellyjones): Does !saveLastUsed always imply that we shouldn't incorporate the share
        // into our ranking?
        boolean persist = !profile.isOffTheRecord() && saveLastUsed;

        ShareRankingBridge.rank(
                profile, type, availableActivities, fold, length, persist, ranking -> {
                    onThirdPartyShareTargetsReceived(
                            callback, resolveInfos, activity, params, saveLastUsed, ranking);
                });
    }

    // Returns a new list of ResovleInfos containing only the elements of the
    // supplied list which are not references to activities from the current
    // package.
    private List<ResolveInfo> filterOutOwnResolveInfos(List<ResolveInfo> infos) {
        String currentPackageName = ContextUtils.getApplicationContext().getPackageName();
        List<ResolveInfo> remaining = new ArrayList<ResolveInfo>();
        for (ResolveInfo info : infos) {
            if (!info.activityInfo.packageName.equals(currentPackageName)) {
                remaining.add(info);
            }
        }
        return remaining;
    }
    private int numberOf3PTilesThatFitOnScreen(Activity activity) {
        int screenWidth = FORCED_SCREEN_WIDTH_FOR_TEST != 0 ? FORCED_SCREEN_WIDTH_FOR_TEST
                                                            : ContextUtils.getApplicationContext()
                                                                      .getResources()
                                                                      .getDisplayMetrics()
                                                                      .widthPixels;
        int tileWidth = FORCED_TILE_WIDTH_FOR_TEST != 0
                ? FORCED_TILE_WIDTH_FOR_TEST
                : activity.getResources().getDimensionPixelSize(R.dimen.sharing_hub_tile_width);
        int tileMargin = FORCED_TILE_MARGIN_FOR_TEST != 0
                ? FORCED_TILE_MARGIN_FOR_TEST
                : activity.getResources().getDimensionPixelSize(R.dimen.sharing_hub_tile_margin);
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
        mBottomSheet.getFirstPartyView().requestLayout();
        mBottomSheet.getThirdPartyView().invalidate();
        mBottomSheet.getThirdPartyView().requestLayout();
    }

}
