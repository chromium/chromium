// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import android.content.ComponentName;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.drawable.Drawable;
import android.view.View.OnClickListener;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareContentTypeHelper.ContentType;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.share.ShareMetricsUtils.ShareCustomAction;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextCoordinator.LinkGeneration;
import org.chromium.chrome.browser.share.share_sheet.ShareSheetLinkToggleMetricsHelper.LinkToggleMetricsDetails;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Set;

/**
 * Handles displaying the share sheet. The version used depends on several conditions:
 *
 * <ul>
 *   <li>Android L+: system share sheet
 *   <li>#chrome-sharing-hub enabled: custom share sheet
 * </ul>
 */
class ShareSheetPropertyModelBuilder {
    public static final int MAX_NUM_APPS = 7;
    // Variations parameter name for the comma-separated list of third-party activity names.
    private static final ArrayList<String> FALLBACK_ACTIVITIES =
            new ArrayList<>(
                    Arrays.asList(
                            "com.whatsapp.ContactPicker",
                            "com.facebook.composer.shareintent.ImplicitShareIntentHandlerDefaultAlias",
                            "com.google.android.gm.ComposeActivityGmailExternal",
                            "com.instagram.share.handleractivity.StoryShareHandlerActivity",
                            "com.facebook.messenger.intents.ShareIntentHandler",
                            "com.google.android.apps.messaging.ui.conversationlist.ShareIntentActivity",
                            "com.twitter.composer.ComposerActivity",
                            "com.snap.mushroom.MainActivity",
                            "com.pinterest.activity.create.PinItActivity",
                            "com.linkedin.android.publishing.sharing.LinkedInDeepLinkActivity",
                            "jp.naver.line.android.activity.selectchat.SelectChatActivityLaunchActivity",
                            "com.facebook.lite.composer.activities.ShareIntentMultiPhotoAlphabeticalAlias",
                            "com.facebook.mlite.share.view.ShareActivity",
                            "com.samsung.android.email.ui.messageview.MessageFileView",
                            "com.yahoo.mail.ui.activities.ComposeActivity",
                            "org.telegram.ui.LaunchActivity",
                            "com.tencent.mm.ui.tools.ShareImgUI"));

    private final BottomSheetController mBottomSheetController;
    private final PackageManager mPackageManager;
    private final Profile mProfile;

    ShareSheetPropertyModelBuilder(
            BottomSheetController bottomSheetController,
            PackageManager packageManager,
            Profile profile) {
        mBottomSheetController = bottomSheetController;
        mPackageManager = packageManager;
        mProfile = profile;
    }

    public PropertyModel buildThirdPartyAppModel(
            ShareSheetBottomSheetContent bottomSheet,
            ShareParams params,
            ResolveInfo info,
            boolean saveLastUsed,
            long shareStartTime,
            int logIndex,
            @LinkGeneration int linkGenerationStatusForMetrics,
            LinkToggleMetricsDetails linkToggleMetricsDetails) {
        OnClickListener onClickListener =
                v -> {
                    onThirdPartyAppSelected(
                            bottomSheet,
                            params,
                            saveLastUsed,
                            info.activityInfo,
                            logIndex,
                            shareStartTime,
                            linkGenerationStatusForMetrics,
                            linkToggleMetricsDetails);
                };
        return createPropertyModel(
                ShareHelper.loadIconForResolveInfo(info, mPackageManager),
                (String) info.loadLabel(mPackageManager),
                /* accessibilityDescription= */ null,
                onClickListener,
                /* showNewBadge= */ false);
    }

    protected List<PropertyModel> selectThirdPartyApps(
            ShareSheetBottomSheetContent bottomSheet,
            Set<Integer> contentTypes,
            ShareParams params,
            boolean saveLastUsed,
            long shareStartTime,
            @LinkGeneration int linkGenerationStatusForMetrics,
            LinkToggleMetricsDetails linkToggleMetricsDetails) {
        List<String> thirdPartyActivityNames = FALLBACK_ACTIVITIES;
        List<ResolveInfo> resolveInfoList =
                getCompatibleApps(contentTypes, params.getFileContentType());
        List<ResolveInfo> thirdPartyActivities = new ArrayList<>();

        // Construct a list of 3P apps. The list should be sorted by the country-specific
        // ranking when available or the fallback list defined above.  If less than MAX_NUM_APPS
        // are available the list is filled with whatever else is available.
        for (String s : thirdPartyActivityNames) {
            for (ResolveInfo res : resolveInfoList) {
                if (res.activityInfo.name.equals(s)) {
                    thirdPartyActivities.add(res);
                    resolveInfoList.remove(res);
                    break;
                }
            }
            if (thirdPartyActivities.size() == MAX_NUM_APPS) {
                break;
            }
        }

        String chromePackageName = ContextUtils.getApplicationContext().getPackageName();
        for (ResolveInfo res : resolveInfoList) {
            if (!res.activityInfo.packageName.equals(chromePackageName)) {
                thirdPartyActivities.add(res);
            }
            if (thirdPartyActivities.size() == MAX_NUM_APPS) {
                break;
            }
        }

        ArrayList<PropertyModel> models = new ArrayList<>();
        for (int i = 0; i < MAX_NUM_APPS && i < thirdPartyActivities.size(); ++i) {
            models.add(
                    buildThirdPartyAppModel(
                            bottomSheet,
                            params,
                            thirdPartyActivities.get(i),
                            saveLastUsed,
                            shareStartTime,
                            i,
                            linkGenerationStatusForMetrics,
                            linkToggleMetricsDetails));
        }

        return models;
    }

