// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.content.DialogInterface;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import androidx.annotation.IdRes;
import androidx.annotation.IntDef;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.widget.ChromeDialog;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.content_public.browser.LifecycleState;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.CheckableImageView;
import org.chromium.ui.widget.TextViewWithLeading;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

// TODO(crbug.com/392943234): Update this class's naming and description when naming is finalized.
/** Handles logic for the Privacy Sandbox Ads consents/notices dialogs. */
public class PrivacySandboxDialogV3 extends ChromeDialog
        implements View.OnClickListener, DialogInterface.OnShowListener {

    @IntDef({
        PrivacySandboxDialogType.UNKNOWN,
        PrivacySandboxDialogType.EEA_CONSENT,
        PrivacySandboxDialogType.EEA_NOTICE,
        PrivacySandboxDialogType.ROW_NOTICE,
        PrivacySandboxDialogType.RESTRICTED_NOTICE,
        PrivacySandboxDialogType.MAX_VALUE,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PrivacySandboxDialogType {
        // Default dialog type.
        int UNKNOWN = 0;
        int EEA_CONSENT = 1;
        int EEA_NOTICE = 2;
        int ROW_NOTICE = 3;
        int RESTRICTED_NOTICE = 4;
        int MAX_VALUE = 5;
    }

    private @PrivacySandboxDialogType int mDialogType;

    private View mContentView;

    private LinearLayout mViewContainer;
    private ButtonCompat mMoreButton;
    private LinearLayout mActionButtons;
    private ScrollView mScrollView;
    private View mBottomFade;

    // Dropdown elements
    private LinearLayout mDropdownElement;
    private CheckableImageView mDropdownExpandArrowView;
    private LinearLayout mDropdownContentContainer;

    // Privacy policy
    private boolean mIsPrivacyPageLoaded;
    private LinearLayout mPrivacyPolicyView;
    private FrameLayout mPrivacyPolicyContent;
    private ThinWebView mThinWebView;
    private WebContents mWebContents;
    private WebContentsObserver mWebContentsObserver;

    private TextViewWithLeading mLearnMoreText;
    private @IdRes int mLearnMoreTextIdRes = R.id.learn_more_text;
    private @StringRes int mLearnMoreLinkString =
            R.string.privacy_sandbox_m1_consent_learn_more_card_v3;

    public PrivacySandboxDialogV3(
            Context context,
            Profile profile,
            ActivityWindowAndroid activityWindowAndroid,
            @PrivacySandboxDialogType int dialogType) {
        super(context, R.style.ThemeOverlay_BrowserUI_Fullscreen);
        mDialogType = dialogType;

        fetchDialogContent(context);
        registerDialogButtons();
        registerDropdownElements(context);
        registerPrivacyPolicy(profile, activityWindowAndroid);

        mScrollView = mContentView.findViewById(R.id.privacy_sandbox_dialog_scroll_view);
        mScrollView
                .getViewTreeObserver()
                .addOnScrollChangedListener(
                        () -> {
                            if (!canScrollVerticallyDown()) {
                                mMoreButton.setVisibility(View.GONE);
                                mBottomFade.setVisibility(View.GONE);
                                mActionButtons.setVisibility(View.VISIBLE);
                                mScrollView.post(
                                        () -> {
                                            mScrollView.pageScroll(ScrollView.FOCUS_DOWN);
                                        });
                            }
                        });

        setOnShowListener(this);
        setCancelable(false);
    }

    private void fetchDialogContent(Context context) {
        switch (mDialogType) {
                // Fall through
            case PrivacySandboxDialogType.EEA_CONSENT:
            case PrivacySandboxDialogType.EEA_NOTICE:
            case PrivacySandboxDialogType.ROW_NOTICE:
            case PrivacySandboxDialogType.RESTRICTED_NOTICE:
            default:
                // TODO(crbug.com/392943234): For now we're defaulting to using the EEA consent
                // dialog for all types as it's the only available dialog right now. Update this as
                // we support more dialogs.
                mContentView =
                        LayoutInflater.from(context)
                                .inflate(R.layout.privacy_sandbox_consent_eea_v3, null);
                mViewContainer = mContentView.findViewById(R.id.privacy_sandbox_consent_eea_view);
                // TODO(crbug.com/392943234): Emit a histogram if we hit the default cause.
        }
        setContentView(mContentView);
    }

    private void registerDialogButtons() {
        ButtonCompat ackButton = mContentView.findViewById(R.id.ack_button);
        ackButton.setOnClickListener(this);
        ButtonCompat noButton = mContentView.findViewById(R.id.no_button);
        noButton.setOnClickListener(this);
        mActionButtons = mContentView.findViewById(R.id.action_buttons);

        mMoreButton = mContentView.findViewById(R.id.more_button);
        mMoreButton.setOnClickListener(this);
        mBottomFade = mContentView.findViewById(R.id.bottom_fade);
    }

    private void registerDropdownElements(Context context) {
        // TODO(crbug.com/392943234): Not all dialogs will contain a dropdown, update logic to
        // reflect this.
        mDropdownElement = mContentView.findViewById(R.id.ad_measurement_dropdown_element);
        mDropdownElement.setOnClickListener(this);

        mDropdownContentContainer =
                mContentView.findViewById(R.id.ad_measurement_dropdown_container);
        mDropdownExpandArrowView = mContentView.findViewById(R.id.ad_measurement_expand_arrow);
        mDropdownExpandArrowView.setImageDrawable(
                PrivacySandboxDialogUtils.createExpandDrawable(context));
        mDropdownExpandArrowView.setChecked(isDropdownExpanded());
    }

    private void handleAckButtonClick() {
        // TODO(crbug.com/392943234): Record that ack was clicked.
        dismiss();
    }

    private void handleNoButtonClick() {
        // TODO(crbug.com/392943234): Record that declined was clicked.
        dismiss();
    }

    private void handleMoreButtonClick() {
        // TODO(crbug.com/392943234): Record that more button was clicked.
        mScrollView.post(() -> mScrollView.pageScroll(ScrollView.FOCUS_DOWN));
    }

    private void handleDropdownClick(View view) {
        if (isDropdownExpanded()) {
            mDropdownContentContainer.setVisibility(View.GONE);
            mDropdownContentContainer.removeAllViews();
        } else {
            mDropdownContentContainer.setVisibility(View.VISIBLE);
            LayoutInflater.from(getContext())
                    .inflate(
                            R.layout.privacy_sandbox_consent_eea_dropdown_v3,
                            mDropdownContentContainer);
            mScrollView.post(() -> mScrollView.scrollTo(0, mDropdownElement.getTop()));
        }

        mDropdownExpandArrowView.setChecked(isDropdownExpanded());
        PrivacySandboxDialogUtils.updateDropdownControlContentDescription(
                getContext(),
                view,
                isDropdownExpanded(),
                R.string.privacy_sandbox_m1_consent_ads_topic_dropdown_v3);
        view.announceForAccessibility(
                getContext()
                        .getString(
                                isDropdownExpanded()
                                        ? R.string.accessibility_expanded_group
                                        : R.string.accessibility_collapsed_group));
    }

    private void handlePrivacyPolicyBackButtonClicked() {
        mPrivacyPolicyView.setVisibility(View.GONE);
        mPrivacyPolicyContent.removeAllViews();
        mViewContainer.setVisibility(View.VISIBLE);
    }

    /**
     * Handles clicks on the Privacy Policy link. If a ThinWebView is available, loads and displays
     * the privacy policy within it, replacing the consent view.
     *
     * @param unused_view The View that was clicked (typically the TextView containing the link).
     */
    private void onPrivacyPolicyClicked(View unused_view) {
        mPrivacyPolicyContent.removeAllViews();
        if (mThinWebView != null && mThinWebView.getView() != null) {
            mViewContainer.setVisibility(View.GONE);
            mPrivacyPolicyContent.addView(mThinWebView.getView());
            mPrivacyPolicyView.setVisibility(View.VISIBLE);
        }
    }

    private void registerPrivacyPolicy(
            Profile profile, ActivityWindowAndroid activityWindowAndroid) {
        mPrivacyPolicyView = mContentView.findViewById(R.id.privacy_policy_view);
        if (mPrivacyPolicyView == null) {
            return;
        }
        mPrivacyPolicyContent = mContentView.findViewById(R.id.privacy_policy_content);
        mContentView.findViewById(R.id.privacy_policy_back_button).setOnClickListener(this);
        mIsPrivacyPageLoaded = false;
        createPrivacyPolicyLink(profile, activityWindowAndroid);
    }

    private void createPrivacyPolicyLink(
            Profile profile, ActivityWindowAndroid activityWindowAndroid) {
        mLearnMoreText = mContentView.findViewById(mLearnMoreTextIdRes);
        mLearnMoreText.setText(
                SpanApplier.applySpans(
                        getContext().getString(mLearnMoreLinkString),
                        new SpanApplier.SpanInfo(
                                "<link>",
                                "</link>",
                                new ChromeClickableSpan(
                                        getContext(), this::onPrivacyPolicyClicked))));
        mLearnMoreText.setMovementMethod(LinkMovementMethod.getInstance());
        if (mThinWebView == null || mWebContents == null || mWebContents.isDestroyed()) {
            mWebContents = WebContentsFactory.createWebContents(profile, true, false);
            mWebContentsObserver =
                    new WebContentsObserver(mWebContents) {
                        @Override
                        public void didFirstVisuallyNonEmptyPaint() {
                            if (!mIsPrivacyPageLoaded) {
                                mIsPrivacyPageLoaded = true;
                            }
                        }

                        @Override
                        public void didFailLoad(
                                boolean isInPrimaryMainFrame,
                                int errorCode,
                                GURL failingUrl,
                                @LifecycleState int rfhLifecycleState) {
                            // TODO(crbug.com/392943234): Emit a histogram on failure
                        }
                    };
            mThinWebView =
                    PrivacySandboxDialogController.createPrivacyPolicyThinWebView(
                            mWebContents, profile, activityWindowAndroid);
        }
    }

    @VisibleForTesting
    public boolean canScrollVerticallyDown() {
        return mScrollView.canScrollVertically(ScrollView.FOCUS_DOWN);
    }

    // OnClickListener:
    @Override
    public void onClick(View view) {
        int id = view.getId();
        if (id == R.id.ack_button) {
            handleAckButtonClick();
        } else if (id == R.id.no_button) {
            handleNoButtonClick();
        } else if (id == R.id.more_button) {
            handleMoreButtonClick();
        // TODO(crbug.com/392943234): Rename this to be more generic
        } else if (id == R.id.ad_measurement_dropdown_element) {
            handleDropdownClick(view);
        } else if (id == R.id.privacy_policy_back_button) {
            handlePrivacyPolicyBackButtonClicked();
        }
    }

    @Override
    public void onShow(DialogInterface dialogInterface) {
        if (canScrollVerticallyDown()) {
            mMoreButton.setVisibility(View.VISIBLE);
            mBottomFade.setVisibility(View.VISIBLE);
            mActionButtons.setVisibility(View.GONE);
        } else {
            mMoreButton.setVisibility(View.GONE);
            mBottomFade.setVisibility(View.GONE);
            mActionButtons.setVisibility(View.VISIBLE);
        }
    }

    @Override
    protected void onStop() {
        super.onStop();

        // Clean up the WebContents, WebContentsObserver and when the dialog is stopped
        if (mThinWebView != null) {
            mWebContents.destroy();
            mWebContents = null;
            mWebContentsObserver.observe(null);
            mWebContentsObserver = null;
            mThinWebView.destroy();
            mThinWebView = null;
        }
    }

    private boolean isDropdownExpanded() {
        return mDropdownContentContainer != null
                && mDropdownContentContainer.getVisibility() == View.VISIBLE;
    }
}
