// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox.v4;

import android.app.Dialog;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridge;
import org.chromium.chrome.browser.privacy_sandbox.PromptAction;
import org.chromium.chrome.browser.privacy_sandbox.R;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.CheckableImageView;

/**
 * Dialog in the form of a consent shown for the Privacy Sandbox.
 */
public class PrivacySandboxDialogConsentEEAV4 extends Dialog implements View.OnClickListener {
    private static final int SPINNER_DURATION_MS = 1500;

    private View mContentView;

    private final CheckableImageView mExpandArrowView;
    private LinearLayout mDropdownContainer;
    private LinearLayout mDropdownElement;
    private LinearLayout mProgressBarContainer;
    private LinearLayout mConsentViewContainer;
    private boolean mAreAnimationsDisabled;

    public PrivacySandboxDialogConsentEEAV4(Context context, boolean disableAnimations) {
        super(context, R.style.ThemeOverlay_BrowserUI_Fullscreen);
        mAreAnimationsDisabled = disableAnimations;
        mContentView =
                LayoutInflater.from(context).inflate(R.layout.privacy_sandbox_consent_eea_v4, null);
        setContentView(mContentView);

        ButtonCompat ackButton = mContentView.findViewById(R.id.ack_button);
        ackButton.setOnClickListener(this);
        ButtonCompat noButton = mContentView.findViewById(R.id.no_button);
        noButton.setOnClickListener(this);

        mProgressBarContainer = mContentView.findViewById(R.id.progress_bar_container);
        mConsentViewContainer = mContentView.findViewById(R.id.privacy_sandbox_consent_eea_view);

        // Controls for the expanding section.
        mDropdownElement = mContentView.findViewById(R.id.dropdown_element);
        mDropdownElement.setOnClickListener(this);
        mDropdownContainer = mContentView.findViewById(R.id.dropdown_container);
        mExpandArrowView = mContentView.findViewById(R.id.expand_arrow);
        mExpandArrowView.setImageDrawable(PrivacySandboxDialogUtils.createExpandDrawable(context));
        mExpandArrowView.setChecked(isDropdownExpanded());
    }

    @Override
    public void show() {
        PrivacySandboxBridge.promptActionOccurred(PromptAction.CONSENT_SHOWN);
        super.show();
    }

    // OnClickListener:
    @Override
    public void onClick(View view) {
        int id = view.getId();
        if (id == R.id.ack_button) {
            PrivacySandboxBridge.promptActionOccurred(PromptAction.CONSENT_ACCEPTED);
            showSavingConfirmationAndDismiss();
        } else if (id == R.id.no_button) {
            PrivacySandboxBridge.promptActionOccurred(PromptAction.CONSENT_DECLINED);
            showSavingConfirmationAndDismiss();
        } else if (id == R.id.dropdown_element) {
            ScrollView scrollView =
                    mContentView.findViewById(R.id.privacy_sandbox_consent_eea_scroll_view);

            if (isDropdownExpanded()) {
                PrivacySandboxBridge.promptActionOccurred(PromptAction.CONSENT_MORE_INFO_CLOSED);
                mDropdownContainer.setVisibility(View.GONE);
                mDropdownContainer.removeAllViews();
                scrollView.post(() -> { scrollView.fullScroll(ScrollView.FOCUS_UP); });
            } else {
                mDropdownContainer.setVisibility(View.VISIBLE);
                PrivacySandboxBridge.promptActionOccurred(PromptAction.CONSENT_MORE_INFO_OPENED);
                LayoutInflater.from(getContext())
                        .inflate(R.layout.privacy_sandbox_consent_eea_dropdown_v4,
                                mDropdownContainer);

                PrivacySandboxDialogUtils.setBulletTextWithBoldContent(getContext(),
                        mDropdownContainer, R.id.privacy_sandbox_m1_consent_learn_more_bullet_one,
                        R.string.privacy_sandbox_m1_consent_learn_more_bullet_1);
                PrivacySandboxDialogUtils.setBulletTextWithBoldContent(getContext(),
                        mDropdownContainer, R.id.privacy_sandbox_m1_consent_learn_more_bullet_two,
                        R.string.privacy_sandbox_m1_consent_learn_more_bullet_2);
                PrivacySandboxDialogUtils.setBulletTextWithBoldContent(getContext(),
                        mDropdownContainer, R.id.privacy_sandbox_m1_consent_learn_more_bullet_three,
                        R.string.privacy_sandbox_m1_consent_learn_more_bullet_3);

                scrollView.post(() -> { scrollView.scrollTo(0, mDropdownElement.getTop()); });
            }

            mExpandArrowView.setChecked(isDropdownExpanded());
            PrivacySandboxDialogUtils.updateDropdownControlContentDescription(getContext(), view,
                    isDropdownExpanded(),
                    R.string.privacy_sandbox_m1_consent_learn_more_expand_label);
            view.announceForAccessibility(getContext().getResources().getString(isDropdownExpanded()
                            ? R.string.accessibility_expanded_group
                            : R.string.accessibility_collapsed_group));
        }
    }

    private void showSavingConfirmationAndDismiss() {
        mProgressBarContainer.setVisibility(View.VISIBLE);
        mConsentViewContainer.setVisibility(View.GONE);
        PostTask.postDelayedTask(TaskTraits.USER_BLOCKING, this::dismiss, getSpinnerDuration());
    }

    private boolean isDropdownExpanded() {
        return mDropdownContainer != null && mDropdownContainer.getVisibility() == View.VISIBLE;
    }

    private long getSpinnerDuration() {
        return mAreAnimationsDisabled ? 0 : SPINNER_DURATION_MS;
    }
}