    private void onThirdPartyAppSelected(
            ShareSheetBottomSheetContent bottomSheet,
            ShareParams params,
            boolean saveLastUsed,
            ActivityInfo ai,
            int logIndex,
            long shareStartTime,
            @LinkGeneration int linkGenerationStatusForMetrics,
            LinkToggleMetricsDetails linkToggleMetricsDetails) {
        // Record all metrics.
        if (logIndex >= 0) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Sharing.SharingHubAndroid.ThirdPartyAppUsage", logIndex, MAX_NUM_APPS + 1);
        }
        ShareSheetCoordinator.recordShareMetrics(
                ShareCustomAction.INVALID,
                "SharingHubAndroid.ThirdPartyAppSelected",
                linkGenerationStatusForMetrics,
                linkToggleMetricsDetails,
                shareStartTime,
                mProfile);
        ComponentName component = new ComponentName(ai.applicationInfo.packageName, ai.name);
        ShareParams.TargetChosenCallback callback = params.getCallback();
        if (callback != null) {
            callback.onTargetChosen(component);
            // Reset callback after onTargetChosen() is called to prevent cancel() being called when
            // the sheet is closed.
            params.setCallback(null);
        }
        mBottomSheetController.hideContent(bottomSheet, true);
        // Fire intent through ShareHelper.
        ShareHelper.shareDirectly(params, component, mProfile, saveLastUsed);
    }

    /**
     * Returns a list of compatible {@link ResolveInfo}s for the set of {@link ContentType}s.
     *
     * Adds {@link ResolveInfo}s according to the following logic:
     *
     * <ul>
     *     <li>If the {@link ContentType}s contain URL or Text, add text-sharing apps.
     *     <li>If the {@link ContentType}s contain a file, add file-sharing apps compatible
     *     {@code fileContentType}.
     * </ul>
     */
    private List<ResolveInfo> getCompatibleApps(Set<Integer> contentTypes, String fileContentType) {
        List<ResolveInfo> resolveInfoList = new ArrayList<>();
        if (!Collections.disjoint(
                contentTypes,
                Arrays.asList(
                        ContentType.LINK_PAGE_NOT_VISIBLE,
                        ContentType.LINK_PAGE_VISIBLE,
                        ContentType.TEXT,
                        ContentType.HIGHLIGHTED_TEXT))) {
            resolveInfoList.addAll(ShareHelper.getCompatibleAppsForSharingText());
        }
        if (!Collections.disjoint(
                contentTypes,
                Arrays.asList(
                        ContentType.IMAGE,
                        ContentType.IMAGE_AND_LINK,
                        ContentType.OTHER_FILE_TYPE))) {
            resolveInfoList.addAll(ShareHelper.getCompatibleAppsForSharingFiles(fileContentType));
        }
        return resolveInfoList;
    }

    static PropertyModel createPropertyModel(
            Drawable icon,
            String label,
            @Nullable String accessibilityDescription,
            OnClickListener listener,
            boolean showNewBadge) {
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ShareSheetItemViewProperties.ALL_KEYS)
                        .with(ShareSheetItemViewProperties.ICON, icon)
                        .with(ShareSheetItemViewProperties.LABEL, label)
                        .with(ShareSheetItemViewProperties.CLICK_LISTENER, listener)
                        .with(ShareSheetItemViewProperties.SHOW_NEW_BADGE, showNewBadge);

        if (accessibilityDescription != null) {
            builder.with(
                    ShareSheetItemViewProperties.CONTENT_DESCRIPTION, accessibilityDescription);
        }
        return builder.build();
    }
}
