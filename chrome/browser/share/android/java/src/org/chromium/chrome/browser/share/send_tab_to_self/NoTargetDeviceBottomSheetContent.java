// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** Content shown if the send-tab-to-self feature is ready but there are no target devices. */
public class NoTargetDeviceBottomSheetContent implements BottomSheetContent {
    private final View mContentView;

    public NoTargetDeviceBottomSheetContent(Context context) {
        this(context, ChromeFeatureList.isEnabled(ChromeFeatureList.SEND_TAB_TO_SELF_SIGNIN_PROMO));
    }

    /** Exposed so tests don't call ChromeFeatureList.isEnabled(), which requires native. */
    @VisibleForTesting
    public NoTargetDeviceBottomSheetContent(Context context, boolean isPromoFeatureEnabled) {
        mContentView = (ViewGroup) LayoutInflater.from(context).inflate(
                R.layout.send_tab_to_self_feature_unavailable_prompt, null);
        if (isPromoFeatureEnabled) {
            TextView label = (TextView) mContentView.findViewById(R.id.empty_state_label);
            label.setText(R.string.send_tab_to_self_android_no_target_device_label);
            mContentView.findViewById(R.id.manage_account_devices_link).setVisibility(View.VISIBLE);
        }
        // TODO(crbug.com/1298185): This is cumulating both signed-out and single device users.
        // Those should be recorded separately instead.
        RecordUserAction.record("SharingHubAndroid.SendTabToSelf.NoTargetDevices");
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {}

    @Override
    public int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        // This ensures that the bottom sheet reappears after the first time. Otherwise, the
        // second time that a user initiates a share, the bottom sheet does not re-appear.
        return true;
    }

    @Override
    public int getPeekHeight() {
        // Return DISABLED to ensure that the entire bottom sheet is shown.
        return BottomSheetContent.HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        // Return WRAP_CONTENT to have the bottom sheet only open as far as it needs to display the
        // list of devices and nothing beyond that.
        return BottomSheetContent.HeightMode.WRAP_CONTENT;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.send_tab_to_self_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.send_tab_to_self_sheet_half_height;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.send_tab_to_self_sheet_full_height;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.send_tab_to_self_sheet_closed;
    }
}
