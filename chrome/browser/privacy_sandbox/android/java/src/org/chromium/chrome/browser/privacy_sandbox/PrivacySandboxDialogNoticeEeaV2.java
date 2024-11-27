// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.content.DialogInterface;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.widget.ChromeDialog;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.thinwebview.ThinWebView;
import org.chromium.content_public.browser.LifecycleState;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.CheckableImageView;
import org.chromium.ui.widget.ChromeImageButton;
import org.chromium.ui.widget.TextViewWithLeading;
import org.chromium.url.GURL;

/**
 * Dialog in the form of a notice shown for the Privacy Sandbox - V2. This new version is part of
 * the Ads API UX Enhancement.
 */
public class PrivacySandboxDialogNoticeEeaV2 extends ChromeDialog
        implements View.OnClickListener, DialogInterface.OnShowListener {
    private final PrivacySandboxBridge mPrivacySandboxBridge;
    private View mContentView;

    private LinearLayout mNoticeViewContainer;
    private ButtonCompat mMoreButton;
    private LinearLayout mActionButtons;
    private ScrollView mScrollView;
    private LinearLayout mSiteSuggestedAdsDropdownElement;
    private LinearLayout mAdMeasurementDropdownElement;

    private final CheckableImageView mSiteSuggestedAdsExpandArrowView;
    private final CheckableImageView mAdMeasurementExpandArrowView;
    private LinearLayout mSiteSuggestedAdsDropdownContainer;
    private LinearLayout mAdMeasurementDropdownContainer;
    private @SurfaceType int mSurfaceType;

    private LinearLayout mPrivacyPolicyView;
    private FrameLayout mPrivacyPolicyContent;
    private ChromeImageButton mPrivacyPolicyBackButton;
    private TextViewWithLeading mLearnMoreBullet1Description;
    private ThinWebView mThinWebView;
    private WebContents mWebContents;
    private WebContentsObserver mWebContentsObserver;
    private final Profile mProfile;
    private long mPrivacyPolicyClickedTimestamp;
    private final ActivityWindowAndroid mActivityWindowAndroid;
    private boolean mIsPrivacyPageLoaded;

    public PrivacySandboxDialogNoticeEeaV2(
            Context context,
            PrivacySandboxBridge privacySandboxBridge,
            @SurfaceType int surfaceType,
            Profile profile,
            ActivityWindowAndroid activityWindowAndroid) {
        super(context, R.style.ThemeOverlay_BrowserUI_Fullscreen);

        mPrivacySandboxBridge = privacySandboxBridge;
        mSurfaceType = surfaceType;
        mProfile = profile;
        mActivityWindowAndroid = activityWindowAndroid;
        mContentView =
                LayoutInflater.from(context).inflate(R.layout.privacy_sandbox_notice_eea_v2, null);
        setContentView(mContentView);

        ButtonCompat ackButton = mContentView.findViewById(R.id.ack_button);
        ackButton.setOnClickListener(this);
        ButtonCompat settingsButton = mContentView.findViewById(R.id.settings_button);
        settingsButton.setOnClickListener(this);

        mMoreButton = mContentView.findViewById(R.id.more_button);
        mActionButtons = mContentView.findViewById(R.id.action_buttons);
        mScrollView = mContentView.findViewById(R.id.privacy_sandbox_dialog_scroll_view);

        // Controls for the Site Suggested Ads expanding section.
        mSiteSuggestedAdsDropdownElement =
                mContentView.findViewById(R.id.site_suggested_ads_dropdown_element);
        mSiteSuggestedAdsDropdownElement.setOnClickListener(this);
        mSiteSuggestedAdsDropdownContainer =
                mContentView.findViewById(R.id.site_suggested_ads_dropdown_container);
        mSiteSuggestedAdsExpandArrowView =
                mContentView.findViewById(R.id.site_suggested_ads_expand_arrow);
        mSiteSuggestedAdsExpandArrowView.setImageDrawable(
                PrivacySandboxDialogUtils.createExpandDrawable(context));
        mSiteSuggestedAdsExpandArrowView.setChecked(isSiteSuggestedAdsDropdownExpanded());

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

        mNoticeViewContainer = mContentView.findViewById(R.id.privacy_sandbox_notice_eea_view);
        mPrivacyPolicyView = mContentView.findViewById(R.id.privacy_policy_view);
        mPrivacyPolicyContent = mContentView.findViewById(R.id.privacy_policy_content);
        mPrivacyPolicyBackButton = mContentView.findViewById(R.id.privacy_policy_back_button);
        mPrivacyPolicyBackButton.setOnClickListener(this);
        mIsPrivacyPageLoaded = false;

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
        mPrivacySandboxBridge.promptActionOccurred(PromptAction.NOTICE_SHOWN, mSurfaceType);
        super.show();
    }

    private void handleAckButtonClick() {
        mPrivacySandboxBridge.promptActionOccurred(PromptAction.NOTICE_ACKNOWLEDGE, mSurfaceType);
        dismiss();
    }

    private void handleSettingsButtonClick() {
        mPrivacySandboxBridge.promptActionOccurred(PromptAction.NOTICE_OPEN_SETTINGS, mSurfaceType);
        dismiss();
        PrivacySandboxSettingsBaseFragment.launchPrivacySandboxSettings(
                getContext(), PrivacySandboxReferrer.PRIVACY_SANDBOX_NOTICE);
    }

    private void handleMoreButtonClick() {
        mPrivacySandboxBridge.promptActionOccurred(
                PromptAction.NOTICE_MORE_BUTTON_CLICKED, mSurfaceType);
        mScrollView.post(() -> mScrollView.pageScroll(ScrollView.FOCUS_DOWN));
    }

    private void handleSiteSuggestedAdsDropdownClick(View view) {
        // TODO(crbug.com/379337243): Add metrics
        if (isSiteSuggestedAdsDropdownExpanded()) {
            mSiteSuggestedAdsDropdownContainer.setVisibility(View.GONE);
            mSiteSuggestedAdsDropdownContainer.removeAllViews();
        } else {
            mSiteSuggestedAdsDropdownContainer.setVisibility(View.VISIBLE);
            LayoutInflater.from(getContext())
                    .inflate(
                            R.layout.privacy_sandbox_notice_eea_site_suggested_ads_dropdown,
                            mSiteSuggestedAdsDropdownContainer);

            PrivacySandboxDialogUtils.setBulletTextWithBoldContent(
                    getContext(),
                    mSiteSuggestedAdsDropdownContainer,
                    R.id.privacy_sandbox_m1_notice_eea_site_suggested_ads_learn_more_bullet_one,
                    R.string.privacy_sandbox_m1_notice_eea_site_suggested_ads_learn_more_bullet_1);
            PrivacySandboxDialogUtils.setBulletTextWithBoldContent(
                    getContext(),
                    mSiteSuggestedAdsDropdownContainer,
                    R.id.privacy_sandbox_m1_notice_eea_site_suggested_ads_learn_more_bullet_two,
                    R.string.privacy_sandbox_m1_notice_eea_site_suggested_ads_learn_more_bullet_2);

            mScrollView.post(
                    () -> mScrollView.scrollTo(0, mSiteSuggestedAdsDropdownElement.getTop()));
            handlePrivacyPolicyFeature();
        }

        mSiteSuggestedAdsExpandArrowView.setChecked(isSiteSuggestedAdsDropdownExpanded());
        PrivacySandboxDialogUtils.updateDropdownControlContentDescription(
                getContext(),
                view,
                isSiteSuggestedAdsDropdownExpanded(),
                R.string.privacy_sandbox_m1_notice_eea_site_suggested_ads_learn_more_label);
        view.announceForAccessibility(
                getContext()
                        .getString(
                                isSiteSuggestedAdsDropdownExpanded()
                                        ? R.string.accessibility_expanded_group
                                        : R.string.accessibility_collapsed_group));
    }

    private void handleAdMeasurementDropdownClick(View view) {
        // TODO(crbug.com/379337243): Add metrics
        if (isMeasurementDropdownExpanded()) {
            mAdMeasurementDropdownContainer.setVisibility(View.GONE);
            mAdMeasurementDropdownContainer.removeAllViews();
        } else {
            mAdMeasurementDropdownContainer.setVisibility(View.VISIBLE);
            LayoutInflater.from(getContext())
                    .inflate(
                            R.layout.privacy_sandbox_notice_eea_ad_measurement_dropdown,
                            mAdMeasurementDropdownContainer);

            PrivacySandboxDialogUtils.setBulletTextWithBoldContent(
                    getContext(),
                    mAdMeasurementDropdownContainer,
                    R.id.privacy_sandbox_m1_notice_eea_ad_measurement_learn_more_bullet_one,
                    R.string.privacy_sandbox_m1_notice_eea_ad_measurement_learn_more_bullet_1);

            mScrollView.post(() -> mScrollView.scrollTo(0, mAdMeasurementDropdownElement.getTop()));
        }

        mAdMeasurementExpandArrowView.setChecked(isMeasurementDropdownExpanded());
        PrivacySandboxDialogUtils.updateDropdownControlContentDescription(
                getContext(),
                view,
                isMeasurementDropdownExpanded(),
                R.string.privacy_sandbox_m1_notice_eea_ad_measurement_learn_more_label);
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
        mNoticeViewContainer.setVisibility(View.VISIBLE);
    }

    // OnClickListener:
    @Override
    public void onClick(View view) {
        int id = view.getId();
        if (id == R.id.ack_button) {
            handleAckButtonClick();
        } else if (id == R.id.settings_button) {
            handleSettingsButtonClick();
        } else if (id == R.id.more_button) {
            handleMoreButtonClick();
        } else if (id == R.id.site_suggested_ads_dropdown_element) {
            handleSiteSuggestedAdsDropdownClick(view);
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
            mActionButtons.setVisibility(View.GONE);
        } else {
            mMoreButton.setVisibility(View.GONE);
            mActionButtons.setVisibility(View.VISIBLE);
        }
        mScrollView.setVisibility(View.VISIBLE);
    }

    @Override
    protected void onStop() {
        super.onStop();

        // Clean up the WebContents, WebContentsObserver and when the dialog is stopped
        if (mThinWebView != null) {
            mWebContents.destroy();
            mWebContents = null;
            mWebContentsObserver.destroy();
            mWebContentsObserver = null;
            mThinWebView.destroy();
            mThinWebView = null;
        }
    }

    private void handlePrivacyPolicyFeature() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PRIVACY_SANDBOX_PRIVACY_POLICY)) {
            mLearnMoreBullet1Description =
                    mContentView.findViewById(
                            R.id
                                    .privacy_sandbox_m1_notice_eea_site_suggested_ads_learn_more_bullet_one_description);
            mLearnMoreBullet1Description.setText(
                    SpanApplier.applySpans(
                            getContext()
                                    .getString(
                                            R.string
                                                    .privacy_sandbox_m1_notice_eea_site_suggested_ads_learn_more_bullet_1_description_clank),
                            new SpanApplier.SpanInfo(
                                    "<link>",
                                    "</link>",
                                    new ClickableSpan() {
                                        @Override
                                        public void onClick(View view) {
                                            onPrivacyPolicyClicked(view);
                                        }
                                    })));
            mLearnMoreBullet1Description.setMovementMethod(LinkMovementMethod.getInstance());
            if (mThinWebView == null || mWebContents == null || mWebContents.isDestroyed()) {
                String privacyPolicyUrl =
                        ColorUtils.inNightMode(mActivityWindowAndroid.getContext().get())
                                ? UrlConstants.GOOGLE_EMBEDDED_PRIVACY_POLICY_DARK_MODE
                                : UrlConstants.GOOGLE_EMBEDDED_PRIVACY_POLICY;
                mWebContents = WebContentsFactory.createWebContents(mProfile, true, false);
                mWebContentsObserver =
                        new WebContentsObserver(mWebContents) {
                            @Override
                            public void didFirstVisuallyNonEmptyPaint() {
                                if (!mIsPrivacyPageLoaded) {
                                    RecordHistogram.recordTimesHistogram(
                                            "PrivacySandbox.PrivacyPolicy.LoadingTime",
                                            System.currentTimeMillis()
                                                    - mPrivacyPolicyClickedTimestamp);
                                    mIsPrivacyPageLoaded = true;
                                }
                            }

                            @Override
                            public void didFailLoad(
                                    boolean isInPrimaryMainFrame,
                                    int errorCode,
                                    GURL failingUrl,
                                    @LifecycleState int rfhLifecycleState) {
                                RecordHistogram.recordSparseHistogram(
                                        "PrivacySandbox.PrivacyPolicy.FailedLoadErrorCode",
                                        errorCode);
                            }
                        };
                mThinWebView =
                        PrivacySandboxDialogController.createThinWebView(
                                mWebContents, mProfile, mActivityWindowAndroid, privacyPolicyUrl);
            }
        }
    }

    /**
     * Handles clicks on the Privacy Policy link. If a ThinWebView is available, loads and displays
     * the privacy policy within it, replacing the consent view.
     *
     * @param unused_view The View that was clicked (typically the TextView containing the link).
     */
    private void onPrivacyPolicyClicked(View unused_view) {
        RecordUserAction.record("Settings.PrivacySandbox.Notice.PrivacyPolicyLinkClicked");
        mPrivacyPolicyClickedTimestamp = System.currentTimeMillis();
        mPrivacyPolicyContent.removeAllViews();
        if (mThinWebView != null && mThinWebView.getView() != null) {
            mNoticeViewContainer.setVisibility(View.GONE);
            mPrivacyPolicyContent.addView(mThinWebView.getView());
            mPrivacyPolicyView.setVisibility(View.VISIBLE);
        }
    }

    private boolean isSiteSuggestedAdsDropdownExpanded() {
        return mSiteSuggestedAdsDropdownContainer != null
                && mSiteSuggestedAdsDropdownContainer.getVisibility() == View.VISIBLE;
    }

    private boolean isMeasurementDropdownExpanded() {
        return mAdMeasurementDropdownContainer != null
                && mAdMeasurementDropdownContainer.getVisibility() == View.VISIBLE;
    }
}
