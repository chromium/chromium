// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.metrics.OnBoarding;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayCoordinator;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayModel;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayState;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.util.Arrays;

/**
 * Coordinator responsible for showing the onboarding screen when the user is using the Autofill
 * Assistant for the first time.
 */
class AssistantOnboardingCoordinator {
    private static final String SMALL_ONBOARDING_EXPERIMENT_ID = "4257013";

    private final String mExperimentIds;
    private final Context mContext;
    private final BottomSheetController mController;
    @Nullable
    private final Tab mTab;

    @Nullable
    private AssistantOverlayCoordinator mOverlayCoordinator;

    @Nullable
    private AssistantBottomSheetContent mContent;
    private boolean mAnimate = true;

    private boolean mOnboardingShown;

    AssistantOnboardingCoordinator(String experimentIds, Context context,
            BottomSheetController controller, @Nullable Tab tab) {
        mExperimentIds = experimentIds;
        mContext = context;
        mController = controller;
        mTab = tab;
    }

    /**
     * Shows onboarding and provides the result to the given callback.
     *
     * <p>The {@code callback} will be called with true or false when the user accepts or cancels
     * the onboarding (respectively).
     *
     * <p>Note that the bottom sheet will be hidden after the callback returns. Call, from the
     * callback, {@link #hide} to hide it earlier or {@link #transferControls} to take ownership of
     * it and possibly keep it past the end of the callback.
     */
    void show(Callback<Boolean> callback) {
        AutofillAssistantMetrics.recordOnBoarding(OnBoarding.OB_SHOWN);
        mOnboardingShown = true;

        if (mTab != null) {
            // If there's a tab, cover it with an overlay.
            AssistantOverlayModel overlayModel = new AssistantOverlayModel();
            mOverlayCoordinator = new AssistantOverlayCoordinator(mTab.getActivity(), overlayModel);
            overlayModel.set(AssistantOverlayModel.STATE, AssistantOverlayState.FULL);
        }
        mContent = new AssistantBottomSheetContent(mContext);
        initContent(callback);
        BottomSheetUtils.showContentAndExpand(mController, mContent, mAnimate);
    }

    /**
     * Transfers ownership of the controls to the caller, returns the overlay coordinator, if one
     * was created.
     *
     * <p>This call is only useful when called from inside a callback provided to {@link #show}, as
     * before that there are no controls and after that the coordinator automatically hides them.
     * This call allows callbacks to reuse the controls setup for onboarding and provide a smooth
     * transition.
     */
    @Nullable
    AssistantOverlayCoordinator transferControls() {
        assert isInProgress();

        AssistantOverlayCoordinator coordinator = mOverlayCoordinator;
        mOverlayCoordinator = null;
        mContent = null;
        return coordinator;
    }

    /** Hides the UI, if one is shown. */
    void hide() {
        if (mOverlayCoordinator != null) {
            mOverlayCoordinator.destroy();
            mOverlayCoordinator = null;
        }

        if (mContent != null) {
            mController.hideContent(mContent, /* animate= */ mAnimate);
            mContent = null;
        }
    }

    /**
     * Returns {@code true} between the time {@link #show} is called and the time
     * the callback has returned.
     */
    boolean isInProgress() {
        return mContent != null;
    }

    /** Don't animate the bottom sheet expansion. */
    @VisibleForTesting
    void disableAnimationForTesting() {
        mAnimate = false;
    }

    /**
     * Returns {@code true} if the onboarding has been shown at the beginning when this
     * autofill assistant flow got triggered.
     */
    boolean getOnboardingShown() {
        return mOnboardingShown;
    }

    /**
     * Set the content of the bottom sheet to be the Autofill Assistant onboarding.
     */
    private void initContent(Callback<Boolean> callback) {
        ScrollView initView = (ScrollView) LayoutInflater.from(mContext).inflate(
                R.layout.autofill_assistant_onboarding, /* root= */ null);

        TextView termsTextView = initView.findViewById(R.id.google_terms_message);
        String termsString = mContext.getApplicationContext().getString(
                R.string.autofill_assistant_google_terms_description);

        NoUnderlineClickableSpan termsSpan = new NoUnderlineClickableSpan(mContext.getResources(),
                (widget)
                        -> CustomTabActivity.showInfoPage(mContext.getApplicationContext(),
                                mContext.getApplicationContext().getString(
                                        R.string.autofill_assistant_google_terms_url)));
        SpannableString spannableMessage = SpanApplier.applySpans(
                termsString, new SpanApplier.SpanInfo("<link>", "</link>", termsSpan));
        termsTextView.setText(spannableMessage);
        termsTextView.setMovementMethod(LinkMovementMethod.getInstance());

        // Set focusable for accessibility.
        initView.setFocusable(true);

        initView.findViewById(R.id.button_init_ok)
                .setOnClickListener(unusedView -> onClicked(true, callback));
        initView.findViewById(R.id.button_init_not_ok)
                .setOnClickListener(unusedView -> onClicked(false, callback));

        // Hide views that should not be displayed when showing the small onboarding.
        if (Arrays.asList(mExperimentIds.split(",")).contains(SMALL_ONBOARDING_EXPERIMENT_ID)) {
            hide(initView, R.id.onboarding_image);
            hide(initView, R.id.onboarding_subtitle);
            hide(initView, R.id.onboarding_separator);
        }

        mContent.setContent(initView, initView);
    }

    private static void hide(View root, int resId) {
        root.findViewById(resId).setVisibility(View.GONE);
    }

    private void onClicked(boolean accept, Callback<Boolean> callback) {
        AutofillAssistantPreferencesUtil.setInitialPreferences(accept);
        AutofillAssistantMetrics.recordOnBoarding(
                accept ? OnBoarding.OB_ACCEPTED : OnBoarding.OB_CANCELLED);
        callback.onResult(accept);
        hide();
    }
}
