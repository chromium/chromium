// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.settings.SettingsLauncher;

/** Bottom sheet view for displaying the Privacy Sandbox notice. */
public class PrivacySandboxBottomSheetNotice implements BottomSheetContent {
    private final BottomSheetController mBottomSheetController;
    private final Context mContext;
    private final SettingsLauncher mSettingsLauncher;

    private View mContentView;

    PrivacySandboxBottomSheetNotice(Context context, BottomSheetController bottomSheetController,
            SettingsLauncher settingsLauncher) {
        mBottomSheetController = bottomSheetController;
        mContext = context;
        mSettingsLauncher = settingsLauncher;

        mContentView = LayoutInflater.from(context).inflate(
                R.layout.privacy_sandbox_notice_bottom_sheet, null);

        View ackButton = mContentView.findViewById(R.id.ack_button);
        ackButton.setOnClickListener((v) -> {
            PrivacySandboxBridge.promptActionOccurred(PromptAction.NOTICE_ACKNOWLEDGE);
            mBottomSheetController.hideContent(this, /* animate= */ true,
                    BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);
        });
        View settingsButton = mContentView.findViewById(R.id.settings_button);
        settingsButton.setOnClickListener((v) -> {
            PrivacySandboxBridge.promptActionOccurred(PromptAction.NOTICE_OPEN_SETTINGS);
            mBottomSheetController.hideContent(this, /* animate= */ true,
                    BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);
            PrivacySandboxSettingsFragmentV3.launchPrivacySandboxSettings(
                    mContext, mSettingsLauncher, PrivacySandboxReferrer.PRIVACY_SANDBOX_NOTICE);
        });
    }

    // BottomSheetContent implementation.

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Nullable
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
    public int getPeekHeight() {
        return BottomSheetContent.HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return BottomSheetContent.HeightMode.WRAP_CONTENT;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.privacy_sandbox_notice_sheet_content_description;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.privacy_sandbox_notice_sheet_closed_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.privacy_sandbox_notice_sheet_opened_half;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.privacy_sandbox_notice_sheet_opened_full;
    }
}
