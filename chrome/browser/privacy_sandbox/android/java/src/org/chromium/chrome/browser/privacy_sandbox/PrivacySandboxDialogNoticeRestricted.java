// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.content.DialogInterface;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.widget.ChromeDialog;
import org.chromium.ui.widget.ButtonCompat;

/** Dialog in the form of a notice shown for the Privacy Sandbox. */
public class PrivacySandboxDialogNoticeRestricted extends ChromeDialog
        implements DialogInterface.OnShowListener {
    private final PrivacySandboxBridge mPrivacySandboxBridge;
    private View mContentView;

    private ButtonCompat mMoreButton;
    private LinearLayout mActionButtons;
    private ScrollView mScrollView;
    private @SurfaceType int mSurfaceType;
    private View.OnClickListener mOnClickListener;
    private boolean mShowMoreButtonForTesting;

    public PrivacySandboxDialogNoticeRestricted(
            Context context,
            PrivacySandboxBridge privacySandboxBridge,
            @SurfaceType int surfaceType,
            boolean showMoreButtonForTesting) {
        super(context, R.style.ThemeOverlay_BrowserUI_Fullscreen);
        mPrivacySandboxBridge = privacySandboxBridge;
        mSurfaceType = surfaceType;
        mShowMoreButtonForTesting = showMoreButtonForTesting;
        mContentView =
                LayoutInflater.from(context)
                        .inflate(R.layout.privacy_sandbox_notice_restricted, null);
        setContentView(mContentView);
        mOnClickListener = getOnClickListener();

        ButtonCompat ackButton = mContentView.findViewById(R.id.ack_button);
        ackButton.setOnClickListener(mOnClickListener);
        ButtonCompat settingsButton = mContentView.findViewById(R.id.settings_button);
        settingsButton.setOnClickListener(mOnClickListener);

        mMoreButton = mContentView.findViewById(R.id.more_button);
        mActionButtons = mContentView.findViewById(R.id.action_buttons);
        mScrollView = mContentView.findViewById(R.id.privacy_sandbox_dialog_scroll_view);

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
                "MeasurementNoticeModal"
                        + PrivacySandboxDialogUtils.getSurfaceTypeAsString(mSurfaceType)) {
            @Override
            public void processClick(View v) {
                processClickImpl(v);
            }
        };
    }

    @Override
    public void show() {
        mPrivacySandboxBridge.promptActionOccurred(
                PromptAction.RESTRICTED_NOTICE_SHOWN, mSurfaceType);
        super.show();
    }

    public void processClickImpl(View view) {
        int id = view.getId();
        if (id == R.id.ack_button) {
            RecordUserAction.record("Settings.PrivacySandbox.RestrictedNoticeDialog.AckClicked");
            mPrivacySandboxBridge.promptActionOccurred(
                    PromptAction.RESTRICTED_NOTICE_ACKNOWLEDGE, mSurfaceType);
            dismiss();
        } else if (id == R.id.settings_button) {
            RecordUserAction.record(
                    "Settings.PrivacySandbox.RestrictedNoticeDialog.OpenSettingsClicked");
            mPrivacySandboxBridge.promptActionOccurred(
                    PromptAction.RESTRICTED_NOTICE_OPEN_SETTINGS, mSurfaceType);
            dismiss();
            SettingsNavigationFactory.createSettingsNavigation()
                    .startSettings(getContext(), AdMeasurementFragment.class);
        } else if (id == R.id.more_button) {
            if (mShowMoreButtonForTesting) mShowMoreButtonForTesting = false;
            mPrivacySandboxBridge.promptActionOccurred(
                    PromptAction.RESTRICTED_NOTICE_MORE_BUTTON_CLICKED, mSurfaceType);
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
        }
    }

    @Override
    public void onShow(DialogInterface dialogInterface) {
        if (mScrollView.canScrollVertically(ScrollView.FOCUS_DOWN) || mShowMoreButtonForTesting) {
            mMoreButton.setVisibility(View.VISIBLE);
            mActionButtons.setVisibility(View.GONE);
        } else {
            mMoreButton.setVisibility(View.GONE);
            mActionButtons.setVisibility(View.VISIBLE);
        }
        mScrollView.setVisibility(View.VISIBLE);
    }
}
