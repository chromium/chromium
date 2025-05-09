// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import androidx.annotation.IdRes;
import androidx.annotation.IntDef;
import androidx.annotation.LayoutRes;
import androidx.annotation.VisibleForTesting;

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
import org.chromium.ui.widget.TextViewWithLeading;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.List;

// TODO(crbug.com/392943234): Update this class's naming and description when naming is finalized.
/**
 * The {@code PrivacySandboxDialogV3} class is responsible for parsing XML layout files, handling
 * click events, and element visibility for Privacy Sandbox dialogs.
 *
 * <p>The class primarily looks for specifically named elements that serve as interactive controls
 * or important display areas within the dialog.
 *
 * <p><b>Supported XML elements (by {@code android:id}):</b>
 *
 * <ul>
 *   <li>{@code ack_button}: The acknowledgement button component.
 *   <li>{@code no_button}: The `no` action button component.
 *   <li>{@code settings_button}: The settings button component.
 *   <li>{@code more_button}: The button that scrolls the screen down when clicked.
 *   <li>{@code action_buttons}: The layout that holds the action buttons listed above.
 *   <li>{@code action_button_divider}: The divider seperating the scroll view and the action
 *       buttons.
 *   <li>{@code dropdown_element}: The dropdown element component.
 *   <li>{@code privacy_policy_back_button}: The back button component specifically within the
 *       privacy policy view.
 *   <li>{@code privacy_policy_text}: The text view component displaying the privacy policy content.
 * </ul>
 */
