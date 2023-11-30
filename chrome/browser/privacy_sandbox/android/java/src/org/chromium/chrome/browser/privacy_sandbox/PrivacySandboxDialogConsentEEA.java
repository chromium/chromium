// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.content.DialogInterface;
import android.os.Handler;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import androidx.annotation.NonNull;

import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.widget.ChromeDialog;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.CheckableImageView;

/** Dialog in the form of a consent shown for the Privacy Sandbox. */
public class PrivacySandboxDialogConsentEEA extends ChromeDialog
        implements View.OnClickListener, DialogInterface.OnShowListener {
    private static final int SPINNER_DURATION_MS = 1500;
    private static final int BACKGROUND_TRANSITION_DURATION_MS = 300;

    private View mContentView;

    private final CheckableImageView mExpandArrowView;
    private LinearLayout mDropdownContainer;
    private LinearLayout mDropdownElement;
    private LinearLayout mProgressBarContainer;
    private LinearLayout mConsentViewContainer;
    private ButtonCompat mMoreButton;
    private LinearLayout mActionButtons;
    private ScrollView mScrollView;

    private boolean mAreAnimationsDisabled;
    private SettingsLauncher mSettingsLauncher;

    public PrivacySandboxDialogConsentEEA(
            Context context,
            @NonNull SettingsLauncher settingsLauncher,
            boolean disableAnimations) {
        super(context, R.style.ThemeOverlay_BrowserUI_Fullscreen);
        mSettingsLauncher = settingsLauncher;
        mAreAnimationsDisabled = disableAnimations;
        mContentView =
                LayoutInflater.from(context).inflate(R.layout.privacy_sandbox_consent_eea, null);
        setContentView(mContentView);

        ButtonCompat ackButton = mContentView.findViewById(R.id.ack_button);
        ackButton.setOnClickListener(this);
        ButtonCompat noButton = mContentView.findViewById(R.id.no_button);
        noButton.setOnClickListener(this);
        mMoreButton = mContentView.findViewById(R.id.more_button);
        mActionButtons = mContentView.findViewById(R.id.action_buttons);
        mScrollView = mContentView.findViewById(R.id.privacy_sandbox_dialog_scroll_view);

        mProgressBarContainer = mContentView.findViewById(R.id.progress_bar_container);
        mConsentViewContainer = mContentView.findViewById(R.id.privacy_sandbox_consent_eea_view);

        // Controls for the expanding section.
        mDropdownElement = mContentView.findViewById(R.id.dropdown_element);
        mDropdownElement.setOnClickListener(this);
        mDropdownContainer = mContentView.findViewById(R.id.dropdown_container);
        mExpandArrowView = mContentView.findViewById(R.id.expand_arrow);
        mExpandArrowView.setImageDrawable(PrivacySandboxDialogUtils.createExpandDrawable(context));
        mExpandArrowView.setChecked(isDropdownExpanded());

        mMoreButton.setOnClickListener(this);
        setOnShowListener(this);
        setCancelable(false);

        mScrollView
                .getViewTreeObserver()
                .addOnScrollChangedListener(
                        () -> {
                            if (!mScrollView.canScrollVertically(ScrollView.FOCUS_DOWN)) {
                                mMoreButton.setVisibility(View.GONE);
                                mActionButtons.setVisibility(View.VISIBLE);
                                mScrollView.post(
                                        () -> {
                                            mScrollView.pageScroll(ScrollView.FOCUS_DOWN);
                                        });
                            }
                        });
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
            dismissAndMaybeShowNotice();
        } else if (id == R.id.no_button) {
            PrivacySandboxBridge.promptActionOccurred(PromptAction.CONSENT_DECLINED);
            dismissAndMaybeShowNotice();
        } else if (id == R.id.more_button) {
            PrivacySandboxBridge.promptActionOccurred(PromptAction.CONSENT_MORE_BUTTON_CLICKED);
            if (mScrollView.canScrollVertically(ScrollView.FOCUS_DOWN)) {
                mScrollView.post(
                        () -> {
                            mScrollView.pageScroll(ScrollView.FOCUS_DOWN);
                        });
            } else {
                mMoreButton.setVisibility(View.GONE);
                mActionButtons.setVisibility(View.VISIBLE);
                mScrollView.post(
                        () -> {
                            mScrollView.pageScroll(ScrollView.FOCUS_DOWN);
                        });
            }
        } else if (id == R.id.dropdown_element) {
            if (isDropdownExpanded()) {
                PrivacySandboxBridge.promptActionOccurred(PromptAction.CONSENT_MORE_INFO_CLOSED);
                mDropdownContainer.setVisibility(View.GONE);
                mDropdownContainer.removeAllViews();
                mScrollView.post(
                        () -> {
                            mScrollView.fullScroll(ScrollView.FOCUS_DOWN);
                        });
            } else {
                mDropdownContainer.setVisibility(View.VISIBLE);
                PrivacySandboxBridge.promptActionOccurred(PromptAction.CONSENT_MORE_INFO_OPENED);
                LayoutInflater.from(getContext())
                        .inflate(
                                R.layout.privacy_sandbox_consent_eea_dropdown,
                                mDropdownContainer);

                PrivacySandboxDialogUtils.setBulletTextWithBoldContent(
                        getContext(),
                        mDropdownContainer,
                        R.id.privacy_sandbox_m1_consent_learn_more_bullet_one,
                        R.string.privacy_sandbox_m1_consent_learn_more_bullet_1);
                PrivacySandboxDialogUtils.setBulletTextWithBoldContent(
                        getContext(),
                        mDropdownContainer,
                        R.id.privacy_sandbox_m1_consent_learn_more_bullet_two,
                        R.string.privacy_sandbox_m1_consent_learn_more_bullet_2);
                PrivacySandboxDialogUtils.setBulletTextWithBoldContent(
                        getContext(),
                        mDropdownContainer,
                        R.id.privacy_sandbox_m1_consent_learn_more_bullet_three,
                        R.string.privacy_sandbox_m1_consent_learn_more_bullet_3);

                mScrollView.post(
                        () -> {
                            mScrollView.scrollTo(0, mDropdownElement.getTop());
                        });
            }

            mExpandArrowView.setChecked(isDropdownExpanded());
            PrivacySandboxDialogUtils.updateDropdownControlContentDescription(
                    getContext(),
                    view,
                    isDropdownExpanded(),
                    R.string.privacy_sandbox_m1_consent_learn_more_expand_label);
            view.announceForAccessibility(
                    getContext()
                            .getResources()
                            .getString(
                                    isDropdownExpanded()
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
    }

    private void dismissAndMaybeShowNotice() {
        mProgressBarContainer.setVisibility(View.VISIBLE);
        mConsentViewContainer.setVisibility(View.GONE);
        var consentHandler = new Handler();
        // Dismiss has a bigger timeout than spinner in order to guarantee a graceful transition
        // between the spinner view and the notice one.
        consentHandler.postDelayed(this::dismiss, getDismissTimeout());
        consentHandler.postDelayed(this::showNotice, getSpinnerTimeout());
    }

    private void showNotice() {
        PrivacySandboxDialogController.showNoticeEEA(getContext(), mSettingsLauncher);
    }

    private boolean isDropdownExpanded() {
        return mDropdownContainer != null && mDropdownContainer.getVisibility() == View.VISIBLE;
    }

    private long getDismissTimeout() {
        return mAreAnimationsDisabled ? 0 : SPINNER_DURATION_MS + BACKGROUND_TRANSITION_DURATION_MS;
    }

    private long getSpinnerTimeout() {
        return mAreAnimationsDisabled ? 0 : SPINNER_DURATION_MS;
    }
}
