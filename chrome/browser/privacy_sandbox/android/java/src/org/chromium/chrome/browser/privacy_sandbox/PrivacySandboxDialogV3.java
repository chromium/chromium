// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.app.Activity;
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
import androidx.annotation.LayoutRes;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

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
import org.chromium.ui.widget.TextViewWithLeading;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.List;

// TODO(crbug.com/392943234): Update this class's naming and description when naming is finalized.
/** Handles logic for the Privacy Sandbox Ads consents/notices dialogs. */
public class PrivacySandboxDialogV3 extends ChromeDialog implements DialogInterface.OnShowListener {

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

    private int mSurfaceType;
    private View mContentView;
    private View.OnClickListener mOnClickListener;

    private LinearLayout mViewContainer;
    private ButtonCompat mMoreButton;
    // Determines if we've shown the action button before, and if so we should always show it.
    private boolean mShouldShowActionButtons;
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

    // TODO(crbug.com/392943234): Update the constructor to accept a layoutRes required for the
    // dialog.
    public PrivacySandboxDialogV3(
            Activity activity,
            Profile profile,
            ActivityWindowAndroid activityWindowAndroid,
            @SurfaceType int surfaceType,
            @PrivacySandboxDialogType int dialogType) {
        super(
                activity,
                R.style.ThemeOverlay_BrowserUI_Fullscreen,
                EdgeToEdgeUtils.isEdgeToEdgeEverywhereEnabled());
        mDialogType = dialogType;
        mSurfaceType = surfaceType;

        fetchDialogContent(activity);
        mOnClickListener = getOnClickListener();
        registerDialogButtons();
        registerDropdownElements(activity);
        registerPrivacyPolicy(profile, activityWindowAndroid);

        mScrollView = mContentView.findViewById(R.id.privacy_sandbox_dialog_scroll_view);
        mScrollView
                .getViewTreeObserver()
                .addOnScrollChangedListener(
                        () -> {
                            if (!canScrollVerticallyDown()) {
                                setMoreButtonVisibility(View.GONE);
                                mActionButtons.setVisibility(View.VISIBLE);
                                // Set the flag to always show the action buttons if we re-render.
                                mShouldShowActionButtons = true;
                                mScrollView.post(
                                        () -> {
                                            mScrollView.pageScroll(ScrollView.FOCUS_DOWN);
                                        });
                            }
                        });

        setOnShowListener(this);
        setCancelable(false);
    }

    private void updateButtonVisibility() {
        // Display the action buttons if we've displayed it before or if we cannot scroll vertically
        // down (we've hit the end of the dialog).
        if (mShouldShowActionButtons || !canScrollVerticallyDown()) {
            mShouldShowActionButtons = true;
            setMoreButtonVisibility(View.GONE);
            mActionButtons.setVisibility(View.VISIBLE);
        } else {
            // Handle the case where we can still scroll down - display the `More` button.
            setMoreButtonVisibility(View.VISIBLE);
            mActionButtons.setVisibility(View.GONE);
        }
    }

    private void setMoreButtonVisibility(int visibility) {
        mMoreButton.setVisibility(visibility);
        // The bottom fade should always match the visibility value for the more button.
        mBottomFade.setVisibility(visibility);
    }

    private void fetchDialogContent(Context context) {
        // TODO(crbug.com/392943234): Update function to accept the Layoutres and remove the switch
        // statement.
        // We're currently keeping the logic as is since the resources are not used anywhere else
        // (unused warnings will trigger).
        @LayoutRes int contentToInflate;
        switch (mDialogType) {
            case PrivacySandboxDialogType.EEA_CONSENT:
                contentToInflate = R.layout.privacy_sandbox_consent_eea_v3;
                break;
            case PrivacySandboxDialogType.EEA_NOTICE:
                contentToInflate = R.layout.privacy_sandbox_notice_eea_v3;
                break;
            case PrivacySandboxDialogType.ROW_NOTICE:
            case PrivacySandboxDialogType.RESTRICTED_NOTICE:
            default:
                // TODO(crbug.com/392943234): Don't default to the eea consent
                contentToInflate = R.layout.privacy_sandbox_consent_eea_v3;
                // TODO(crbug.com/392943234): Emit a histogram if we hit the default cause.
                throw new IllegalStateException(
                        "[PrivacySandboxDialog] Invalid dialog content requested.");
        }
        mContentView = LayoutInflater.from(context).inflate(contentToInflate, null);
        mViewContainer = mContentView.findViewById(R.id.privacy_sandbox_dialog_view);
        setContentView(mContentView);
    }

    private void registerDialogButtons() {
        // Process buttons that exists in all dialogs.
        ButtonCompat ackButton = mContentView.findViewById(R.id.ack_button);
        mActionButtons = mContentView.findViewById(R.id.action_buttons);
        mMoreButton = mContentView.findViewById(R.id.more_button);
        mBottomFade = mContentView.findViewById(R.id.bottom_fade);

        ackButton.setOnClickListener(mOnClickListener);
        mMoreButton.setOnClickListener(mOnClickListener);

        // Conditionally register the other CTA button if it exists.
        List<Integer> buttonIds = Arrays.asList(R.id.settings_button, R.id.no_button);
        ButtonCompat button;
        for (int buttonId : buttonIds) {
            button = mContentView.findViewById(buttonId);
            if (button != null) {
                button.setOnClickListener(mOnClickListener);
            }
        }
    }

