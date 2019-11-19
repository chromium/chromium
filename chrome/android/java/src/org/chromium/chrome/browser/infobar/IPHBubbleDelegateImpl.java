// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.Context;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.download.DownloadInfoBarController;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.infobar.IPHInfoBarSupport.PopupState;
import org.chromium.chrome.browser.infobar.IPHInfoBarSupport.TrackerParameters;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.widget.textbubble.TextBubble;
import org.chromium.components.feature_engagement.Tracker;

/**
 * Default implementation of {@link IPHInfoBarSupport.IPHBubbleDelegate} that handles interacting
 * with {@link Tracker} and creating a {@link TextBubble} based on the type of infobar
 * shown.
 */
class IPHBubbleDelegateImpl implements IPHInfoBarSupport.IPHBubbleDelegate {
    private final Context mContext;
    private final Tracker mTracker;

    IPHBubbleDelegateImpl(Context context) {
        mContext = context;
        Profile profile = Profile.getLastUsedProfile();
        mTracker = TrackerFactory.getTrackerForProfile(profile);
    }

    // IPHInfoBarSupport.IPHBubbleDelegate implementation.
    @Override
    public PopupState createStateForInfoBar(View anchorView, @InfoBarIdentifier int infoBarId) {
        TrackerParameters params = getTrackerParameters(infoBarId);
        if (params == null || !mTracker.shouldTriggerHelpUI(params.feature)) return null;

        PopupState state = new PopupState();
        state.view = anchorView;
        state.feature = params.feature;
        state.bubble = new TextBubble(
                mContext, anchorView, params.textId, params.accessibilityTextId, anchorView);
        state.bubble.setDismissOnTouchInteraction(true);

        return state;
    }

    @Override
    public void onPopupDismissed(PopupState state) {
        mTracker.dismissed(state.feature);
    }

    private @Nullable TrackerParameters getTrackerParameters(@InfoBarIdentifier int infoBarId) {
        switch (infoBarId) {
            case InfoBarIdentifier.DOWNLOAD_PROGRESS_INFOBAR_ANDROID:
                DownloadInfoBarController controller =
                        DownloadManagerService.getDownloadManagerService()
                        .getInfoBarController(Profile.getLastUsedProfile().isOffTheRecord());
                return controller != null ? controller.getTrackerParameters() : null;
            default:
                return null;
        }
    }
}
