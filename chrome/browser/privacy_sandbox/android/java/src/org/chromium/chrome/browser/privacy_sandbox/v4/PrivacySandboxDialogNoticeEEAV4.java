// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox.v4;

import android.content.Context;
import android.content.DialogInterface;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridge;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxReferrer;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxSettingsBaseFragment;
import org.chromium.chrome.browser.privacy_sandbox.PromptAction;
import org.chromium.chrome.browser.privacy_sandbox.R;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.widget.ChromeDialog;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.CheckableImageView;

/**
 * Dialog in the form of a notice shown for the Privacy Sandbox.
 */
public class PrivacySandboxDialogNoticeEEAV4
        extends ChromeDialog implements View.OnClickListener, DialogInterface.OnShowListener {
    private SettingsLauncher mSettingsLauncher;
    private View mContentView;

    private ButtonCompat mMoreButton;
    private LinearLayout mActionButtons;
    private ScrollView mScrollView;
    private LinearLayout mDropdownElement;

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

        mMoreButton = mContentView.findViewById(R.id.more_button);
        mActionButtons = mContentView.findViewById(R.id.action_buttons);
        mScrollView = mContentView.findViewById(R.id.privacy_sandbox_dialog_scroll_view);

        // Controls for the expanding section.
        mDropdownElement = mContentView.findViewById(R.id.dropdown_element);
        mDropdownElement.setOnClickListener(this);
        mDropdownContainer = mContentView.findViewById(R.id.dropdown_container);
        mExpandArrowView = mContentView.findViewById(R.id.expand_arrow);
        mExpandArrowView.setImageDrawable(PrivacySandboxDialogUtils.createExpandDrawable(context));
        mExpandArrowView.setChecked(isDropdownExpanded());

        setBulletsDescription();

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
            PrivacySandboxBridge.promptActionOccurred(PromptAction.NOTICE_ACKNOWLEDGE);
            dismiss();
        } else if (id == R.id.settings_button) {
            PrivacySandboxBridge.promptActionOccurred(PromptAction.NOTICE_OPEN_SETTINGS);
            dismiss();
            PrivacySandboxSettingsBaseFragment.launchPrivacySandboxSettings(
                    getContext(), mSettingsLauncher, PrivacySandboxReferrer.PRIVACY_SANDBOX_NOTICE);
        } else if (id == R.id.more_button) {
            PrivacySandboxBridge.promptActionOccurred(PromptAction.NOTICE_MORE_BUTTON_CLICKED);
            if (mScrollView.canScrollVertically(ScrollView.FOCUS_DOWN)) {
                mScrollView.post(() -> { mScrollView.pageScroll(ScrollView.FOCUS_DOWN); });
            } else {
                mMoreButton.setVisibility(View.GONE);
                mActionButtons.setVisibility(View.VISIBLE);
                mScrollView.post(() -> { mScrollView.pageScroll(ScrollView.FOCUS_DOWN); });
            }
        } else if (id == R.id.dropdown_element) {
            if (isDropdownExpanded()) {
                PrivacySandboxBridge.promptActionOccurred(PromptAction.NOTICE_MORE_INFO_CLOSED);
                mDropdownContainer.setVisibility(View.GONE);
                mDropdownContainer.removeAllViews();
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

                mScrollView.post(() -> { mScrollView.scrollTo(0, mDropdownElement.getTop()); });
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