    private void registerDropdownElements(Context context) {
        mDropdownElement = mContentView.findViewById(R.id.dropdown_element);
        if (mDropdownElement == null) {
            return;
        }
        mDropdownElement.setOnClickListener(mOnClickListener);
        mDropdownContentContainer = mContentView.findViewById(R.id.dropdown_container);
        mDropdownExpandArrowView = mContentView.findViewById(R.id.dropdown_element_expand_arrow);
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

    private void handleSettingsButtonClick() {
        // TODO(crbug.com/392943234): Record that the settings button was clicked.
        dismiss();
        PrivacySandboxSettingsBaseFragment.launchPrivacySandboxSettings(
                getContext(), PrivacySandboxReferrer.PRIVACY_SANDBOX_NOTICE);
    }

    private void handleMoreButtonClick() {
        // TODO(crbug.com/392943234): Record that more button was clicked.
        mScrollView.post(() -> mScrollView.pageScroll(ScrollView.FOCUS_DOWN));
    }

    private void inflateDropdownContent() {
        mDropdownContentContainer.setVisibility(View.VISIBLE);
        @LayoutRes int resourceToInflate;
        switch (mDialogType) {
            case PrivacySandboxDialogType.EEA_CONSENT:
                resourceToInflate = R.layout.privacy_sandbox_consent_eea_dropdown_v3;
                break;
            case PrivacySandboxDialogType.EEA_NOTICE:
                resourceToInflate = R.layout.privacy_sandbox_notice_eea_dropdown_v3;
                break;
            case PrivacySandboxDialogType.ROW_NOTICE:
            case PrivacySandboxDialogType.RESTRICTED_NOTICE:
            default:
                // TODO(crbug.com/392943234): Don't default to the eea dropdown.
                resourceToInflate = R.layout.privacy_sandbox_consent_eea_dropdown_v3;
                // TODO(crbug.com/392943234): Emit a histogram if we hit the default cause.
        }
        LayoutInflater.from(getContext()).inflate(resourceToInflate, mDropdownContentContainer);
        mScrollView.post(() -> mScrollView.scrollTo(0, mDropdownElement.getTop()));
    }

    private void handleDropdownClick(View view) {
        if (isDropdownExpanded()) {
            mDropdownContentContainer.setVisibility(View.GONE);
            mDropdownContentContainer.removeAllViews();
        } else {
            inflateDropdownContent();
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
        updateButtonVisibility();
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
            // Hide the `more` button which may be shown at the top of the screen if the user has
            // not reached the button of the previous dialog.
            setMoreButtonVisibility(View.GONE);
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
        mContentView
                .findViewById(R.id.privacy_policy_back_button)
                .setOnClickListener(mOnClickListener);
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

    // TODO(crbug.com/403286432): Remove this function after refactoring the debouncing class is
    // done.
    private String getDialogName() {
        String dialogName = "";
        switch (mDialogType) {
            case PrivacySandboxDialogType.EEA_CONSENT:
                dialogName = "TopicsConsentModal";
                break;
            case PrivacySandboxDialogType.EEA_NOTICE:
                dialogName = "ProtectedAudienceMeasurementNoticeModal";
                break;
            case PrivacySandboxDialogType.ROW_NOTICE:
                dialogName = "ThreeAdsAPIsNoticeModal";
                break;
            case PrivacySandboxDialogType.RESTRICTED_NOTICE:
                dialogName = "MeasurementNoticeModal";
                break;
            case PrivacySandboxDialogType.UNKNOWN:
            case PrivacySandboxDialogType.MAX_VALUE:
            default:
                break;
        }
        return dialogName + PrivacySandboxDialogUtils.getSurfaceTypeAsString(mSurfaceType);
    }

    private View.OnClickListener getOnClickListener() {
        return new PrivacySandboxDebouncedOnClick(getDialogName()) {
            @Override
            public void processClick(View v) {
                processClickImpl(v);
            }
        };
    }

    public void processClickImpl(View view) {
        int id = view.getId();
        if (id == R.id.ack_button) {
            handleAckButtonClick();
        } else if (id == R.id.no_button) {
            handleNoButtonClick();
        } else if (id == R.id.settings_button) {
            handleSettingsButtonClick();
        } else if (id == R.id.more_button) {
            handleMoreButtonClick();
        } else if (id == R.id.dropdown_element) {
            handleDropdownClick(view);
        } else if (id == R.id.privacy_policy_back_button) {
            handlePrivacyPolicyBackButtonClicked();
        }
    }

    @Override
    public void onShow(DialogInterface dialogInterface) {
        updateButtonVisibility();
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
