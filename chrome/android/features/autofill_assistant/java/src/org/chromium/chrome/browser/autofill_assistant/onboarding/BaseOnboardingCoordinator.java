// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.onboarding;

import android.content.Context;
import android.text.SpannableString;
import android.text.TextUtils;
import android.text.method.LinkMovementMethod;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.LocaleUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantMetrics;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantPreferencesUtil;
import org.chromium.chrome.browser.autofill_assistant.metrics.OnBoarding;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayCoordinator;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

/**
 * Base Coordinator class responsible for showing the onboarding screen when the user is using the
 * Autofill Assistant for the first time.
 */
@JNINamespace("autofill_assistant")
public abstract class BaseOnboardingCoordinator implements OnboardingView {
    public static final String INTENT_IDENTFIER = "INTENT";
    private static final String FETCH_TIMEOUT_IDENTIFIER = "ONBOARDING_FETCH_TIMEOUT_MS";
    private static final String BUY_MOVIE_TICKETS_INTENT = "BUY_MOVIE_TICKET";
    private static final String RENT_CAR_INTENT = "RENT_CAR";
    private static final String FLIGHTS_INTENT = "FLIGHTS_CHECKIN";
    private static final String PASSWORD_CHANGE_INTENT = "PASSWORD_CHANGE";
    private static final String FOOD_ORDERING_INTENT = "FOOD_ORDERING";
    private static final String FOOD_ORDERING_PICKUP_INTENT = "FOOD_ORDERING_PICKUP";
    private static final String FOOD_ORDERING_DELIVERY_INTENT = "FOOD_ORDERING_DELIVERY";
    private static final String VOICE_SEARCH_INTENT = "TELEPORT";
    private static final String SHOPPING_INTENT = "SHOPPING";
    private static final String SHOPPING_ASSISTED_CHECKOUT_INTENT = "SHOPPING_ASSISTED_CHECKOUT";
    private static final String BUY_MOVIE_TICKETS_EXPERIMENT_ID = "4363482";

    private final String mExperimentIds;
    private final Map<String, String> mParameters;
    private final Map<String, String> mStringMap = new HashMap<>();

    @Nullable
    private WebContentsObserver mWebContentsObserver;
    private boolean mOnboardingShown;

    final Context mContext;
    boolean mAnimate = true;
    @Nullable
    ScrollView mView;

    public BaseOnboardingCoordinator(
            String experimentIds, Map<String, String> parameters, Context context) {
        mExperimentIds = experimentIds;
        mParameters = parameters;
        mContext = context;

        mView = createViewImpl();
    }

    /**
     * Shows onboarding and provides the result to the given callback.
     *
     * <p>The {@code callback} will be called when the user accepts, cancels or dismisses the
     * onboarding.
     *
     * <p>Note that the onboarding screen will be hidden after the callback returns. Call, from the
     * callback, {@link #hide} to hide it earlier or {@link #transferControls} to take ownership of
     * it and possibly keep it past the end of the callback.
     *
     * <p>The {@code targetUrl} is the initial URL Autofill Assistant is being started on. The
     * navigation to that URL is allowed, other navigations will hide Autofill Assistant.
     */
    @Override
    public void show(Callback<Integer> callback, WebContents webContents, String targetUrl) {
        addWebContentObserver(callback, webContents, targetUrl);
        show(callback);
    }

    /**
     * Same as {@link #show(Callback, WebContents, String)}, but does not break on navigation
     * events.
     */
    public void show(Callback<Integer> callback) {
        AutofillAssistantMetrics.recordOnBoarding(
                OnBoarding.OB_SHOWN, mParameters.get(INTENT_IDENTFIER));
        mOnboardingShown = true;

        initViewImpl(callback);
        setupSharedView(callback);

        int fetchTimeoutMs = 300;
        if (mParameters.containsKey(FETCH_TIMEOUT_IDENTIFIER)) {
            fetchTimeoutMs = Integer.parseInt(mParameters.get(FETCH_TIMEOUT_IDENTIFIER));
        }
        if (!mParameters.containsKey(INTENT_IDENTFIER) || fetchTimeoutMs == 0) {
            updateAndShowView();
        } else {
            BaseOnboardingCoordinatorJni.get().fetchOnboardingDefinition(this,
                    mParameters.get(INTENT_IDENTFIER), LocaleUtils.getDefaultLocaleString(),
                    fetchTimeoutMs);
        }
    }

    /**
     * Returns {@code true} if the onboarding has been shown at the beginning when this
     * autofill assistant flow got triggered.
     */
    @Override
    public boolean getOnboardingShown() {
        return mOnboardingShown;
    }

    /**
     * Transfers the overlay coordinator used by the onboarding to the caller. This is intended to
     * be used to facilitate a smooth transition between onboarding and regular script, i.e., to
     * avoid flickering of the overlay. Not all onboarding implementations show an overlay, so this
     * may return null.
     */
    @Nullable
    public AssistantOverlayCoordinator transferControls() {
        return null;
    }

    /** Destroy web contents observer. */
    void destroy() {
        if (mWebContentsObserver != null) {
            mWebContentsObserver.destroy();
            mWebContentsObserver = null;
        }
    }

