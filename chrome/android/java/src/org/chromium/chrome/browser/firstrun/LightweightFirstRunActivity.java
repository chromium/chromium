// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.os.Bundle;
import android.os.Handler;
import android.os.SystemClock;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.back_press.SecondaryActivityBackPressUma.SecondaryActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.enterprise.util.EnterpriseInfo;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.ui.widget.LoadingView;

/** Lightweight FirstRunActivity. It shows ToS dialog only. */
public class LightweightFirstRunActivity extends FirstRunActivityBase
        implements LoadingView.Observer {
    // TODO(crbug.com/40156897) Clean this boolean when releasing this feature, and remove
    // @Nullable from members below.
    private static boolean sSupportSkippingTos = true;

    private @Nullable SkipTosDialogPolicyListener mSkipTosDialogPolicyListener;

    private FirstRunFlowSequencer mFirstRunFlowSequencer;
    private TextView mTosAndPrivacyTextView;
    private Button mOkButton;
    private LoadingView mLoadingView;
    private View mLoadingViewContainer;
    private View mLightweightFreButtons;
    private View mPrivacyDisclaimer;
    private boolean mViewCreated;
    private boolean mNativeInitialized;
    private boolean mTriggerAcceptAfterNativeInit;

    private Handler mHandler;
    private Runnable mExitFreRunnable;

    public static final String EXTRA_ASSOCIATED_APP_NAME =
            "org.chromium.chrome.browser.firstrun.AssociatedAppName";

    public LightweightFirstRunActivity() {
        super();

        if (sSupportSkippingTos) {
            mSkipTosDialogPolicyListener =
                    new SkipTosDialogPolicyListener(
                            getPolicyLoadListener(), EnterpriseInfo.getInstance(), null);
            // We can ignore the result from #onAvailable here, as views are not created at this
            // point.
            mSkipTosDialogPolicyListener.onAvailable((ignored) -> onPolicyLoadListenerAvailable());
        }
    }

    @Override
    public void triggerLayoutInflation() {
        super.triggerLayoutInflation();

        setFinishOnTouchOutside(true);

        mFirstRunFlowSequencer =
                new FirstRunFlowSequencer(
                        getProfileProviderSupplier(), getChildAccountStatusSupplier()) {
                    @Override
                    public void onFlowIsKnown(Bundle freProperties) {
                        if (freProperties == null) {
                            completeFirstRunExperience();
                            return;
                        }

                        boolean isChild =
                                freProperties.getBoolean(
                                        SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT, false);
                        initializeViews(isChild);
                    }
                };
        mFirstRunFlowSequencer.start();
        onInitialLayoutInflationComplete();
    }

    /** Called once it is known whether the device has a child account. */
    private void initializeViews(boolean hasChildAccount) {
        setContentView(
                LayoutInflater.from(LightweightFirstRunActivity.this)
                        .inflate(R.layout.lightweight_fre_tos, null));

        NoUnderlineClickableSpan clickableGoogleTermsSpan =
                new NoUnderlineClickableSpan(
                        this, (view) -> showInfoPage(R.string.google_terms_of_service_url));
        NoUnderlineClickableSpan clickableChromeAdditionalTermsSpan =
                new NoUnderlineClickableSpan(
                        this,
                        (view) -> showInfoPage(R.string.chrome_additional_terms_of_service_url));
        NoUnderlineClickableSpan clickableGooglePrivacySpan =
                new NoUnderlineClickableSpan(
                        this, (view) -> showInfoPage(R.string.google_privacy_policy_url));
        String associatedAppName =
                IntentUtils.safeGetStringExtra(getIntent(), EXTRA_ASSOCIATED_APP_NAME);
        if (associatedAppName == null) {
            associatedAppName = "";
        }
        final CharSequence tosAndPrivacyText;
        if (hasChildAccount) {
            tosAndPrivacyText =
                    SpanApplier.applySpans(
                            getString(
                                    R.string
                                            .lightweight_fre_associated_app_tos_and_privacy_child_account,
                                    associatedAppName),
                            new SpanInfo("<LINK1>", "</LINK1>", clickableGoogleTermsSpan),
                            new SpanInfo("<LINK2>", "</LINK2>", clickableChromeAdditionalTermsSpan),
                            new SpanInfo("<LINK3>", "</LINK3>", clickableGooglePrivacySpan));
        } else {
            tosAndPrivacyText =
                    SpanApplier.applySpans(
                            getString(
                                    R.string.lightweight_fre_associated_app_tos, associatedAppName),
                            new SpanInfo("<LINK1>", "</LINK1>", clickableGoogleTermsSpan),
                            new SpanInfo(
                                    "<LINK2>", "</LINK2>", clickableChromeAdditionalTermsSpan));
        }

        mTosAndPrivacyTextView = findViewById(R.id.lightweight_fre_tos_and_privacy);
        mTosAndPrivacyTextView.setText(tosAndPrivacyText);
        mTosAndPrivacyTextView.setMovementMethod(LinkMovementMethod.getInstance());

        mLightweightFreButtons = findViewById(R.id.lightweight_fre_buttons);
        mOkButton = findViewById(R.id.button_primary);
        mOkButton.setOnClickListener(view -> acceptTermsOfService());

        ((Button) findViewById(R.id.button_secondary))
                .setOnClickListener(view -> abortFirstRunExperience());

        mLoadingView = findViewById(R.id.loading_view);
        mLoadingViewContainer = findViewById(R.id.loading_view_container);

        mPrivacyDisclaimer = findViewById(R.id.privacy_disclaimer);

        mViewCreated = true;

        if (mSkipTosDialogPolicyListener != null) {
            // Check if we need to setup logic for policy loading.
            if (mSkipTosDialogPolicyListener.get() == null) {
                mLoadingView.addObserver(this);
                mLoadingView.showLoadingUI();
                setTosComponentVisibility(false);
            } else if (mSkipTosDialogPolicyListener.get()) {
                setTosComponentVisibility(false);
                skipTosByPolicy();
            }
        }
    }

    private void setTosComponentVisibility(boolean isVisible) {
        int visibility = isVisible ? View.VISIBLE : View.GONE;
        mTosAndPrivacyTextView.setVisibility(visibility);
        mLightweightFreButtons.setVisibility(visibility);
    }

    private void onPolicyLoadListenerAvailable() {
        if (mViewCreated) mLoadingView.hideLoadingUI();
    }

    @Override
    public void onShowLoadingUIComplete() {
        mLoadingViewContainer.setVisibility(View.VISIBLE);
    }

    @Override
    public void onHideLoadingUIComplete() {
        assert mSkipTosDialogPolicyListener != null && mSkipTosDialogPolicyListener.get() != null;
        if (mSkipTosDialogPolicyListener.get()) {
            skipTosByPolicy();
        } else {
            // Else, show the ToS as the loading spinner is GONE.
            boolean hasAccessibilityFocus = mLoadingViewContainer.isAccessibilityFocused();
            mLoadingViewContainer.setVisibility(View.GONE);
            setTosComponentVisibility(true);

            if (hasAccessibilityFocus) {
                mTosAndPrivacyTextView.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
            }
        }
    }

    @Override
    public void finishNativeInitialization() {
        super.finishNativeInitialization();
        assert !mNativeInitialized;

        mNativeInitialized = true;
        RecordHistogram.recordTimesHistogram(
                "MobileFre.NativeInitialized", SystemClock.elapsedRealtime() - getStartTime());
        if (mTriggerAcceptAfterNativeInit) acceptTermsOfService();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        mLoadingView.destroy();

        if (mSkipTosDialogPolicyListener != null) mSkipTosDialogPolicyListener.destroy();

        if (mHandler != null && mExitFreRunnable != null) {
            mHandler.removeCallbacks(mExitFreRunnable);
        }
    }

    @Override
    public @BackPressResult int handleBackPress() {
        abortFirstRunExperience();
        return BackPressResult.SUCCESS;
    }

    @Override
    public int getSecondaryActivity() {
        return SecondaryActivity.LIGHTWEIGHT_FIRST_RUN;
    }

    private void abortFirstRunExperience() {
        finish();
        notifyCustomTabCallbackFirstRunIfNecessary(getIntent(), false);
    }

    public void completeFirstRunExperience() {
        FirstRunStatus.setLightweightFirstRunFlowComplete(true);
        exitLightweightFirstRun();
    }

    private void skipTosByPolicy() {
        mLoadingViewContainer.setVisibility(View.GONE);
        mPrivacyDisclaimer.setVisibility(View.VISIBLE);
        mPrivacyDisclaimer.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);

        mExitFreRunnable =
                () -> {
                    FirstRunStatus.setFirstRunSkippedByPolicy(true);
                    exitLightweightFirstRun();
                    mExitFreRunnable = null;
                };
        mHandler = new Handler(ThreadUtils.getUiThreadLooper());
        mHandler.postDelayed(mExitFreRunnable, FirstRunUtils.getSkipTosExitDelayMs());
    }

    private void exitLightweightFirstRun() {
        finish();
        sendFirstRunCompletePendingIntent();
    }

    private void acceptTermsOfService() {
        if (!mNativeInitialized) {
            mTriggerAcceptAfterNativeInit = true;

            // Disable the "accept" button to indicate that "something is happening".
            mOkButton.setEnabled(false);
            return;
        }
        FirstRunUtils.acceptTermsOfService(false);
        completeFirstRunExperience();
    }

    /**
     * Show an informational web page. The page doesn't show navigation control.
     * @param url Resource id for the URL of the web page.
     */
    public void showInfoPage(@StringRes int url) {
        CustomTabActivity.showInfoPage(
                this, LocalizationUtils.substituteLocalePlaceholder(getString(url)));
    }

    @VisibleForTesting
    public static void setSupportSkippingTos(boolean isSupported) {
        sSupportSkippingTos = isSupported;
    }
}
