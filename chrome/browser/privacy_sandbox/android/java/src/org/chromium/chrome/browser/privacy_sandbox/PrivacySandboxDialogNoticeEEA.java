// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.app.Activity;
import android.content.DialogInterface;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.components.browser_ui.widget.ChromeDialog;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.CheckableImageView;

/** Dialog in the form of a notice shown for the Privacy Sandbox. */
@NullMarked
public class PrivacySandboxDialogNoticeEEA extends ChromeDialog
        implements DialogInterface.OnShowListener {
    private final PrivacySandboxBridge mPrivacySandboxBridge;
    private final View mContentView;

    private final ButtonCompat mMoreButton;
    private final LinearLayout mActionButtons;
    private final ScrollView mScrollView;
    private final LinearLayout mDropdownElement;

    private final CheckableImageView mExpandArrowView;
    private final LinearLayout mDropdownContainer;
    private final @SurfaceType int mSurfaceType;
    private final View.OnClickListener mOnClickListener;

    public PrivacySandboxDialogNoticeEEA(
            Activity activity,
            PrivacySandboxBridge privacySandboxBridge,
            @SurfaceType int surfaceType) {
        super(
                activity,
                R.style.ThemeOverlay_BrowserUI_Fullscreen,
                EdgeToEdgeUtils.isEdgeToEdgeEverywhereEnabled());

        mPrivacySandboxBridge = privacySandboxBridge;
        mSurfaceType = surfaceType;
        mContentView =
                LayoutInflater.from(activity).inflate(R.layout.privacy_sandbox_notice_eea, null);
        setContentView(mContentView);
        mOnClickListener = getOnClickListener();

        ButtonCompat ackButton = mContentView.findViewById(R.id.ack_button);
        ackButton.setOnClickListener(mOnClickListener);
        ButtonCompat settingsButton = mContentView.findViewById(R.id.settings_button);
        settingsButton.setOnClickListener(mOnClickListener);

        mMoreButton = mContentView.findViewById(R.id.more_button);
        mActionButtons = mContentView.findViewById(R.id.action_buttons);
        mScrollView = mContentView.findViewById(R.id.privacy_sandbox_dialog_scroll_view);

        // Controls for the expanding section.
        mDropdownElement = mContentView.findViewById(R.id.dropdown_element);
        mDropdownElement.setOnClickListener(mOnClickListener);
        mDropdownContainer = mContentView.findViewById(R.id.dropdown_container);
        mExpandArrowView = mContentView.findViewById(R.id.expand_arrow);
        mExpandArrowView.setImageDrawable(PrivacySandboxDialogUtils.createExpandDrawable(activity));
        mExpandArrowView.setChecked(isDropdownExpanded());

        setBulletsDescription();

        mMoreButton.setOnClickListener(mOnClickListener);
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

    private View.OnClickListener getOnClickListener() {
        return new PrivacySandboxDebouncedOnClick(
                "ProtectedAudienceMeasurementNoticeModal"
                        + PrivacySandboxDialogUtils.getSurfaceTypeAsString(mSurfaceType)) {
            @Override
            public void processClick(View v) {
                processClickImpl(v);
            }
        };
    }

    @Override
    public void show() {
        mPrivacySandboxBridge.promptActionOccurred(PromptAction.NOTICE_SHOWN, mSurfaceType);
        super.show();
    }

    public void processClickImpl(View view) {
        int id = view.getId();
        if (id == R.id.ack_button) {
            RecordUserAction.record("Settings.PrivacySandbox.NoticeEeaDialog.AckClicked");
            mPrivacySandboxBridge.promptActionOccurred(
                    PromptAction.NOTICE_ACKNOWLEDGE, mSurfaceType);
            dismiss();
        } else if (id == R.id.settings_button) {
            RecordUserAction.record("Settings.PrivacySandbox.NoticeEeaDialog.OpenSettingsClicked");
            mPrivacySandboxBridge.promptActionOccurred(
                    PromptAction.NOTICE_OPEN_SETTINGS, mSurfaceType);
            dismiss();
            PrivacySandboxSettingsBaseFragment.launchPrivacySandboxSettings(
                    getContext(), PrivacySandboxReferrer.PRIVACY_SANDBOX_NOTICE);
        } else if (id == R.id.more_button) {
            mPrivacySandboxBridge.promptActionOccurred(
                    PromptAction.NOTICE_MORE_BUTTON_CLICKED, mSurfaceType);
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
                mPrivacySandboxBridge.promptActionOccurred(
                        PromptAction.NOTICE_MORE_INFO_CLOSED, mSurfaceType);
                mDropdownContainer.setVisibility(View.GONE);
                mDropdownContainer.removeAllViews();
            } else {
                mDropdownContainer.setVisibility(View.VISIBLE);
                mPrivacySandboxBridge.promptActionOccurred(
                        PromptAction.NOTICE_MORE_INFO_OPENED, mSurfaceType);
                LayoutInflater.from(getContext())
                        .inflate(R.layout.privacy_sandbox_notice_eea_dropdown, mDropdownContainer);

                PrivacySandboxDialogUtils.setBulletTextWithBoldContent(
                        getContext(),
                        mDropdownContainer,
                        R.id.privacy_sandbox_m1_notice_eea_learn_more_bullet_one,
                        R.string.privacy_sandbox_m1_notice_eea_learn_more_bullet_1);
                PrivacySandboxDialogUtils.setBulletTextWithBoldContent(
                        getContext(),
                        mDropdownContainer,
                        R.id.privacy_sandbox_m1_notice_eea_learn_more_bullet_two,
                        R.string.privacy_sandbox_m1_notice_eea_learn_more_bullet_2);
                PrivacySandboxDialogUtils.setBulletTextWithBoldContent(
                        getContext(),
                        mDropdownContainer,
                        R.id.privacy_sandbox_m1_notice_eea_learn_more_bullet_three,
                        R.string.privacy_sandbox_m1_notice_eea_learn_more_bullet_3);

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
                    R.string.privacy_sandbox_m1_notice_eea_learn_more_expand_label);
            view.announceForAccessibility(
                    getContext()
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
        mScrollView.setVisibility(View.VISIBLE);
    }

    private void setBulletsDescription() {
        PrivacySandboxDialogUtils.setBulletText(
                getContext(),
                mContentView,
                R.id.privacy_sandbox_m1_notice_eea_bullet_one,
                R.string.privacy_sandbox_m1_notice_eea_bullet_1);
        PrivacySandboxDialogUtils.setBulletText(
                getContext(),
                mContentView,
                R.id.privacy_sandbox_m1_notice_eea_bullet_two,
                R.string.privacy_sandbox_m1_notice_eea_bullet_2);
    }

    private boolean isDropdownExpanded() {
        return mDropdownContainer != null && mDropdownContainer.getVisibility() == View.VISIBLE;
    }
}
