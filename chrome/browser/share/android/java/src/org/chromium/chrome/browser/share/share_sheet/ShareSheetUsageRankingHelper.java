// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import android.app.Activity;
import android.content.pm.ResolveInfo;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareContentTypeHelper;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.share.ShareMetricsUtils.ShareCustomAction;
import org.chromium.chrome.browser.share.ShareRankingBridge;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextCoordinator.LinkGeneration;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetLinkToggleMetricsHelper.LinkToggleMetricsDetails;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Helper class for ShareSheetCoordinator that hold Usage Ranking functions. */
public class ShareSheetUsageRankingHelper {
    // Knobs to allow for overriding the layout behavior of the share sheet row,
    // as used for deciding how to rank share targets. These are here to allow
    // tests not to depend on either the real physical dimensions of the test
    // device or the real layout values, which are in the resource bundle and
    // may vary depending on screen DPI.
    public static int FORCED_SCREEN_WIDTH_FOR_TEST;
    public static int FORCED_TILE_WIDTH_FOR_TEST;
    public static int FORCED_TILE_MARGIN_FOR_TEST;

    // This same constant is used on the C++ side, in ShareRanking, to indicate
    // the position of the special "More..." target. Don't change its value
    // without also changing the C++ side.
    private static final String MORE_TARGET_NAME = "$more";

    // Packages in this set will never be offered as share targets, even if they
    // match the share intent. This allows us to hide the Android CTS shim
    // packages, which are installed on some OEM production builds and claim to
    // handle all share intents, but are actually intended only to be used for
    // testing.
    private static final Set<String> PACKAGE_BLOCK_LIST =
            Set.of(
                    // https://crbug.com/1323786
                    "com.android.cts.ctsshim", "com.android.cts.priv.ctsshim");

    // Don't log click indexes for usage-ranked items: the ordering is local to this client, so
    // histogramming them would have no value.
    private static final int NO_LOG_INDEX = -1;

    private final BottomSheetController mBottomSheetController;
    private final ShareSheetPropertyModelBuilder mPropertyModelBuilder;
    private final Profile mProfile;

    private ShareSheetBottomSheetContent mBottomSheet;
    private long mShareStartTime;
    private @LinkGeneration int mLinkGenerationStatusForMetrics;
    private LinkToggleMetricsDetails mLinkToggleMetricsDetails;

    // Variables used for testing
    private boolean mDisableBridgeForTesting;
    private List<String> mTargetsForTesting;

    /**
     * Constructs a new ShareSheetUsageRankingHelper.
     *
     * @param bottomSheetController The {@link BottomSheetController} for the current activity.
     * @param bottomSheet The bottomSheet for the current activity.
     * @param shareStartTime The start time of the current share.
     * @param linkGenerationStatusForMetrics User action of sharing text from failed link-to-text
     *         generation, sharing text from successful link-to-text generation, or sharing
     *         link-to-text.
     * @param linkToggleMetricsDetails {@link LinkToggleMetricsDetails} to record link toggle
     *         metrics, and contains the {@link LinkToggleState} to update to.
     * @param propertyModelBuilder The {@link ShareSheetPropertyModelBuilder} for the share sheet.
     * @param profile The current profile of the User.
     */
    ShareSheetUsageRankingHelper(
            BottomSheetController bottomSheetController,
            ShareSheetBottomSheetContent bottomSheet,
            long shareStartTime,
            int linkGenerationStatusForMetrics,
            LinkToggleMetricsDetails linkToggleMetricsDetails,
            ShareSheetPropertyModelBuilder propertyModelBuilder,
            Profile profile) {
        mBottomSheetController = bottomSheetController;
        mPropertyModelBuilder = propertyModelBuilder;
        mProfile = profile;
        mBottomSheet = bottomSheet;
        mShareStartTime = shareStartTime;
        mLinkGenerationStatusForMetrics = linkGenerationStatusForMetrics;
        mLinkToggleMetricsDetails = linkToggleMetricsDetails;
    }

    void setTargetsForTesting(List<String> targets) {
        mDisableBridgeForTesting = true;
        mTargetsForTesting = targets;
    }

    void createThirdPartyPropertyModelsFromUsageRanking(
            Activity activity,
            ShareParams params,
            Set<Integer> contentTypes,
            boolean saveLastUsed,
            Callback<List<PropertyModel>> callback) {
        String type = contentTypesToTypeForRanking(contentTypes);

        List<ResolveInfo> availableResolveInfos = ShareHelper.getCompatibleAppsForSharingText();
        availableResolveInfos.addAll(
                ShareHelper.getCompatibleAppsForSharingFiles(params.getFileContentType()));

        List<String> availableActivities = new ArrayList<String>();
        Map<String, ResolveInfo> resolveInfos = new HashMap<String, ResolveInfo>();

        // The system can return ResolveInfos which refer to activities exported
        // by Chrome - especially the Print activity. We don't want to offer
        // these as "third party" targets, so filter them out.
        availableResolveInfos = filterOutOwnResolveInfos(availableResolveInfos);

        // Certain resolve infos correspond to system-internal or testing
        // packages and should never be offered.
        availableResolveInfos = filterOutBlocklistedResolveInfos(availableResolveInfos);

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

        if (mDisableBridgeForTesting) {
            onThirdPartyShareTargetsReceived(
                    callback, resolveInfos, activity, params, saveLastUsed, mTargetsForTesting);
            return;
        }

        // TODO(ellyjones): Does !saveLastUsed always imply that we shouldn't incorporate the share
        // into our ranking?
        boolean persist = !mProfile.isOffTheRecord() && saveLastUsed;

        ShareRankingBridge.rank(
                mProfile,
                type,
                availableActivities,
                fold,
                length,
                persist,
                ranking -> {
                    onThirdPartyShareTargetsReceived(
                            callback, resolveInfos, activity, params, saveLastUsed, ranking);
                });
    }

