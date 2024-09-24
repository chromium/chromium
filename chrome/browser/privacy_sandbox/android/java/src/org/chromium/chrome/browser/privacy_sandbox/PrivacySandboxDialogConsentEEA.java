// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.content.DialogInterface;
import android.os.Handler;
import android.text.method.LinkMovementMethod;
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
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.CheckableImageView;
import org.chromium.ui.widget.ChromeImageButton;
import org.chromium.ui.widget.TextViewWithLeading;
import org.chromium.url.GURL;

/** Dialog in the form of a consent shown for the Privacy Sandbox. */
public class PrivacySandboxDialogConsentEEA extends ChromeDialog
        implements View.OnClickListener, DialogInterface.OnShowListener {
    private static final int SPINNER_DURATION_MS = 1500;
    private static final int BACKGROUND_TRANSITION_DURATION_MS = 300;

    private final PrivacySandboxBridge mPrivacySandboxBridge;

    private View mContentView;

    private final CheckableImageView mExpandArrowView;
    private LinearLayout mDropdownContainer;
    private LinearLayout mDropdownElement;
    private LinearLayout mProgressBarContainer;
    private LinearLayout mConsentViewContainer;
    private ButtonCompat mMoreButton;
    private LinearLayout mActionButtons;
    private ScrollView mScrollView;
    private TextViewWithLeading mLearnMoreText;
    private int mSurfaceType;
    private LinearLayout mPrivacyPolicyView;
    private FrameLayout mPrivacyPolicyContent;
    private ChromeImageButton mPrivacyPolicyBackButton;

    private ThinWebView mThinWebView;
    private WebContents mWebContents;
    private WebContentsObserver mWebContentsObserver;
    private long mPrivacyPolicyClickedTimestamp;
    private final Profile mProfile;
    private final ActivityWindowAndroid mActivityWindowAndroid;
    private boolean mIsPrivacyPageLoaded;

    private boolean mAreAnimationsDisabled;

    public PrivacySandboxDialogConsentEEA(
            Context context,
            PrivacySandboxBridge privacySandboxBridge,
            boolean disableAnimations,
            @SurfaceType int surfaceType,
            Profile profile,
            ActivityWindowAndroid activityWindowAndroid) {
        super(context, R.style.ThemeOverlay_BrowserUI_Fullscreen);
        mPrivacySandboxBridge = privacySandboxBridge;
        mAreAnimationsDisabled = disableAnimations;
        mSurfaceType = surfaceType;
        mProfile = profile;
        mActivityWindowAndroid = activityWindowAndroid;
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
        mPrivacyPolicyView = mContentView.findViewById(R.id.privacy_policy_view);
        mPrivacyPolicyContent = mContentView.findViewById(R.id.privacy_policy_content);
        mPrivacyPolicyBackButton = mContentView.findViewById(R.id.privacy_policy_back_button);
        mPrivacyPolicyBackButton.setOnClickListener(this);
        mIsPrivacyPageLoaded = false;

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
        mPrivacySandboxBridge.promptActionOccurred(PromptAction.CONSENT_SHOWN, mSurfaceType);
        super.show();
    }

    // OnClickListener:
    @Override
    public void onClick(View view) {
        int id = view.getId();
        if (id == R.id.ack_button) {
            mPrivacySandboxBridge.promptActionOccurred(PromptAction.CONSENT_ACCEPTED, mSurfaceType);
            dismissAndMaybeShowNotice();
        } else if (id == R.id.no_button) {
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
                handlePrivacyPolicyFeature();
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
        } else if (id == R.id.privacy_policy_back_button) {
            handlePrivacyPolicyBackButtonClicked();
        }
    }

    private void handlePrivacyPolicyBackButtonClicked() {
        mPrivacyPolicyView.setVisibility(View.GONE);
        mPrivacyPolicyContent.removeAllViews();
        mConsentViewContainer.setVisibility(View.VISIBLE);
    }

    private void handlePrivacyPolicyFeature() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PRIVACY_SANDBOX_PRIVACY_POLICY)) {
            mLearnMoreText = mContentView.findViewById(R.id.privacy_sandbox_learn_more_text);
            mLearnMoreText.setText(
                    SpanApplier.applySpans(
                            getContext()
                                    .getResources()
                                    .getString(
                                            R.string.privacy_sandbox_m1_notice_learn_more_v2_clank),
                            new SpanApplier.SpanInfo(
                                    "<link>",
                                    "</link>",
                                    new NoUnderlineClickableSpan(
                                            getContext(), this::onPrivacyPolicyClicked))));
            mLearnMoreText.setMovementMethod(LinkMovementMethod.getInstance());
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
                // TODO(crbug.com/366010532): Add in functionality to add language code to the url
                mThinWebView =
                        PrivacySandboxDialogController.createThinWebView(
                                mWebContents, mProfile, mActivityWindowAndroid, privacyPolicyUrl);
            }
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
     * @param view The View that was clicked (typically the TextView containing the link).
     */
    private void onPrivacyPolicyClicked(View view) {
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
            mWebContents.destroy();
            mWebContents = null;
            mWebContentsObserver.destroy();
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
                getContext(), mPrivacySandboxBridge, mSurfaceType);
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
