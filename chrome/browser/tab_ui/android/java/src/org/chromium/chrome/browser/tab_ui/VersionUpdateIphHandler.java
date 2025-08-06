// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import static org.chromium.chrome.browser.tabmodel.TabGroupTitleUtils.getDisplayableTitle;
import static org.chromium.components.collaboration.messaging.MessageUtils.extractTabGroupId;

import android.content.Context;
import android.view.View;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.collaboration.messaging.MessagingBackendServiceFactory;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.collaboration.messaging.PersistentMessage;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.tab_group_sync.MessageType;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.VersioningMessageController;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.List;
import java.util.Optional;

/** Handles showing IPH when a user updates Chrome and regains access to a shared tab group. */
@NullMarked
public class VersionUpdateIphHandler {

    // Prevents instantiation.
    private VersionUpdateIphHandler() {}

    /**
     * Attempts to show IPH on the tab switcher button if conditions for a version update message
     * are met.
     *
     * @param userEducationHelper Helper for showing IPHs.
     * @param tabSwitcherButton The view to anchor the IPH to.
     * @param profile The current user profile.
     * @param info Information about the tab model dot (containing the group title).
     */
    public static void maybeShowTabSwitcherButtonIph(
            UserEducationHelper userEducationHelper,
            View tabSwitcherButton,
            Profile profile,
            TabModelDotInfo info) {
        if (wontShowIphForProfile(profile)) return;

        assert profile != null;
        if (!isAutoOpenEnabled(profile)) return;

        VersioningMessageController versioningMessageController =
                getVersioningMessageController(profile);
        if (shouldNotShowIph(versioningMessageController)) return;

        Context context = tabSwitcherButton.getContext();
        String contentString =
                context.getString(R.string.tab_group_update_iph_text, info.tabGroupTitle);
        showIph(userEducationHelper, tabSwitcherButton, contentString, versioningMessageController);
    }

    /**
     * Attempts to show IPH on the tab group pane button if conditions for a version update message
     * are met.
     *
     * @param userEducationHelper Helper for showing IPHs.
     * @param filter The current tab group model filter.
     * @param anchorView The view to anchor the IPH to within the tab group pane.
     */
    public static void maybeShowTabGroupPaneButtonIph(
            UserEducationHelper userEducationHelper, TabGroupModelFilter filter, View anchorView) {
        Profile profile = filter.getTabModel().getProfile();
        if (wontShowIphForProfile(profile)) return;

        assert profile != null;
        if (isAutoOpenEnabled(profile)) return;

        VersioningMessageController versioningMessageController =
                getVersioningMessageController(profile);
        if (shouldNotShowIph(versioningMessageController)) return;

        MessagingBackendService messagingBackendService =
                MessagingBackendServiceFactory.getForProfile(profile);
        assert messagingBackendService != null;

        List<PersistentMessage> messages =
                messagingBackendService.getMessages(
                        Optional.of(MessageType.VERSION_UPDATED_MESSAGE));
        if (messages.isEmpty()) return;

        Context context = anchorView.getContext();
        String tabGroupTitle = getTabGroupTitleFromMessages(context, filter, messages);
        if (tabGroupTitle == null) return;

        String iphText = context.getString(R.string.tab_group_update_iph_text, tabGroupTitle);
        showIph(userEducationHelper, anchorView, iphText, versioningMessageController);
    }

    private static boolean wontShowIphForProfile(@Nullable Profile profile) {
        return profile == null || profile.isOffTheRecord();
    }

    private static VersioningMessageController getVersioningMessageController(Profile profile) {
        TabGroupSyncService tabGroupSyncService = TabGroupSyncServiceFactory.getForProfile(profile);
        assert tabGroupSyncService != null;
        return tabGroupSyncService.getVersioningMessageController();
    }

    private static @Nullable String getTabGroupTitleFromMessages(
            Context context, TabGroupModelFilter filter, List<PersistentMessage> messages) {
        for (PersistentMessage message : messages) {
            Token tabGroupId = extractTabGroupId(message);
            if (tabGroupId != null) {
                String title = getDisplayableTitle(context, filter, tabGroupId);
                if (title != null) return title;
            }
        }
        return null;
    }

    private static void showIph(
            UserEducationHelper userEducationHelper,
            View anchorView,
            String contentString,
            VersioningMessageController versioningMessageController) {
        userEducationHelper.requestShowIph(
                new IphCommandBuilder(
                                anchorView.getResources(),
                                FeatureConstants.TAB_GROUP_SHARE_VERSION_UPDATE_FEATURE,
                                contentString,
                                contentString)
                        .setAnchorView(anchorView)
                        .setHighlightParams(new HighlightParams(HighlightShape.CIRCLE))
                        .build());

        versioningMessageController.onMessageUiShown(MessageType.VERSION_UPDATED_MESSAGE);
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
