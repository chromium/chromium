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
import androidx.annotation.StringRes;

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
import org.chromium.ui.widget.ChromeImageButton;
import org.chromium.ui.widget.TextViewWithLeading;
import org.chromium.url.GURL;

// TODO(crbug.com/392943234): Update this class's naming and description when naming is finalized.
/** Handles logic for the Privacy Sandbox Ads consents/notices dialogs. */
public class PrivacySandboxDialogV3 extends ChromeDialog
        implements View.OnClickListener, DialogInterface.OnShowListener {
    private View mContentView;

    private LinearLayout mViewContainer;
    private ButtonCompat mMoreButton;
    private LinearLayout mActionButtons;
    private ScrollView mScrollView;
    private View mBottomFade;
    private LinearLayout mAdMeasurementDropdownElement;

    private final CheckableImageView mAdMeasurementExpandArrowView;
    private LinearLayout mAdMeasurementDropdownContainer;
    private TextViewWithLeading mLearnMoreText;

    private boolean mIsPrivacyPageLoaded;

    private LinearLayout mPrivacyPolicyView;
    private FrameLayout mPrivacyPolicyContent;
    private ChromeImageButton mPrivacyPolicyBackButton;
    private ThinWebView mThinWebView;
    private WebContents mWebContents;
    private WebContentsObserver mWebContentsObserver;
    private final Profile mProfile;
    private final ActivityWindowAndroid mActivityWindowAndroid;

    private @IdRes int mLearnMoreTextIdRes = R.id.learn_more_text;
    private @StringRes int mLearnMoreLinkString =
            R.string.privacy_sandbox_m1_consent_learn_more_card_v3;

    public PrivacySandboxDialogV3(
            Context context, Profile profile, ActivityWindowAndroid activityWindowAndroid) {
        super(context, R.style.ThemeOverlay_BrowserUI_Fullscreen);

        mProfile = profile;
        mActivityWindowAndroid = activityWindowAndroid;

        mContentView =
                LayoutInflater.from(context).inflate(R.layout.privacy_sandbox_consent_eea_v3, null);
        setContentView(mContentView);

        ButtonCompat ackButton = mContentView.findViewById(R.id.ack_button);
        ackButton.setOnClickListener(this);

        ButtonCompat noButton = mContentView.findViewById(R.id.no_button);
        noButton.setOnClickListener(this);

        mMoreButton = mContentView.findViewById(R.id.more_button);
        mActionButtons = mContentView.findViewById(R.id.action_buttons);
        mScrollView = mContentView.findViewById(R.id.privacy_sandbox_dialog_scroll_view);

        mBottomFade = mContentView.findViewById(R.id.bottom_fade);

        // Controls for the Ad Measurement expanding section.
        mAdMeasurementDropdownElement =
                mContentView.findViewById(R.id.ad_measurement_dropdown_element);
        mAdMeasurementDropdownElement.setOnClickListener(this);
        mAdMeasurementDropdownContainer =
                mContentView.findViewById(R.id.ad_measurement_dropdown_container);
        mAdMeasurementExpandArrowView = mContentView.findViewById(R.id.ad_measurement_expand_arrow);
        mAdMeasurementExpandArrowView.setImageDrawable(
                PrivacySandboxDialogUtils.createExpandDrawable(context));
        mAdMeasurementExpandArrowView.setChecked(isMeasurementDropdownExpanded());

        mViewContainer = mContentView.findViewById(R.id.privacy_sandbox_consent_eea_view);
        mPrivacyPolicyView = mContentView.findViewById(R.id.privacy_policy_view);
        mPrivacyPolicyContent = mContentView.findViewById(R.id.privacy_policy_content);
        mPrivacyPolicyBackButton = mContentView.findViewById(R.id.privacy_policy_back_button);
        mPrivacyPolicyBackButton.setOnClickListener(this);
        mIsPrivacyPageLoaded = false;

        createPrivacyPolicyLink();
        mMoreButton.setOnClickListener(this);
        setOnShowListener(this);
        setCancelable(false);

        mScrollView
                .getViewTreeObserver()
                .addOnScrollChangedListener(
                        () -> {
                            if (!mScrollView.canScrollVertically(ScrollView.FOCUS_DOWN)) {
                                mMoreButton.setVisibility(View.GONE);
                                mBottomFade.setVisibility(View.GONE);
                                mActionButtons.setVisibility(View.VISIBLE);
                                mScrollView.post(
                                        () -> {
                                            mScrollView.pageScroll(ScrollView.FOCUS_DOWN);
                                        });
                            }
                        });
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

    private void handleAdMeasurementDropdownClick(View view) {
        if (isMeasurementDropdownExpanded()) {
            mAdMeasurementDropdownContainer.setVisibility(View.GONE);
            mAdMeasurementDropdownContainer.removeAllViews();
        } else {
            mAdMeasurementDropdownContainer.setVisibility(View.VISIBLE);
            LayoutInflater.from(getContext())
                    .inflate(
                            R.layout.privacy_sandbox_consent_eea_dropdown_v3,
                            mAdMeasurementDropdownContainer);

            mScrollView.post(() -> mScrollView.scrollTo(0, mAdMeasurementDropdownElement.getTop()));
        }

        mAdMeasurementExpandArrowView.setChecked(isMeasurementDropdownExpanded());
        PrivacySandboxDialogUtils.updateDropdownControlContentDescription(
                getContext(),
                view,
                isMeasurementDropdownExpanded(),
                R.string.privacy_sandbox_m1_consent_ads_topic_dropdown_v3);
        view.announceForAccessibility(
                getContext()
                        .getString(
                                isMeasurementDropdownExpanded()
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

    private void createPrivacyPolicyLink() {
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
            mWebContents = WebContentsFactory.createWebContents(mProfile, true, false);
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
                            mWebContents, mProfile, mActivityWindowAndroid);
        }
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
        } else if (id == R.id.ad_measurement_dropdown_element) {
            handleAdMeasurementDropdownClick(view);
        } else if (id == R.id.privacy_policy_back_button) {
            handlePrivacyPolicyBackButtonClicked();
        }
    }

    @Override
    public void onShow(DialogInterface dialogInterface) {
        if (mScrollView.canScrollVertically(ScrollView.FOCUS_DOWN)) {
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

    private boolean isMeasurementDropdownExpanded() {
        return mAdMeasurementDropdownContainer != null
                && mAdMeasurementDropdownContainer.getVisibility() == View.VISIBLE;
    }
}
