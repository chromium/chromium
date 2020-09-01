// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.metrics.DropOutReason;
import org.chromium.chrome.browser.autofill_assistant.metrics.OnBoarding;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayCoordinator;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayModel;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayState;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.util.Arrays;
import java.util.Map;

/**
 * Coordinator responsible for showing the onboarding screen when the user is using the Autofill
 * Assistant for the first time.
 */
class AssistantOnboardingCoordinator {
    private static final String INTENT_IDENTFIER = "INTENT";
    private static final String BUY_MOVIE_TICKETS_INTENT = "BUY_MOVIE_TICKET";
    private static final String RENT_CAR_INTENT = "RENT_CAR";
    private static final String FLIGHTS_INTENT = "FLIGHTS_CHECKIN";
    private static final String PASSWORD_CHANGE_INTENT = "PASSWORD_CHANGE";
    private static final String FOOD_ORDERING_INTENT = "FOOD_ORDERING";
    private static final String VOICE_SEARCH_INTENT = "TELEPORT";
    private static final String SHOPPING_INTENT = "SHOPPING";
    private static final String SHOPPING_ASSISTED_CHECKOUT_INTENT = "SHOPPING_ASSISTED_CHECKOUT";
    private static final String BUY_MOVIE_TICKETS_EXPERIMENT_ID = "4363482";

    private final String mExperimentIds;
    private final Map<String, String> mParameters;
    private final Context mContext;
    private final BottomSheetController mController;
    private final BrowserControlsStateProvider mBrowserControls;
    private final CompositorViewHolder mCompositorViewHolder;
    private final ScrimCoordinator mScrimCoordinator;

    @Nullable
    private AssistantOverlayCoordinator mOverlayCoordinator;

    @Nullable
    private AssistantBottomSheetContent mContent;
    private boolean mAnimate = true;

    private boolean mOnboardingShown;

    AssistantOnboardingCoordinator(String experimentIds, Map<String, String> parameters,
            Context context, BottomSheetController controller,
            BrowserControlsStateProvider browserControls, CompositorViewHolder compositorViewHolder,
            ScrimCoordinator scrim) {
        mExperimentIds = experimentIds;
        mParameters = parameters;
        mContext = context;
        mController = controller;
        mBrowserControls = browserControls;
        mCompositorViewHolder = compositorViewHolder;
        mScrimCoordinator = scrim;
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

        // If there's a tab, cover it with an overlay.
        AssistantOverlayModel overlayModel = new AssistantOverlayModel();
        mOverlayCoordinator = new AssistantOverlayCoordinator(
                mContext, mBrowserControls, mCompositorViewHolder, mScrimCoordinator, overlayModel);
        overlayModel.set(AssistantOverlayModel.STATE, AssistantOverlayState.FULL);

        mContent =
                new AssistantBottomSheetContent(mContext, () -> new AssistantBottomBarDelegate() {
                    @Override
                    public boolean onBackButtonPressed() {
                        onUserAction(
                                /* accept= */ false, callback, OnBoarding.OB_NO_ANSWER,
                                DropOutReason.ONBOARDING_BACK_BUTTON_CLICKED);
                        return true;
                    }
                });
        initContent(callback);
        BottomSheetUtils.showContentAndMaybeExpand(
                mController, mContent, /* shouldExpand = */ true, mAnimate);
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
                .setOnClickListener(unusedView
                        -> onUserAction(
                                /* accept= */ true, callback, OnBoarding.OB_ACCEPTED,
                                DropOutReason.DECLINED));
        initView.findViewById(R.id.button_init_not_ok)
                .setOnClickListener(unusedView
                        -> onUserAction(
                                /* accept= */ false, callback, OnBoarding.OB_CANCELLED,
                                DropOutReason.DECLINED));

        updateViewBasedOnIntent(initView);

        mContent.setContent(initView, initView);
    }

    private void onUserAction(boolean accept, Callback<Boolean> callback,
            @OnBoarding int onboardingAnswer, @DropOutReason int dropoutReason) {
        AutofillAssistantPreferencesUtil.setInitialPreferences(accept);
        AutofillAssistantMetrics.recordOnBoarding(onboardingAnswer);
        if (!accept) {
            AutofillAssistantMetrics.recordDropOut(dropoutReason);
        }

        callback.onResult(accept);
        hide();
    }

    private void updateViewBasedOnIntent(ScrollView initView) {
        if (!mParameters.containsKey(INTENT_IDENTFIER)) {
            return;
        }

        TextView titleTextView = initView.findViewById(R.id.onboarding_try_assistant);
        TextView termsTextView = initView.findViewById(R.id.onboarding_subtitle);
        switch (mParameters.get(INTENT_IDENTFIER)) {
            case FLIGHTS_INTENT:
                termsTextView.setText(R.string.autofill_assistant_init_message_short);
                titleTextView.setText(R.string.autofill_assistant_init_message_flights_checkin);
                break;
            case FOOD_ORDERING_INTENT:
                termsTextView.setText(R.string.autofill_assistant_init_message_short);
                titleTextView.setText(R.string.autofill_assistant_init_message_food_ordering);
                break;
            case VOICE_SEARCH_INTENT:
                termsTextView.setText(R.string.autofill_assistant_init_message_short);
                titleTextView.setText(R.string.autofill_assistant_init_message_voice_search);
                break;
            case RENT_CAR_INTENT:
                termsTextView.setText(R.string.autofill_assistant_init_message_short);
                titleTextView.setText(R.string.autofill_assistant_init_message_rent_car);
                break;
            case PASSWORD_CHANGE_INTENT:
                termsTextView.setText(R.string.autofill_assistant_init_message_short);
                titleTextView.setText(R.string.autofill_assistant_init_message_password_change);
                break;
            case SHOPPING_INTENT:
            case SHOPPING_ASSISTED_CHECKOUT_INTENT:
                termsTextView.setText(R.string.autofill_assistant_init_message_short);
                titleTextView.setText(R.string.autofill_assistant_init_message_shopping);
                break;
            case BUY_MOVIE_TICKETS_INTENT:
                if (Arrays.asList(mExperimentIds.split(","))
                                .contains(BUY_MOVIE_TICKETS_EXPERIMENT_ID)) {
                    termsTextView.setText(R.string.autofill_assistant_init_message_short);
                    titleTextView.setText(
                            R.string.autofill_assistant_init_message_buy_movie_tickets);
                }

                break;
        }
    }
}