    private void addWebContentObserver(
            Callback<Integer> callback, WebContents webContents, String targetUrl) {
        mWebContentsObserver = new WebContentsObserver(webContents) {
            @Override
            public void didStartNavigation(NavigationHandle navigationHandle) {
                if (navigationHandle.getUrl().getSpec().equals(targetUrl)) {
                    return;
                }

                if (navigationHandle.isInMainFrame() && !navigationHandle.isRendererInitiated()
                        && !navigationHandle.isSameDocument()) {
                    onUserAction(/* result= */ AssistantOnboardingResult.NAVIGATION, callback);
                }
            }
        };
        webContents.addObserver(mWebContentsObserver);
    }

    /**
     * Setup the shared |mView|
     */
    private void setupSharedView(Callback<Integer> callback) {
        // Set focusable for accessibility.
        mView.setFocusable(true);
        mView.findViewById(R.id.button_init_ok)
                .setOnClickListener(unusedView
                        -> onUserAction(
                                /* result= */ AssistantOnboardingResult.ACCEPTED, callback));
        mView.findViewById(R.id.button_init_not_ok)
                .setOnClickListener(unusedView
                        -> onUserAction(
                                /* result= */ AssistantOnboardingResult.REJECTED, callback));
    }

    void onUserAction(@AssistantOnboardingResult Integer result, Callback<Integer> callback) {
        switch (result) {
            case AssistantOnboardingResult.DISMISSED:
                break;
            case AssistantOnboardingResult.REJECTED:
                AutofillAssistantPreferencesUtil.setInitialPreferences(false);
                break;
            case AssistantOnboardingResult.ACCEPTED:
                AutofillAssistantPreferencesUtil.setInitialPreferences(true);
                break;
        }
        callback.onResult(result);
        hide();
    }

    Context getContext() {
        return mContext;
    }

    @CalledByNative
    @VisibleForTesting
    public void addEntryToStringMap(String key, String value) {
        mStringMap.put(key, value);
    }

    @CalledByNative
    @VisibleForTesting
    public void updateAndShowView() {
        updateView();
        showViewImpl();
    }

    private void updateView() {
        assert mView != null;

        String termsAndConditionsKey = "terms_and_conditions";
        String termsAndConditionsUrlKey = "terms_and_conditions_url";
        updateTermsAndConditions(mView, mStringMap.get(termsAndConditionsKey),
                mStringMap.get(termsAndConditionsUrlKey));

        if (mStringMap.isEmpty()) {
            updateViewBasedOnIntent(mView);
        } else {
            String onboardingTitleKey = "onboarding_title";
            if (mStringMap.containsKey(onboardingTitleKey)) {
                ((TextView) mView.findViewById(R.id.onboarding_try_assistant))
                        .setText(mStringMap.get(onboardingTitleKey));
            }

            String onboardingTextKey = "onboarding_text";
            if (mStringMap.containsKey(onboardingTextKey)) {
                ((TextView) mView.findViewById(R.id.onboarding_subtitle))
                        .setText(mStringMap.get(onboardingTextKey));
            }
        }
    }

    private void updateTermsAndConditions(ScrollView initView,
            @Nullable String termsAndConditionsString, @Nullable String termsAndConditionsUrl) {
        TextView termsTextView = initView.findViewById(R.id.google_terms_message);

        // Note: `SpanApplier.applySpans` will throw an error if the text does not contain
        // <link></link> to replace!
        if (TextUtils.isEmpty(termsAndConditionsString)
                || !termsAndConditionsString.contains("<link>")
                || !termsAndConditionsString.contains("</link>")) {
            termsAndConditionsString = mContext.getApplicationContext().getString(
                    R.string.autofill_assistant_google_terms_description);
        }

        NoUnderlineClickableSpan termsSpan = new NoUnderlineClickableSpan(mContext.getResources(),
                (widget)
                        -> CustomTabActivity.showInfoPage(mContext.getApplicationContext(),
                                TextUtils.isEmpty(termsAndConditionsUrl)
                                                || !UrlUtilitiesJni.get().isGoogleSubDomainUrl(
                                                        termsAndConditionsUrl)
                                        ? mContext.getApplicationContext().getString(
                                                R.string.autofill_assistant_google_terms_url)
                                        : termsAndConditionsUrl));
        SpannableString spannableMessage = SpanApplier.applySpans(
                termsAndConditionsString, new SpanApplier.SpanInfo("<link>", "</link>", termsSpan));
        termsTextView.setText(spannableMessage);
        termsTextView.setMovementMethod(LinkMovementMethod.getInstance());
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
            case FOOD_ORDERING_PICKUP_INTENT:
            case FOOD_ORDERING_DELIVERY_INTENT:
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

    /** Don't animate the user interface. */
    @VisibleForTesting
    public void disableAnimationForTesting() {
        mAnimate = false;
    }

    abstract ScrollView createViewImpl();
    abstract void initViewImpl(Callback<Integer> callback);
    abstract void showViewImpl();

    /**
     * Returns {@code true} between the time {@link #show} is called and the time
     * the callback has returned.
     */
    @VisibleForTesting
    public abstract boolean isInProgress();

    @NativeMethods
    interface Natives {
        void fetchOnboardingDefinition(
                BaseOnboardingCoordinator coordinator, String intent, String locale, int timeoutMs);
    }
}