// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox.v4;

import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridge;
import org.chromium.chrome.browser.privacy_sandbox.PromptAction;
import org.chromium.chrome.browser.privacy_sandbox.R;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.ui.widget.ButtonCompat;

/**
 * Dialog in the form of a notice shown for the Privacy Sandbox.
 */
public class PrivacySandboxDialogNoticeRestrictedV4
        extends Dialog implements View.OnClickListener, DialogInterface.OnShowListener {
    private SettingsLauncher mSettingsLauncher;
    private View mContentView;

    private ButtonCompat mMoreButton;
    private LinearLayout mActionButtons;
    private ScrollView mScrollView;

    public PrivacySandboxDialogNoticeRestrictedV4(
            Context context, @NonNull SettingsLauncher settingsLauncher) {
        super(context, R.style.ThemeOverlay_BrowserUI_Fullscreen);
        mSettingsLauncher = settingsLauncher;
        mContentView = LayoutInflater.from(context).inflate(
                R.layout.privacy_sandbox_notice_restricted_v4, null);
        setContentView(mContentView);

        ButtonCompat ackButton = mContentView.findViewById(R.id.ack_button);
        ackButton.setOnClickListener(this);
        ButtonCompat settingsButton = mContentView.findViewById(R.id.settings_button);
        settingsButton.setOnClickListener(this);

        mMoreButton = mContentView.findViewById(R.id.more_button);
        mActionButtons = mContentView.findViewById(R.id.action_buttons);
        mScrollView = mContentView.findViewById(R.id.privacy_sandbox_dialog_scroll_view);

        mMoreButton.setOnClickListener(this);
        setOnShowListener(this);
        setCancelable(false);

        mScrollView.getViewTreeObserver().addOnScrollChangedListener(() -> {
            if (!mScrollView.canScrollVertically(ScrollView.FOCUS_DOWN)) {
                mMoreButton.setVisibility(View.GONE);
                mActionButtons.setVisibility(View.VISIBLE);
                mScrollView.post(() -> { mScrollView.pageScroll(ScrollView.FOCUS_DOWN); });
            }
        });
    }

    @Override
    public void show() {
        PrivacySandboxBridge.promptActionOccurred(PromptAction.NOTICE_SHOWN);
        super.show();
    }

    // OnClickListener:
    @Override
    public void onClick(View view) {
        int id = view.getId();
        if (id == R.id.ack_button) {
            PrivacySandboxBridge.promptActionOccurred(PromptAction.RESTRICTED_NOTICE_ACKNOWLEDGE);
            dismiss();
        } else if (id == R.id.settings_button) {
            PrivacySandboxBridge.promptActionOccurred(PromptAction.RESTRICTED_NOTICE_OPEN_SETTINGS);
            dismiss();
            mSettingsLauncher.launchSettingsActivity(getContext(), AdMeasurementFragmentV4.class);
        } else if (id == R.id.more_button) {
            PrivacySandboxBridge.promptActionOccurred(PromptAction.NOTICE_MORE_BUTTON_CLICKED);
            if (mScrollView.canScrollVertically(ScrollView.FOCUS_DOWN)) {
                mScrollView.post(() -> { mScrollView.pageScroll(ScrollView.FOCUS_DOWN); });
            } else {
                mMoreButton.setVisibility(View.GONE);
                mActionButtons.setVisibility(View.VISIBLE);
                mScrollView.post(() -> { mScrollView.pageScroll(ScrollView.FOCUS_DOWN); });
            }
        }
    }

    @Override
    public void onShow(DialogInterface dialogInterface) {
        if (mScrollView.canScrollVertically(ScrollView.FOCUS_DOWN)) {
            mMoreButton.setVisibility(View.VISIBLE);
            mActionButtons.setVisibility(View.GONE);
        } else {
            mMoreButton.setVisibility(View.GONE);
            mActionButtons.setVisibility(View.VISIBLE);
        }
        mScrollView.setVisibility(View.VISIBLE);
    }
}
