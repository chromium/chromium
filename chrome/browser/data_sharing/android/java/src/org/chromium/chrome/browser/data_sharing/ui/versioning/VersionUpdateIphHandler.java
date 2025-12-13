// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.versioning;

import android.content.Context;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.tab_group_sync.MessageType;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.VersioningMessageController;
import org.chromium.components.user_prefs.UserPrefs;

/** Handles showing IPH when a user updates Chrome and regains access to a shared tab group. */
@NullMarked
public class VersionUpdateIphHandler {

    // Prevents instantiation.
    private VersionUpdateIphHandler() {}

    /**
     * Attempts to show IPH on the anchor view if conditions for a version update message are met.
     *
     * @param userEducationHelper Helper for showing IPHs.
     * @param anchorView The view to anchor the IPH to.
     * @param profile The current profile.
     * @param requiresAutoOpenSettingEnabled The expected value of the "auto-open tab groups"
     *     setting. The IPH will only be shown if the setting's actual value matches this expected
     *     value.
     */
    public static void maybeShowVersioningIph(
            UserEducationHelper userEducationHelper,
            View anchorView,
            Profile profile,
            boolean requiresAutoOpenSettingEnabled) {
        PrefService prefService = UserPrefs.get(profile);
        if (!wontShowIphForProfile(profile)
                && requiresAutoOpenSettingEnabled
                        == prefService.getBoolean(Pref.AUTO_OPEN_SYNCED_TAB_GROUPS)) {
            showIph(userEducationHelper, anchorView, profile);
        }
    }

    private static void showIph(
            UserEducationHelper userEducationHelper, View anchorView, Profile profile) {
        VersioningMessageController versioningMessageController =
                getVersioningMessageController(profile);
        if (versioningMessageController == null || !shouldShowIph(versioningMessageController)) {
            return;
        }

        Context context = anchorView.getContext();
        TabGroupSyncService tabGroupSyncService = TabGroupSyncServiceFactory.getForProfile(profile);
        if (tabGroupSyncService == null) return;

        String iphText =
                context.getString(
                        R.string.collaboration_shared_tab_groups_available_again_iph_message);

        userEducationHelper.requestShowIph(
                new IphCommandBuilder(
                                anchorView.getResources(),
                                FeatureConstants.TAB_GROUP_SHARE_VERSION_UPDATE_FEATURE,
                                iphText,
                                iphText)
                        .setAnchorView(anchorView)
                        .setHighlightParams(new HighlightParams(HighlightShape.CIRCLE))
                        .setOnShowCallback(
                                () ->
                                        versioningMessageController.onMessageUiShown(
                                                MessageType.VERSION_UPDATED_MESSAGE))
                        .build());
    }

    private static boolean wontShowIphForProfile(@Nullable Profile profile) {
        return profile == null || profile.isOffTheRecord();
    }

    private static @Nullable VersioningMessageController getVersioningMessageController(
            Profile profile) {
        TabGroupSyncService tabGroupSyncService = TabGroupSyncServiceFactory.getForProfile(profile);
        assert tabGroupSyncService != null;
        return tabGroupSyncService.getVersioningMessageController();
    }

    private static boolean shouldShowIph(VersioningMessageController controller) {
        return controller.isInitialized()
                && controller.shouldShowMessageUi(MessageType.VERSION_UPDATED_MESSAGE);
    }
}
