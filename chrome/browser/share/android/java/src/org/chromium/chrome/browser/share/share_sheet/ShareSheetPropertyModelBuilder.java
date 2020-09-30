// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import android.content.ComponentName;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.View.OnClickListener;

import androidx.annotation.IntDef;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Handles displaying the share sheet. The version used depends on several conditions:
 *
 * <ul>
 *   <li>Android K and below: custom share dialog
 *   <li>Android L+: system share sheet
 *   <li>#chrome-sharing-hub enabled: custom share sheet
 * </ul>
 */
// TODO(crbug/1022172): Should be package-protected once modularization is complete.
public class ShareSheetPropertyModelBuilder {
    @IntDef({ContentType.LINK_PAGE_VISIBLE, ContentType.LINK_PAGE_NOT_VISIBLE, ContentType.TEXT,
            ContentType.HIGHLIGHTED_TEXT, ContentType.LINK_AND_TEXT, ContentType.IMAGE,
            ContentType.OTHER_FILE_TYPE})
    @Retention(RetentionPolicy.SOURCE)
    @interface ContentType {
        int LINK_PAGE_VISIBLE = 0;
        int LINK_PAGE_NOT_VISIBLE = 1;
        int TEXT = 2;
        int HIGHLIGHTED_TEXT = 3;
        int LINK_AND_TEXT = 4;
        int IMAGE = 5;
        int OTHER_FILE_TYPE = 6;
    }

    private static final int MAX_NUM_APPS = 7;
    private static final String IMAGE_TYPE = "image/";
    // Variations parameter name for the comma-separated list of third-party activity names.
    private static final String PARAM_SHARING_HUB_THIRD_PARTY_APPS = "sharing-hub-third-party-apps";

    static final HashSet<Integer> ALL_CONTENT_TYPES = new HashSet<>(
            Arrays.asList(ContentType.LINK_PAGE_VISIBLE, ContentType.LINK_PAGE_NOT_VISIBLE,
                    ContentType.TEXT, ContentType.HIGHLIGHTED_TEXT, ContentType.LINK_AND_TEXT,
                    ContentType.IMAGE, ContentType.OTHER_FILE_TYPE));
    private static final ArrayList<String> FALLBACK_ACTIVITIES =
            new ArrayList<>(Arrays.asList("com.whatsapp.ContactPicker",
                    "com.facebook.composer.shareintent.ImplicitShareIntentHandlerDefaultAlias",
                    "com.google.android.gm.ComposeActivityGmailExternal",
                    "com.instagram.share.handleractivity.StoryShareHandlerActivity",
                    "com.facebook.messenger.intents.ShareIntentHandler",
                    "com.google.android.apps.messaging.ui.conversationlist.ShareIntentActivity",
                    "com.twitter.composer.ComposerActivity", "com.snap.mushroom.MainActivity",
                    "com.pinterest.activity.create.PinItActivity",
                    "com.linkedin.android.publishing.sharing.LinkedInDeepLinkActivity",
                    "jp.naver.line.android.activity.selectchat.SelectChatActivityLaunchActivity",
                    "com.facebook.lite.composer.activities.ShareIntentMultiPhotoAlphabeticalAlias",
                    "com.facebook.mlite.share.view.ShareActivity",
                    "com.samsung.android.email.ui.messageview.MessageFileView",
                    "com.yahoo.mail.ui.activities.ComposeActivity",
                    "org.telegram.ui.LaunchActivity", "com.tencent.mm.ui.tools.ShareImgUI"));

    private final BottomSheetController mBottomSheetController;
    private final PackageManager mPackageManager;

    // TODO(crbug/1022172): Should be package-protected once modularization is complete.
    public ShareSheetPropertyModelBuilder(
            BottomSheetController bottomSheetController, PackageManager packageManager) {
        mBottomSheetController = bottomSheetController;
        mPackageManager = packageManager;
    }