    private String contentTypesToTypeForRanking(Set<Integer> contentTypes) {
        // TODO(ellyjones): Once we have field data, check whether the split into image vs not image
        // is sufficient (i.e. is share ranking is performing well with a split this coarse).
        if (contentTypes.contains(ShareContentTypeHelper.ContentType.IMAGE)) {
            return "image";
        } else {
            return "other";
        }
    }

    // Returns a new list of ResolveInfos containing only the elements of the
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

    // Returns a new list of ResolveInfos with blocklisted packages removed.
    @VisibleForTesting
    static List<ResolveInfo> filterOutBlocklistedResolveInfos(List<ResolveInfo> infos) {
        List<ResolveInfo> remaining = new ArrayList<ResolveInfo>();
        for (ResolveInfo info : infos) {
            if (!PACKAGE_BLOCK_LIST.contains(info.activityInfo.packageName)) {
                remaining.add(info);
            }
        }
        return remaining;
    }

    private int numberOf3PTilesThatFitOnScreen(Activity activity) {
        int screenWidth =
                FORCED_SCREEN_WIDTH_FOR_TEST != 0
                        ? FORCED_SCREEN_WIDTH_FOR_TEST
                        : ContextUtils.getApplicationContext()
                                .getResources()
                                .getDisplayMetrics()
                                .widthPixels;
        int tileWidth =
                FORCED_TILE_WIDTH_FOR_TEST != 0
                        ? FORCED_TILE_WIDTH_FOR_TEST
                        : activity.getResources()
                                .getDimensionPixelSize(R.dimen.sharing_hub_tile_width);
        int tileMargin =
                FORCED_TILE_MARGIN_FOR_TEST != 0
                        ? FORCED_TILE_MARGIN_FOR_TEST
                        : activity.getResources()
                                .getDimensionPixelSize(R.dimen.sharing_hub_tile_margin);
        // In 'fix more' mode, ask for as many tiles as can fit; this will probably end up looking a
        // bit strange since there will likely be an uneven amount of padding on the right edge.
        // When not in that mode, the default is 10 tiles.
        //
        // Each tile has margin on both sides, so:
        int tileVisualWidth = (2 * tileMargin) + tileWidth;
        return (screenWidth - (2 * tileMargin)) / tileVisualWidth;
    }

    private void onThirdPartyShareTargetsReceived(
            Callback<List<PropertyModel>> callback,
            Map<String, ResolveInfo> resolveInfos,
            Activity activity,
            ShareParams params,
            boolean saveLastUsed,
            List<String> targets) {
        // Build PropertyModels for all the ResolveInfos that correspond to
        // actual targets, in the order that we're going to show them.
        List<PropertyModel> models = new ArrayList<PropertyModel>();
        for (String target : targets) {
            if (target.equals(MORE_TARGET_NAME)) {
                models.add(createMorePropertyModel(activity, params, saveLastUsed));
            } else if (!target.equals("")) {
                assert resolveInfos.get(target) != null;
                models.add(
                        mPropertyModelBuilder.buildThirdPartyAppModel(
                                mBottomSheet,
                                params,
                                resolveInfos.get(target),
                                saveLastUsed,
                                mShareStartTime,
                                NO_LOG_INDEX,
                                mLinkGenerationStatusForMetrics,
                                mLinkToggleMetricsDetails));
            }
        }
        PostTask.postTask(TaskTraits.UI_DEFAULT, callback.bind(models));
    }

    PropertyModel createMorePropertyModel(
            Activity activity, ShareParams params, boolean saveLastUsed) {
        return ShareSheetPropertyModelBuilder.createPropertyModel(
                AppCompatResources.getDrawable(activity, R.drawable.sharing_more),
                activity.getResources().getString(R.string.sharing_more_icon_label),
                /* accessibilityDescription= */ null,
                (shareParams) -> {
                    ShareSheetCoordinator.recordShareMetrics(
                            ShareCustomAction.INVALID,
                            "SharingHubAndroid.MoreSelected",
                            mLinkGenerationStatusForMetrics,
                            mLinkToggleMetricsDetails,
                            mShareStartTime,
                            mProfile);
                    mBottomSheetController.hideContent(mBottomSheet, true);
                    ShareHelper.shareWithSystemShareSheetUi(params, mProfile, saveLastUsed);
                    // Reset callback to prevent cancel() being called when the custom sheet is
                    // closed. The callback will be called by ShareHelper on actions from the
                    // default share UI.
                    params.setCallback(null);
                },
                /* showNewBadge= */ false);
    }

    static class ResolveInfoPackageNameComparator implements Comparator<ResolveInfo> {
        @Override
        public int compare(ResolveInfo a, ResolveInfo b) {
            return a.activityInfo.packageName.compareTo(b.activityInfo.packageName);
        }
    }
}
