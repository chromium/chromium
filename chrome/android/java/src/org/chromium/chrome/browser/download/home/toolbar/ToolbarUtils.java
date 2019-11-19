// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.toolbar;

import android.support.v4.view.ViewCompat;
import android.view.View;

import org.chromium.chrome.browser.download.DirectoryOption;
import org.chromium.chrome.browser.download.DownloadDirectoryProvider;
import org.chromium.chrome.browser.ui.widget.highlight.ViewHighlighter;
import org.chromium.chrome.browser.ui.widget.textbubble.TextBubble;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.ArrayList;

/**
 * Utility methods for download home toolbar.
 */
public class ToolbarUtils {
    /**
     * Sets up feature engagement tracker for the download settings in-product help text bubble.
     * @param tracker The {@link Tracker} to use for the in-product help.
     * @param toolbar The toolbar that contains the settings menu.
     */
    public static void setupTrackerForDownloadSettingsIPH(Tracker tracker, View toolbar) {
        tracker.addOnInitializedCallback(
                success -> ToolbarUtils.maybeShowDownloadSettingsTextBubble(tracker, toolbar));
    }

    private static void maybeShowDownloadSettingsTextBubble(Tracker tracker, View toolbar) {
        // If the user doesn't have an SD card don't show the IPH.
        DownloadDirectoryProvider.getInstance().getAllDirectoriesOptions(
                dirs -> { onDirectoryOptionsRetrieved(dirs, tracker, toolbar); });
    }

    private static void onDirectoryOptionsRetrieved(
            ArrayList<DirectoryOption> dirs, Tracker tracker, View rootView) {
        if (dirs.size() < 2) return;

        // Check to see if the help UI should be triggered.
        if (!tracker.shouldTriggerHelpUI(FeatureConstants.DOWNLOAD_SETTINGS_FEATURE)) return;

        // Build and show text bubble.
        View anchorView = rootView.findViewById(org.chromium.chrome.download.R.id.settings_menu_id);

        // Show the setting text bubble after the root view is attached to window.
        if (ViewCompat.isAttachedToWindow(rootView)) {
            showDownloadSettingsInProductHelp(tracker, anchorView, rootView);
        } else {
            rootView.addOnAttachStateChangeListener(new View.OnAttachStateChangeListener() {
                @Override
                public void onViewAttachedToWindow(View v) {
                    showDownloadSettingsInProductHelp(tracker, anchorView, rootView);
                    rootView.removeOnAttachStateChangeListener(this);
                }
                @Override
                public void onViewDetachedFromWindow(View v) {}
            });
        }
    }

    private static void showDownloadSettingsInProductHelp(
            Tracker tracker, View anchorView, View rootView) {
        TextBubble textBubble = new TextBubble(rootView.getContext(), rootView,
                org.chromium.chrome.download.R.string.iph_download_settings_text,
                org.chromium.chrome.download.R.string.iph_download_settings_accessibility_text,
                new ViewRectProvider(anchorView));
        textBubble.setDismissOnTouchInteraction(true);
        textBubble.addOnDismissListener(() -> {
            tracker.dismissed(FeatureConstants.DOWNLOAD_SETTINGS_FEATURE);
            toggleHighlightForDownloadSettingsTextBubble(anchorView, false);
        });
        toggleHighlightForDownloadSettingsTextBubble(anchorView, true);
        textBubble.show();
    }

    private static void toggleHighlightForDownloadSettingsTextBubble(
            View anchorView, boolean shouldHighlight) {
        if (shouldHighlight) {
            ViewHighlighter.turnOnHighlight(anchorView, true);
        } else {
            ViewHighlighter.turnOffHighlight(anchorView);
        }
    }
}
