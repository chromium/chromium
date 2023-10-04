// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.settings.SettingsLauncher;

/** Bottom sheet view for displaying the Privacy Sandbox notice. */
public class PrivacySandboxBottomSheetNotice implements BottomSheetContent {
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mBottomSheetObserver;
    private final Context mContext;
    private final SettingsLauncher mSettingsLauncher;

    private boolean mOpenedSettings;
    private View mContentView;

    PrivacySandboxBottomSheetNotice(Context context, BottomSheetController bottomSheetController,
            SettingsLauncher settingsLauncher) {
        mBottomSheetController = bottomSheetController;
        mContext = context;
        mSettingsLauncher = settingsLauncher;
        mOpenedSettings = false;

        mContentView = LayoutInflater.from(context).inflate(
                R.layout.privacy_sandbox_notice_bottom_sheet, null);

        mBottomSheetObserver = new EmptyBottomSheetObserver() {
            @Override
            public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                if (mOpenedSettings) {
                    // Action already recorded.
                    return;
                }
                if (reason == BottomSheetController.StateChangeReason.TAP_SCRIM
                        || reason == BottomSheetController.StateChangeReason.BACK_PRESS
                        || reason == BottomSheetController.StateChangeReason.INTERACTION_COMPLETE) {
                    PrivacySandboxBridge.promptActionOccurred(PromptAction.NOTICE_ACKNOWLEDGE);
                } else {
                    // The sheet was closed by a non-user action.
                    PrivacySandboxBridge.promptActionOccurred(
                            PromptAction.NOTICE_CLOSED_NO_INTERACTION);
                }
            }
        };

        TextView description =
                mContentView.findViewById(R.id.privacy_sandbox_notice_sheet_description);

        description.setMovementMethod(LinkMovementMethod.getInstance());

        View ackButton = mContentView.findViewById(R.id.ack_button);
        ackButton.setOnClickListener((v) -> {
            mBottomSheetController.hideContent(this, /* animate= */ true,
                    BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);
        });
        View settingsButton = mContentView.findViewById(R.id.settings_button);
        settingsButton.setOnClickListener((v) -> {
            mOpenedSettings = true;
            PrivacySandboxBridge.promptActionOccurred(PromptAction.NOTICE_OPEN_SETTINGS);
            mBottomSheetController.hideContent(this, /* animate= */ true,
                    BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);
            PrivacySandboxSettingsBaseFragment.launchPrivacySandboxSettings(
                    mContext, mSettingsLauncher, PrivacySandboxReferrer.PRIVACY_SANDBOX_NOTICE);
        });
    }

    public void showNotice(boolean animate) {
        // Reset whether the user opened settings.
        mOpenedSettings = false;
        if (!mBottomSheetController.requestShowContent(this, animate)) {
            mBottomSheetController.hideContent(
                    this, /* animate= */ false, BottomSheetController.StateChangeReason.NONE);
            destroy();
            return;
        }
        PrivacySandboxBridge.promptActionOccurred(PromptAction.NOTICE_SHOWN);
        mBottomSheetController.addObserver(mBottomSheetObserver);
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
    public void destroy() {
        mBottomSheetController.removeObserver(mBottomSheetObserver);
    }

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
    public boolean hasCustomLifecycle() {
        // Declare a custom lifecycle to prevent the bottom sheet from being
        // dismissed by the start screen.
        return true;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.privacy_sandbox_notice_sheet_title;
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
