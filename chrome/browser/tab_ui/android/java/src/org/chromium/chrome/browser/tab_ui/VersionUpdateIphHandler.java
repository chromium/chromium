// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import static android.text.TextUtils.isEmpty;

import static org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils.getDisplayableTitle;

import android.content.Context;
import android.view.View;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.MessageType;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.VersioningMessageController;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

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
     * @param filter The current tab group model filter.
     * @param expectsAutoOpen The expected value of the "auto-open tab groups" setting. The IPH will
     *     only be shown if the setting's actual value matches this expected value.
     */
    public static void maybeShowVersioningIph(
            UserEducationHelper userEducationHelper,
            View anchorView,
            TabGroupModelFilter filter,
            boolean expectsAutoOpen) {
        Profile profile = getProfile(filter);
        if (profile == null || expectsAutoOpen != isAutoOpenEnabled(profile)) return;
        showIph(userEducationHelper, anchorView, profile, filter);
    }

    private static void showIph(
            UserEducationHelper userEducationHelper,
            View anchorView,
            Profile profile,
            TabGroupModelFilter filter) {
        VersioningMessageController versioningMessageController =
                getVersioningMessageController(profile);
        if (versioningMessageController == null || shouldNotShowIph(versioningMessageController)) {
            return;
        }

        Context context = anchorView.getContext();
        List<String> sharedGroupTitles = new ArrayList<>();

        TabGroupSyncService tabGroupSyncService = TabGroupSyncServiceFactory.getForProfile(profile);
        if (tabGroupSyncService == null) return;

        Set<Token> allTabGroupIds = filter.getAllTabGroupIds();
        if (allTabGroupIds.isEmpty()) return;

        for (Token groupId : allTabGroupIds) {
            SavedTabGroup group = tabGroupSyncService.getGroup(new LocalTabGroupId(groupId));
            if (group == null || isEmpty(group.collaborationId)) continue;
            sharedGroupTitles.add(getDisplayableTitle(context, filter, groupId));
            break;
        }

        if (sharedGroupTitles.isEmpty()) return;
        sharedGroupTitles.size();
        String iphText =
                context.getString(R.string.tab_group_update_iph_text, sharedGroupTitles.get(0));

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

    @Nullable
    private static Profile getProfile(TabGroupModelFilter filter) {
        Profile profile = filter.getTabModel().getProfile();
        if (wontShowIphForProfile(profile)) return null;
        return profile;
    }

    private static @Nullable VersioningMessageController getVersioningMessageController(
            Profile profile) {
        TabGroupSyncService tabGroupSyncService = TabGroupSyncServiceFactory.getForProfile(profile);
        assert tabGroupSyncService != null;
        return tabGroupSyncService.getVersioningMessageController();
    }

    private static boolean shouldNotShowIph(VersioningMessageController controller) {
        return !controller.isInitialized()
                || !controller.shouldShowMessageUi(MessageType.VERSION_UPDATED_MESSAGE);
    }

    private static boolean isAutoOpenEnabled(Profile profile) {
        PrefService prefService = UserPrefs.get(profile);
        return prefService.getBoolean(Pref.AUTO_OPEN_SYNCED_TAB_GROUPS);
    }
}
