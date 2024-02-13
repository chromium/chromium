// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.Context;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.infobar.IPHInfoBarSupport.PopupState;
import org.chromium.chrome.browser.infobar.IPHInfoBarSupport.TrackerParameters;
import org.chromium.chrome.browser.permissions.PermissionSettingsBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

/**
 * Default implementation of {@link IPHInfoBarSupport.IPHBubbleDelegate} that handles interacting
 * with {@link Tracker} and creating a {@link TextBubble} based on the type of infobar
 * shown.
 */
class IPHBubbleDelegateImpl implements IPHInfoBarSupport.IPHBubbleDelegate {
    private final Context mContext;
    private final Tracker mTracker;
    private final Tab mTab;

    IPHBubbleDelegateImpl(Context context, Tab tab) {
        mContext = context;
        mTracker = TrackerFactory.getTrackerForProfile(tab.getProfile());
        mTab = tab;
    }

    // IPHInfoBarSupport.IPHBubbleDelegate implementation.
    @Override
    public PopupState createStateForInfoBar(View anchorView, @InfoBarIdentifier int infoBarId) {
        TrackerParameters params = getTrackerParameters(infoBarId);
        if (params == null || !mTracker.shouldTriggerHelpUI(params.feature)) return null;

        PopupState state = new PopupState();
        state.view = anchorView;
        state.feature = params.feature;
        state.bubble =
                new TextBubble(
                        mContext,
                        anchorView,
                        params.textId,
                        params.accessibilityTextId,
                        anchorView,
                        ChromeAccessibilityUtil.get().isAccessibilityEnabled());
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
                return null;
            case InfoBarIdentifier.PERMISSION_INFOBAR_DELEGATE_ANDROID:
                if (PermissionSettingsBridge.shouldShowNotificationsPromo(mTab.getWebContents())) {
                    PermissionSettingsBridge.didShowNotificationsPromo(mTab.getProfile());
                    return new IPHInfoBarSupport.TrackerParameters(
                            FeatureConstants.QUIET_NOTIFICATION_PROMPTS_FEATURE,
                            R.string.notifications_iph,
                            R.string.notifications_iph);
                }
                return null;
            default:
                return null;
        }
    }
}
