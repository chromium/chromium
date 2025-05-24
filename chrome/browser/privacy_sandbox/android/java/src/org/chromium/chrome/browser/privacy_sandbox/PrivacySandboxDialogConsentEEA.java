// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Handler;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import androidx.annotation.IdRes;
import androidx.annotation.StringRes;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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

/** Dialog in the form of a consent shown for the Privacy Sandbox. */
@NullMarked
public class PrivacySandboxDialogConsentEEA extends ChromeDialog
        implements DialogInterface.OnShowListener {
    private static final int SPINNER_DURATION_MS = 1500;
    private static final int BACKGROUND_TRANSITION_DURATION_MS = 300;

    private final PrivacySandboxBridge mPrivacySandboxBridge;

    private final Activity mActivity;
    private final View mContentView;

    private final CheckableImageView mExpandArrowView;
    private final LinearLayout mDropdownContainer;
    private final LinearLayout mDropdownElement;
    private final LinearLayout mProgressBarContainer;
    private final LinearLayout mConsentViewContainer;
    private final ButtonCompat mMoreButton;
    private final LinearLayout mActionButtons;
    private final ScrollView mScrollView;
    private @Nullable TextViewWithLeading mLearnMoreText;
    private final int mSurfaceType;
    private final LinearLayout mPrivacyPolicyView;
    private final FrameLayout mPrivacyPolicyContent;
    private final ChromeImageButton mPrivacyPolicyBackButton;
    private final View.OnClickListener mOnClickListener;

    private @Nullable ThinWebView mThinWebView;
    private @Nullable WebContents mWebContents;
    private @Nullable WebContentsObserver mWebContentsObserver;
    private long mPrivacyPolicyClickedTimestamp;
    private final Profile mProfile;
    private final ActivityWindowAndroid mActivityWindowAndroid;
    private boolean mIsPrivacyPageLoaded;

    private final boolean mAreAnimationsDisabled;

    private @StringRes int mLearnMoreBullet1StringRes =
            R.string.privacy_sandbox_m1_consent_learn_more_bullet_1;
    private @StringRes int mLearnMoreBullet2StringRes =
            R.string.privacy_sandbox_m1_consent_learn_more_bullet_2;
    private @StringRes int mLearnMoreBullet3StringRes =
            R.string.privacy_sandbox_m1_consent_learn_more_bullet_3;
    private @IdRes int mLearnMoreTextIdRes = R.id.privacy_sandbox_learn_more_text;
    private @StringRes int mLearnMoreLinkString =
            R.string.privacy_sandbox_m1_notice_learn_more_v2_clank;

    public PrivacySandboxDialogConsentEEA(
            Activity activity,
            PrivacySandboxBridge privacySandboxBridge,
            boolean disableAnimations,
            @SurfaceType int surfaceType,
            Profile profile,
            ActivityWindowAndroid activityWindowAndroid) {
        super(
                activity,
                R.style.ThemeOverlay_BrowserUI_Fullscreen,
                EdgeToEdgeUtils.isEdgeToEdgeEverywhereEnabled());
        mActivity = activity;
        mPrivacySandboxBridge = privacySandboxBridge;
        mAreAnimationsDisabled = disableAnimations;
        mSurfaceType = surfaceType;
        mProfile = profile;
        mActivityWindowAndroid = activityWindowAndroid;
        mContentView =
                LayoutInflater.from(mActivity).inflate(R.layout.privacy_sandbox_consent_eea, null);
        setContentView(mContentView);
        mOnClickListener = getOnClickListener();

        ButtonCompat ackButton = mContentView.findViewById(R.id.ack_button);
        ackButton.setOnClickListener(mOnClickListener);
        ButtonCompat noButton = mContentView.findViewById(R.id.no_button);
        noButton.setOnClickListener(mOnClickListener);
        mMoreButton = mContentView.findViewById(R.id.more_button);
        mActionButtons = mContentView.findViewById(R.id.action_buttons);
        mScrollView = mContentView.findViewById(R.id.privacy_sandbox_dialog_scroll_view);

        mProgressBarContainer = mContentView.findViewById(R.id.progress_bar_container);
        mConsentViewContainer = mContentView.findViewById(R.id.privacy_sandbox_consent_eea_view);
        mPrivacyPolicyView = mContentView.findViewById(R.id.privacy_policy_view);
        mPrivacyPolicyContent = mContentView.findViewById(R.id.privacy_policy_content);
        mPrivacyPolicyBackButton = mContentView.findViewById(R.id.privacy_policy_back_button);
        mPrivacyPolicyBackButton.setOnClickListener(mOnClickListener);
        mIsPrivacyPageLoaded = false;

        // Controls for the expanding section.
        mDropdownElement = mContentView.findViewById(R.id.dropdown_element);
        mDropdownElement.setOnClickListener(mOnClickListener);
        mDropdownContainer = mContentView.findViewById(R.id.dropdown_container);
        mExpandArrowView = mContentView.findViewById(R.id.expand_arrow);
        mExpandArrowView.setImageDrawable(PrivacySandboxDialogUtils.createExpandDrawable(activity));
        mExpandArrowView.setChecked(isDropdownExpanded());

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
        handleAdsApiUxEnhancements();
    }

    private View.OnClickListener getOnClickListener() {
        return new PrivacySandboxDebouncedOnClick(
                "TopicsConsentModal"
                        + PrivacySandboxDialogUtils.getSurfaceTypeAsString(mSurfaceType)) {
            @Override
            public void processClick(View v) {
                processClickImpl(v);
            }
        };
    }

    private void handleAdsApiUxEnhancements() {
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.PRIVACY_SANDBOX_ADS_API_UX_ENHANCEMENTS)) {
            return;
        }
        Context context = mActivityWindowAndroid.getContext().get();
        assumeNonNull(context);
        TextViewWithLeading description1 =
                mContentView.findViewById(R.id.privacy_sandbox_m1_consent_description_1);
        TextViewWithLeading description2 =
                mContentView.findViewById(R.id.privacy_sandbox_m1_consent_description_2);
        TextViewWithLeading description4 =
                mContentView.findViewById(R.id.privacy_sandbox_m1_consent_description_4);
        // Removing and modifying descriptions to be visible.
        description1.setVisibility(View.GONE);
        description2.setText(
                context.getString(R.string.privacy_sandbox_m1_consent_description_2_v2));
        description4.setText(
                context.getString(R.string.privacy_sandbox_m1_consent_description_4_v2));
        // Modifying the string used for the bullet points in the dropdown container.
        mLearnMoreBullet1StringRes = R.string.privacy_sandbox_m1_consent_learn_more_bullet_1_v2;
        mLearnMoreBullet2StringRes = R.string.privacy_sandbox_m1_consent_learn_more_bullet_2_v2;
        mLearnMoreBullet3StringRes = R.string.privacy_sandbox_m1_consent_learn_more_bullet_3_v2;
        // Modifying the id and string used for the privacy policy link
        mLearnMoreTextIdRes = R.id.privacy_sandbox_m1_consent_learn_more_bullet_2_description;
        mLearnMoreLinkString =
                R.string.privacy_sandbox_m1_consent_learn_more_bullet_2_description_clank;
        // Handling Ad Topics Content Parity feature - the changes are made on top of the changes to
        // the Ads API UX Enhancements.
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.PRIVACY_SANDBOX_AD_TOPICS_CONTENT_PARITY)) {
            description2.setText(
                    context.getString(
                            R.string.privacy_sandbox_m1_consent_description_1_content_parity));
            mLearnMoreLinkString =
                    R.string
                            .privacy_sandbox_m1_consent_learn_more_bullet_2_description_content_parity_clank;
        }
    }

    @Override
    public void show() {
        mPrivacySandboxBridge.promptActionOccurred(PromptAction.CONSENT_SHOWN, mSurfaceType);
        super.show();
    }

    public void processClickImpl(View view) {
        int id = view.getId();
        if (id == R.id.ack_button) {
            RecordUserAction.record("Settings.PrivacySandbox.ConsentDialog.AckClicked");
            mPrivacySandboxBridge.promptActionOccurred(PromptAction.CONSENT_ACCEPTED, mSurfaceType);
            dismissAndMaybeShowNotice();
        } else if (id == R.id.no_button) {
            RecordUserAction.record("Settings.PrivacySandbox.ConsentDialog.NoClicked");
            mPrivacySandboxBridge.promptActionOccurred(PromptAction.CONSENT_DECLINED, mSurfaceType);
            dismissAndMaybeShowNotice();
        } else if (id == R.id.more_button) {
            mPrivacySandboxBridge.promptActionOccurred(
                    PromptAction.CONSENT_MORE_BUTTON_CLICKED, mSurfaceType);
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
                        PromptAction.CONSENT_MORE_INFO_CLOSED, mSurfaceType);
                mDropdownContainer.setVisibility(View.GONE);
                mDropdownContainer.removeAllViews();
                mScrollView.post(
                        () -> {
                            mScrollView.fullScroll(ScrollView.FOCUS_DOWN);
                        });
            } else {
                mDropdownContainer.setVisibility(View.VISIBLE);
                mPrivacySandboxBridge.promptActionOccurred(
                        PromptAction.CONSENT_MORE_INFO_OPENED, mSurfaceType);
                LayoutInflater.from(getContext())
                        .inflate(R.layout.privacy_sandbox_consent_eea_dropdown, mDropdownContainer);

                PrivacySandboxDialogUtils.setBulletTextWithBoldContent(
                        getContext(),
                        mDropdownContainer,
                        R.id.privacy_sandbox_m1_consent_learn_more_bullet_one,
                        mLearnMoreBullet1StringRes);
                PrivacySandboxDialogUtils.setBulletTextWithBoldContent(
                        getContext(),
                        mDropdownContainer,
                        R.id.privacy_sandbox_m1_consent_learn_more_bullet_two,
                        mLearnMoreBullet2StringRes);
                PrivacySandboxDialogUtils.setBulletTextWithBoldContent(
                        getContext(),
                        mDropdownContainer,
                        R.id.privacy_sandbox_m1_consent_learn_more_bullet_three,
                        mLearnMoreBullet3StringRes);
                // Removing the old learn more text and setting the new one to be visible. These
                // changes aren't included in the handleAdsApiUxEnhancements function due to the
                // mDropdownContainer not containing any views until this point.
                if (ChromeFeatureList.isEnabled(
                        ChromeFeatureList.PRIVACY_SANDBOX_ADS_API_UX_ENHANCEMENTS)) {
                    mContentView.findViewById(mLearnMoreTextIdRes).setVisibility(View.VISIBLE);
                    mContentView
                            .findViewById(R.id.privacy_sandbox_learn_more_text)
                            .setVisibility(View.GONE);
                }

                mScrollView.post(
                        () -> {
                            mScrollView.scrollTo(0, mDropdownElement.getTop());
                        });
                handlePrivacyPolicyLink();
            }

            mExpandArrowView.setChecked(isDropdownExpanded());
            PrivacySandboxDialogUtils.updateDropdownControlContentDescription(
                    getContext(),
                    view,
                    isDropdownExpanded(),
                    R.string.privacy_sandbox_m1_consent_learn_more_expand_label);
            view.announceForAccessibility(
                    getContext()
                            .getString(
                                    isDropdownExpanded()
                                            ? R.string.accessibility_expanded_group
                                            : R.string.accessibility_collapsed_group));
        } else if (id == R.id.privacy_policy_back_button) {
            handlePrivacyPolicyBackButtonClicked();
        }
    }

    private void handlePrivacyPolicyBackButtonClicked() {
        mPrivacyPolicyView.setVisibility(View.GONE);
        mPrivacyPolicyContent.removeAllViews();
        mConsentViewContainer.setVisibility(View.VISIBLE);
    }

    private void handlePrivacyPolicyLink() {
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

    /**
     * Handles clicks on the Privacy Policy link. If a ThinWebView is available, loads and displays
     * the privacy policy within it, replacing the consent view.
     *
     * @param unused_view The View that was clicked (typically the TextView containing the link).
     */
    private void onPrivacyPolicyClicked(View unused_view) {
        RecordUserAction.record("Settings.PrivacySandbox.Consent.PrivacyPolicyLinkClicked");
        mPrivacyPolicyClickedTimestamp = System.currentTimeMillis();
        mPrivacyPolicyContent.removeAllViews();
        if (mThinWebView != null && mThinWebView.getView() != null) {
            mConsentViewContainer.setVisibility(View.GONE);
            mPrivacyPolicyContent.addView(mThinWebView.getView());
            mPrivacyPolicyView.setVisibility(View.VISIBLE);
        }
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
        PrivacySandboxDialogController.showNoticeEEA(
                mActivity, mPrivacySandboxBridge, mSurfaceType, mProfile, mActivityWindowAndroid);
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

    public int getSurfaceTypeForTesting() {
        return mSurfaceType;
    }
}
