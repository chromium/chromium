// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.DialogInterface;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
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

/**
 * Dialog in the form of a notice shown for the Privacy Sandbox - V2. This new version is part of
 * the Ads API UX Enhancement.
 */
@NullMarked
public class PrivacySandboxDialogNoticeEeaV2 extends ChromeDialog
        implements DialogInterface.OnShowListener {
    private final PrivacySandboxBridge mPrivacySandboxBridge;
    private final View mContentView;

    private final LinearLayout mNoticeViewContainer;
    private final ButtonCompat mMoreButton;
    private final LinearLayout mActionButtons;
    private final ScrollView mScrollView;
    private final LinearLayout mSiteSuggestedAdsDropdownElement;
    private final LinearLayout mAdMeasurementDropdownElement;

    private final CheckableImageView mSiteSuggestedAdsExpandArrowView;
    private final CheckableImageView mAdMeasurementExpandArrowView;
    private final LinearLayout mSiteSuggestedAdsDropdownContainer;
    private final LinearLayout mAdMeasurementDropdownContainer;
    private final @SurfaceType int mSurfaceType;

    private final LinearLayout mPrivacyPolicyView;
    private final FrameLayout mPrivacyPolicyContent;
    private final ChromeImageButton mPrivacyPolicyBackButton;
    private @Nullable TextViewWithLeading mLearnMoreBullet1Description;
    private @Nullable ThinWebView mThinWebView;
    private @Nullable WebContents mWebContents;
    private @Nullable WebContentsObserver mWebContentsObserver;
    private final Profile mProfile;
    private long mPrivacyPolicyClickedTimestamp;
    private final ActivityWindowAndroid mActivityWindowAndroid;
    private boolean mIsPrivacyPageLoaded;
    private final View.OnClickListener mOnClickListener;

    public PrivacySandboxDialogNoticeEeaV2(
            Activity activity,
            PrivacySandboxBridge privacySandboxBridge,
            @SurfaceType int surfaceType,
            Profile profile,
            ActivityWindowAndroid activityWindowAndroid) {
        super(
                activity,
                R.style.ThemeOverlay_BrowserUI_Fullscreen,
                EdgeToEdgeUtils.isEdgeToEdgeEverywhereEnabled());

        mPrivacySandboxBridge = privacySandboxBridge;
        mSurfaceType = surfaceType;
        mProfile = profile;
        mActivityWindowAndroid = activityWindowAndroid;
        mContentView =
                LayoutInflater.from(activity).inflate(R.layout.privacy_sandbox_notice_eea_v2, null);
        setContentView(mContentView);
        mOnClickListener = getOnClickListener();

        ButtonCompat ackButton = mContentView.findViewById(R.id.ack_button);
        ackButton.setOnClickListener(mOnClickListener);
        ButtonCompat settingsButton = mContentView.findViewById(R.id.settings_button);
        settingsButton.setOnClickListener(mOnClickListener);

        mMoreButton = mContentView.findViewById(R.id.more_button);
        mActionButtons = mContentView.findViewById(R.id.action_buttons);
        mScrollView = mContentView.findViewById(R.id.privacy_sandbox_dialog_scroll_view);

        // Controls for the Site Suggested Ads expanding section.
        mSiteSuggestedAdsDropdownElement =
                mContentView.findViewById(R.id.site_suggested_ads_dropdown_element);
        mSiteSuggestedAdsDropdownElement.setOnClickListener(mOnClickListener);
        mSiteSuggestedAdsDropdownContainer =
                mContentView.findViewById(R.id.site_suggested_ads_dropdown_container);
        mSiteSuggestedAdsExpandArrowView =
                mContentView.findViewById(R.id.site_suggested_ads_expand_arrow);
        mSiteSuggestedAdsExpandArrowView.setImageDrawable(
                PrivacySandboxDialogUtils.createExpandDrawable(activity));
        mSiteSuggestedAdsExpandArrowView.setChecked(isSiteSuggestedAdsDropdownExpanded());

        // Controls for the Ad Measurement expanding section.
        mAdMeasurementDropdownElement =
                mContentView.findViewById(R.id.ad_measurement_dropdown_element);
        mAdMeasurementDropdownElement.setOnClickListener(mOnClickListener);
        mAdMeasurementDropdownContainer =
                mContentView.findViewById(R.id.ad_measurement_dropdown_container);
        mAdMeasurementExpandArrowView = mContentView.findViewById(R.id.ad_measurement_expand_arrow);
        mAdMeasurementExpandArrowView.setImageDrawable(
                PrivacySandboxDialogUtils.createExpandDrawable(activity));
        mAdMeasurementExpandArrowView.setChecked(isMeasurementDropdownExpanded());

        mNoticeViewContainer = mContentView.findViewById(R.id.privacy_sandbox_notice_eea_view);
        mPrivacyPolicyView = mContentView.findViewById(R.id.privacy_policy_view);
        mPrivacyPolicyContent = mContentView.findViewById(R.id.privacy_policy_content);
        mPrivacyPolicyBackButton = mContentView.findViewById(R.id.privacy_policy_back_button);
        mPrivacyPolicyBackButton.setOnClickListener(mOnClickListener);
        mIsPrivacyPageLoaded = false;

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
        if (isSiteSuggestedAdsDropdownExpanded()) {
            mPrivacySandboxBridge.promptActionOccurred(
                    PromptAction.NOTICE_SITE_SUGGESTED_ADS_MORE_INFO_CLOSED, mSurfaceType);
            mSiteSuggestedAdsDropdownContainer.setVisibility(View.GONE);
            mSiteSuggestedAdsDropdownContainer.removeAllViews();
        } else {
            mPrivacySandboxBridge.promptActionOccurred(
                    PromptAction.NOTICE_SITE_SUGGESTED_ADS_MORE_INFO_OPENED, mSurfaceType);
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
            handlePrivacyPolicyLink();
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
        if (isMeasurementDropdownExpanded()) {
            mPrivacySandboxBridge.promptActionOccurred(
                    PromptAction.NOTICE_ADS_MEASUREMENT_MORE_INFO_CLOSED, mSurfaceType);
            mAdMeasurementDropdownContainer.setVisibility(View.GONE);
            mAdMeasurementDropdownContainer.removeAllViews();
        } else {
            mPrivacySandboxBridge.promptActionOccurred(
                    PromptAction.NOTICE_ADS_MEASUREMENT_MORE_INFO_OPENED, mSurfaceType);
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

    public void processClickImpl(View view) {
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
            assumeNonNull(mWebContents);
            assumeNonNull(mWebContentsObserver);
            mWebContents.destroy();
            mWebContents = null;
            mWebContentsObserver.observe(null);
            mWebContentsObserver = null;
            mThinWebView.destroy();
            mThinWebView = null;
        }
    }

    private void handlePrivacyPolicyLink() {
        mLearnMoreBullet1Description =
                mContentView.findViewById(
                        R.id
                                .privacy_sandbox_m1_notice_eea_site_suggested_ads_learn_more_bullet_one_description);
        mLearnMoreBullet1Description.setText(
                SpanApplier.applySpans(
                        mLearnMoreBullet1Description.getText().toString(),
                        new SpanApplier.SpanInfo(
                                "<link>",
                                "</link>",
                                new ChromeClickableSpan(
                                        getContext(), this::onPrivacyPolicyClicked))));
        mLearnMoreBullet1Description.setMovementMethod(LinkMovementMethod.getInstance());
        if (mThinWebView == null || mWebContents == null || mWebContents.isDestroyed()) {
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
                                    "PrivacySandbox.PrivacyPolicy.FailedLoadErrorCode", errorCode);
                        }
                    };
            mThinWebView =
                    PrivacySandboxDialogController.createPrivacyPolicyThinWebView(
                            mWebContents, mProfile, mActivityWindowAndroid);
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
