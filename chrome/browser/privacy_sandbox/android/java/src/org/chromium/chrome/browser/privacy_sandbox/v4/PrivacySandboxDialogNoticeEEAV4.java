// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox.v4;

import android.app.Dialog;
import android.content.Context;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridge;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxReferrer;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxSettingsBaseFragment;
import org.chromium.chrome.browser.privacy_sandbox.PromptAction;
import org.chromium.chrome.browser.privacy_sandbox.R;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.CheckableImageView;

/**
 * Dialog in the form of a notice shown for the Privacy Sandbox.
 */
public class PrivacySandboxDialogNoticeEEAV4 extends Dialog implements View.OnClickListener {
    private SettingsLauncher mSettingsLauncher;
    private View mContentView;

    private final CheckableImageView mExpandArrowView;
    private LinearLayout mDropdownContainer;

    public PrivacySandboxDialogNoticeEEAV4(
            Context context, @NonNull SettingsLauncher settingsLauncher) {
        super(context, R.style.ThemeOverlay_BrowserUI_Fullscreen);

        mSettingsLauncher = settingsLauncher;
        mContentView =
                LayoutInflater.from(context).inflate(R.layout.privacy_sandbox_notice_eea_v4, null);
        setContentView(mContentView);

        ButtonCompat ackButton = mContentView.findViewById(R.id.ack_button);
        ackButton.setOnClickListener(this);
        ButtonCompat settingsButton = mContentView.findViewById(R.id.settings_button);
        settingsButton.setOnClickListener(this);

        // Controls for the expanding section.
        LinearLayout dropdownElement = mContentView.findViewById(R.id.dropdown_element);
        dropdownElement.setOnClickListener(this);
        mDropdownContainer = mContentView.findViewById(R.id.dropdown_container);
        mExpandArrowView = mContentView.findViewById(R.id.expand_arrow);
        mExpandArrowView.setImageDrawable(PrivacySandboxDialogUtils.createExpandDrawable(context));
        mExpandArrowView.setChecked(isDropdownExpanded());

        setBulletsDescription();
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
            PrivacySandboxBridge.promptActionOccurred(PromptAction.NOTICE_ACKNOWLEDGE);
            dismiss();
        } else if (id == R.id.settings_button) {
            PrivacySandboxBridge.promptActionOccurred(PromptAction.NOTICE_OPEN_SETTINGS);
            dismiss();
            // TODO(b/254408752): Update the referrer.
            PrivacySandboxSettingsBaseFragment.launchPrivacySandboxSettings(
                    getContext(), mSettingsLauncher, PrivacySandboxReferrer.PRIVACY_SANDBOX_NOTICE);
        } else if (id == R.id.dropdown_element) {
            var content = mContentView.findViewById(R.id.privacy_sandbox_notice_eea_content);
            ScrollView scrollView =
                    mContentView.findViewById(R.id.privacy_sandbox_notice_eea_scroll_view);

            if (isDropdownExpanded()) {
                PrivacySandboxBridge.promptActionOccurred(PromptAction.NOTICE_MORE_INFO_CLOSED);
                mDropdownContainer.setVisibility(View.GONE);
                mDropdownContainer.removeAllViews();

                ((FrameLayout.LayoutParams) content.getLayoutParams()).gravity =
                        Gravity.CENTER_VERTICAL;
            } else {
                mDropdownContainer.setVisibility(View.VISIBLE);
                PrivacySandboxBridge.promptActionOccurred(PromptAction.NOTICE_MORE_INFO_OPENED);
                LayoutInflater.from(getContext())
                        .inflate(R.layout.privacy_sandbox_notice_eea_dropdown_v4,
                                mDropdownContainer);

                PrivacySandboxDialogUtils.setBulletTextWithBoldContent(getContext(),
                        mDropdownContainer,
                        R.id.privacy_sandbox_m1_notice_eea_learn_more_bullet_one,
                        R.string.privacy_sandbox_m1_notice_eea_learn_more_bullet_1);
                PrivacySandboxDialogUtils.setBulletTextWithBoldContent(getContext(),
                        mDropdownContainer,
                        R.id.privacy_sandbox_m1_notice_eea_learn_more_bullet_two,
                        R.string.privacy_sandbox_m1_notice_eea_learn_more_bullet_2);
                PrivacySandboxDialogUtils.setBulletTextWithBoldContent(getContext(),
                        mDropdownContainer,
                        R.id.privacy_sandbox_m1_notice_eea_learn_more_bullet_three,
                        R.string.privacy_sandbox_m1_notice_eea_learn_more_bullet_3);

                ((FrameLayout.LayoutParams) content.getLayoutParams()).gravity = Gravity.TOP;

                scrollView.post(() -> {
                    scrollView.setSmoothScrollingEnabled(true);
                    scrollView.fullScroll(ScrollView.FOCUS_DOWN);
                });
            }

            mExpandArrowView.setChecked(isDropdownExpanded());
            PrivacySandboxDialogUtils.updateDropdownControlContentDescription(getContext(), view,
                    isDropdownExpanded(),
                    R.string.privacy_sandbox_m1_notice_eea_learn_more_expand_label);
            view.announceForAccessibility(getContext().getResources().getString(isDropdownExpanded()
                            ? R.string.accessibility_expanded_group
                            : R.string.accessibility_collapsed_group));
        }
    }

    private void setBulletsDescription() {
        PrivacySandboxDialogUtils.setBulletText(getContext(), mContentView,
                R.id.privacy_sandbox_m1_notice_eea_bullet_one,
                R.string.privacy_sandbox_m1_notice_eea_bullet_1);
        PrivacySandboxDialogUtils.setBulletText(getContext(), mContentView,
                R.id.privacy_sandbox_m1_notice_eea_bullet_two,
                R.string.privacy_sandbox_m1_notice_eea_bullet_2);
    }

    private boolean isDropdownExpanded() {
        return mDropdownContainer != null && mDropdownContainer.getVisibility() == View.VISIBLE;
    }
}