@NullMarked
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

    private final @PrivacySandboxDialogType int mDialogType;

    private final int mSurfaceType;
    private View mContentView;
    private final View.OnClickListener mOnClickListener;

    private LinearLayout mViewContainer;
    private ButtonCompat mMoreButton;
    // Determines if we've shown the action button before, and if so we should always show it.
    private boolean mShouldShowActionButtons;
    private LinearLayout mActionButtons;
    private final ScrollView mScrollView;
    private View mBottomFade;

    // Dropdown elements
    private LinearLayout mDropdownElement;
    private @Nullable CheckableImageView mDropdownExpandArrowView;
    private @Nullable LinearLayout mDropdownContentContainer;

    // Privacy policy
    private boolean mIsPrivacyPageLoaded;
    private LinearLayout mPrivacyPolicyView;
    private @Nullable FrameLayout mPrivacyPolicyContent;
    private @Nullable ThinWebView mThinWebView;
    private @Nullable WebContents mWebContents;
    private @Nullable WebContentsObserver mWebContentsObserver;
    private final @IdRes int mPrivacyPolicyTextIdRes = R.id.privacy_policy_text;

    private final ActivityWindowAndroid mActivityWindowAndroid;
    private final Profile mProfile;

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

        mActivityWindowAndroid = activityWindowAndroid;
        mProfile = profile;

        fetchDialogContent(activity);
        mScrollView = mContentView.findViewById(R.id.privacy_sandbox_dialog_scroll_view);
        mOnClickListener = getOnClickListener();
        registerDialogButtons();
        registerDropdownElements(activity);
        registerPrivacyPolicy();
        setOnShowListener(this);
        setCancelable(false);
    }

    private void registerScrollViewListeners() {
        mShouldShowActionButtons = false;
        mScrollView.addOnLayoutChangeListener(
                new View.OnLayoutChangeListener() {
                    @Override
                    public void onLayoutChange(
                            View v,
                            int left,
                            int top,
                            int right,
                            int bottom,
                            int oldLeft,
                            int oldTop,
                            int oldRight,
                            int oldBottom) {
                        // Only update button visibility if the main dialog is visible.
                        if (mViewContainer.getVisibility() != View.VISIBLE) {
                            return;
                        }
                        // Wait for the scroll view to fully load before updating button visibility.
                        mScrollView.post(
                                () -> {
                                    updateButtonVisibility();
                                });
                    }
                });
        mScrollView
                .getViewTreeObserver()
                .addOnScrollChangedListener(
                        () -> {
                            if (!canScrollVerticallyDown()) {
                                setMoreButtonVisibility(View.GONE);
                                setActionButtonsVisibility(View.VISIBLE);
                                mScrollView.post(
                                        () -> {
                                            // Ensure we're at the very bottom of the scroll view.
                                            mScrollView.pageScroll(ScrollView.FOCUS_DOWN);
                                        });
                            }
                        });
    }

    private void updateButtonVisibility() {
        // Display the action buttons if we've displayed it before or if we cannot scroll vertically
        // down (we've hit the end of the dialog).
        if (mShouldShowActionButtons || !canScrollVerticallyDown()) {
            setMoreButtonVisibility(View.GONE);
            setActionButtonsVisibility(View.VISIBLE);
        } else {
            // Handle the case where we can still scroll down - display the `More` button.
            setMoreButtonVisibility(View.VISIBLE);
            setActionButtonsVisibility(View.GONE);
        }
    }

    private void setMoreButtonVisibility(int visibility) {
        mMoreButton.setVisibility(visibility);
        // The bottom fade should always match the visibility value for the more button.
        mBottomFade.setVisibility(visibility);
    }

    private void setActionButtonsVisibility(int visibility) {
        mActionButtons.setVisibility(visibility);
        View divider = mContentView.findViewById(R.id.action_button_divider);
        if (mActionButtons.getVisibility() == View.VISIBLE) {
            mShouldShowActionButtons = true;
            // Zero out the padding for the scroll view content if any was applied.
            mScrollView.getChildAt(0).setPadding(0, 0, 0, 0);
            if (divider != null) {
                // The divider view should be a child of the root element such that it's width
                // matches the screen's width.
                divider.setVisibility(View.VISIBLE);
                // Position the divider such that it divides the scroll view and the action buttons.
                divider.setY(mScrollView.getBottom());
            }
        } else if (divider != null) {
            divider.setVisibility(View.GONE);
        }
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
                contentToInflate = R.layout.privacy_sandbox_notice_row_v3;
                break;
            case PrivacySandboxDialogType.RESTRICTED_NOTICE:
                contentToInflate = R.layout.privacy_sandbox_notice_restricted_v3;
                break;
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
        assumeNonNull(mDropdownContentContainer);
        mDropdownContentContainer.setVisibility(View.VISIBLE);
        // TODO(crbug.com/392943234): Take in the dropdown resource as input within the constructor
        @LayoutRes int resourceToInflate;
        switch (mDialogType) {
            case PrivacySandboxDialogType.EEA_CONSENT:
                resourceToInflate = R.layout.privacy_sandbox_consent_eea_dropdown_v3;
                break;
            case PrivacySandboxDialogType.EEA_NOTICE:
                resourceToInflate = R.layout.privacy_sandbox_notice_eea_dropdown_v3;
                break;
            case PrivacySandboxDialogType.ROW_NOTICE:
                resourceToInflate = R.layout.privacy_sandbox_notice_row_dropdown_v3;
                break;
            case PrivacySandboxDialogType.RESTRICTED_NOTICE:
            default:
                // TODO(crbug.com/392943234): Don't default to the eea dropdown.
                resourceToInflate = R.layout.privacy_sandbox_consent_eea_dropdown_v3;
                // TODO(crbug.com/392943234): Emit a histogram if we hit the default cause.
        }
        LayoutInflater.from(getContext()).inflate(resourceToInflate, mDropdownContentContainer);
        mScrollView.post(() -> mScrollView.scrollTo(0, mDropdownElement.getTop()));
        // Attempt to register the privacy policy if it exists in the dropdown content.
        registerPrivacyPolicy();
    }

    private void handleDropdownClick(View view) {
        if (isDropdownExpanded()) {
            assumeNonNull(mDropdownContentContainer);
            mDropdownContentContainer.setVisibility(View.GONE);
            mDropdownContentContainer.removeAllViews();
        } else {
            inflateDropdownContent();
        }

        assumeNonNull(mDropdownExpandArrowView);
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
        assumeNonNull(mPrivacyPolicyContent);
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
        assumeNonNull(mPrivacyPolicyContent);
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

    private void registerPrivacyPolicy() {
        mPrivacyPolicyView = mContentView.findViewById(R.id.privacy_policy_view);
        if (mPrivacyPolicyView == null) {
            return;
        }
        mPrivacyPolicyContent = mContentView.findViewById(R.id.privacy_policy_content);
        mContentView
                .findViewById(R.id.privacy_policy_back_button)
                .setOnClickListener(mOnClickListener);
        mIsPrivacyPageLoaded = false;
        createPrivacyPolicyLink(mProfile, mActivityWindowAndroid);
    }

    private void createPrivacyPolicyLink(
            Profile profile, ActivityWindowAndroid activityWindowAndroid) {
        TextViewWithLeading privacyPolicyTextView =
                mContentView.findViewById(mPrivacyPolicyTextIdRes);
        if (privacyPolicyTextView == null) {
            return;
        }
        String privacyPolicyText = privacyPolicyTextView.getText().toString();
        // The privacy policy should have link tags before attempting to create clickable
        // spans.
        if (!privacyPolicyText.contains("<link>") || !privacyPolicyText.contains("</link>")) {
            return;
        }
        privacyPolicyTextView.setText(
                SpanApplier.applySpans(
                        privacyPolicyText,
                        new SpanApplier.SpanInfo(
                                "<link>",
                                "</link>",
                                new ChromeClickableSpan(
                                        getContext(), this::onPrivacyPolicyClicked))));
        privacyPolicyTextView.setMovementMethod(LinkMovementMethod.getInstance());
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

    private void initButtonState() {
        setMoreButtonVisibility(View.GONE);
        mActionButtons.setVisibility(View.GONE);
        // Don't need to check for padding if content is already scrollable.
        if (canScrollVerticallyDown()) {
            registerScrollViewListeners();
            return;
        }
        mActionButtons.setVisibility(View.INVISIBLE);
        // Wait until the action buttons have been updated.
        mActionButtons.post(
                () -> {
                    mScrollView.post(
                            () -> {
                                // If we can scroll down this indicates that showing the action
                                // buttons will resize the scrollview to be scrollable.
                                if (canScrollVerticallyDown()) {
                                    View dialog =
                                            mContentView.findViewById(R.id.privacy_sandbox_dialog);
                                    ViewGroup scrollViewChild =
                                            (ViewGroup) mScrollView.getChildAt(0);
                                    View lastElementView =
                                            scrollViewChild.getChildAt(
                                                    scrollViewChild.getChildCount() - 1);
                                    // The amount of pixels from the bottom of the last element in
                                    // the dialog to the bottom of the dialog.
                                    int pixelsPaddingToMakeScrollable =
                                            dialog.getBottom() - lastElementView.getBottom();
                                    // Add padding to make the scroll view scrollable.
                                    mScrollView
                                            .getChildAt(0)
                                            .setPadding(
                                                    /* left= */ 0,
                                                    /* top= */ 0,
                                                    /* right= */ 0,
                                                    /* bottom= */ pixelsPaddingToMakeScrollable);
                                }
                                mActionButtons.setVisibility(View.GONE);
                                // Start listening to render changes only after the initial render.
                                registerScrollViewListeners();
                            });
                });
    }

    @Override
    public void onShow(DialogInterface dialogInterface) {
        initButtonState();
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

    private boolean isDropdownExpanded() {
        return mDropdownContentContainer != null
                && mDropdownContentContainer.getVisibility() == View.VISIBLE;
    }
}