    /**
     * Returns a set of {@link ContentType}s for the current share.
     *
     * <p>If {@link ChromeFeatureList.CHROME_SHARING_HUB_V15} is not enabled, this returns a set
     * of all of the {@link ContentType}s. Otherwise, it adds {@link ContentType}s according to
     * the following logic:
     *
     * <ul>
     *     <li>If a URL is present, {@code isUrlOfVisiblePage} determines whether to add
     *     {@link ContentType.LINK_PAGE_VISIBLE} or {@link ContentType.LINK_PAGE_NOT_VISIBLE}.
     *     <li>If the text being shared is not the same as the URL, add {@link ContentType.TEXT}
     *     <li>If text is highlighted by user, add {@link ContentType.HIGHLIGHTED_TEXT}.
     *     <li>If the share contains files and the {@code fileContentType} is an image, add
     *     {@link ContentType.IMAGE}. Otherwise, add {@link ContentType.OTHER_FILE_TYPE}.
     * </ul>
     */
    static Set<Integer> getContentTypes(ShareParams params, ChromeShareExtras chromeShareExtras) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARING_HUB_V15)) {
            return ALL_CONTENT_TYPES;
        }
        Set<Integer> contentTypes = new HashSet<>();
        if (!TextUtils.isEmpty(params.getUrl())) {
            if (chromeShareExtras.isUrlOfVisiblePage()) {
                contentTypes.add(ContentType.LINK_PAGE_VISIBLE);
            } else {
                contentTypes.add(ContentType.LINK_PAGE_NOT_VISIBLE);
            }
        }
        if (!TextUtils.isEmpty(params.getText())) {
            if (chromeShareExtras.isUserHighlightedText()) {
                contentTypes.add(ContentType.HIGHLIGHTED_TEXT);
            } else {
                contentTypes.add(ContentType.TEXT);
            }
        }
        if (!TextUtils.isEmpty(params.getUrl()) && !TextUtils.isEmpty(params.getText())) {
            contentTypes.add(ContentType.LINK_AND_TEXT);
        }
        if (params.getFileUris() != null) {
            if (!TextUtils.isEmpty(params.getFileContentType())
                    && params.getFileContentType().startsWith(IMAGE_TYPE)) {
                contentTypes.add(ContentType.IMAGE);
            } else {
                contentTypes.add(ContentType.OTHER_FILE_TYPE);
            }
        }
        return contentTypes;
    }

    protected List<PropertyModel> selectThirdPartyApps(ShareSheetBottomSheetContent bottomSheet,
            Set<Integer> contentTypes, ShareParams params, boolean saveLastUsed,
            WindowAndroid window, long shareStartTime) {
        List<String> thirdPartyActivityNames = getThirdPartyActivityNames();
        final ShareParams.TargetChosenCallback callback = params.getCallback();
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
            ResolveInfo info = thirdPartyActivities.get(i);
            final int logIndex = i;
            OnClickListener onClickListener = v -> {
                onThirdPartyAppSelected(bottomSheet, params, window, callback, saveLastUsed,
                        info.activityInfo, logIndex, shareStartTime);
            };
            PropertyModel propertyModel =
                    createPropertyModel(ShareHelper.loadIconForResolveInfo(info, mPackageManager),
                            (String) info.loadLabel(mPackageManager), onClickListener);
            models.add(propertyModel);
        }

        return models;
    }

    private void onThirdPartyAppSelected(ShareSheetBottomSheetContent bottomSheet,
            ShareParams params, WindowAndroid window, ShareParams.TargetChosenCallback callback,
            boolean saveLastUsed, ActivityInfo ai, int logIndex, long shareStartTime) {
        // Record all metrics.
        RecordUserAction.record("SharingHubAndroid.ThirdPartyAppSelected");
        RecordHistogram.recordEnumeratedHistogram(
                "Sharing.SharingHubAndroid.ThirdPartyAppUsage", logIndex, MAX_NUM_APPS + 1);
        ChromeProvidedSharingOptionsProvider.recordTimeToShare(shareStartTime);
        ComponentName component = new ComponentName(ai.applicationInfo.packageName, ai.name);
        if (callback != null) {
            callback.onTargetChosen(component);
        }
        if (saveLastUsed) {
            ShareHelper.setLastShareComponentName(component);
        }
        mBottomSheetController.hideContent(bottomSheet, true);
        // Fire intent through ShareHelper.
        ShareHelper.shareDirectly(params, component);
    }

    /**
     * Returns a list of compatible {@link ResolveInfo}s for the set of {@link ContentType}s.
     *
     * <p>If {@link ChromeFeatureList.CHROME_SHARING_HUB_V15} is not enabled, this returns a list
     * of all of text-sharing apps. Otherwise, it adds {@link ResolveInfo}s according to the
     * following following logic:
     *
     * <ul>
     *     <li>If the {@link ContentType}s contain URL or Text, add text-sharing apps.
     *     <li>If the {@link ContentType}s contain a file, add file-sharing apps compatible
     *     {@code fileContentType}.
     * </ul>
     */
    private List<ResolveInfo> getCompatibleApps(Set<Integer> contentTypes, String fileContentType) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CHROME_SHARING_HUB_V15)) {
            return mPackageManager.queryIntentActivities(
                    ShareHelper.getShareLinkAppCompatibilityIntent(), 0);
        }
        List<ResolveInfo> resolveInfoList = new ArrayList<>();
        if (!Collections.disjoint(contentTypes,
                    Arrays.asList(ContentType.LINK_PAGE_NOT_VISIBLE, ContentType.LINK_PAGE_VISIBLE,
                            ContentType.TEXT, ContentType.HIGHLIGHTED_TEXT))) {
            resolveInfoList.addAll(mPackageManager.queryIntentActivities(
                    ShareHelper.getShareLinkAppCompatibilityIntent(), 0));
        }
        if (!Collections.disjoint(
                    contentTypes, Arrays.asList(ContentType.IMAGE, ContentType.OTHER_FILE_TYPE))) {
            resolveInfoList.addAll(mPackageManager.queryIntentActivities(
                    ShareHelper.createShareFileAppCompatibilityIntent(fileContentType), 0));
        }
        return resolveInfoList;
    }

    static PropertyModel createPropertyModel(
            Drawable icon, String label, OnClickListener listener) {
        return new PropertyModel.Builder(ShareSheetItemViewProperties.ALL_KEYS)
                .with(ShareSheetItemViewProperties.ICON, icon)
                .with(ShareSheetItemViewProperties.LABEL, label)
                .with(ShareSheetItemViewProperties.CLICK_LISTENER, listener)
                .build();
    }

    private List<String> getThirdPartyActivityNames() {
        String param = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.CHROME_SHARING_HUB, PARAM_SHARING_HUB_THIRD_PARTY_APPS);
        if (param.isEmpty()) {
            return FALLBACK_ACTIVITIES;
        }
        return new ArrayList<>(Arrays.asList(param.split(",")));
    }
}
