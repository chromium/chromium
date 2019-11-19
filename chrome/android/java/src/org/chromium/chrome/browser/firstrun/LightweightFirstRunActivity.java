// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.content.res.Resources;
import android.os.Bundle;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.StringRes;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.components.signin.ChildAccountStatus;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/**
* Lightweight FirstRunActivity. It shows ToS dialog only.
*/
public class LightweightFirstRunActivity extends FirstRunActivityBase {
    private FirstRunFlowSequencer mFirstRunFlowSequencer;
    private Button mOkButton;
    private boolean mNativeInitialized;
    private boolean mTriggerAcceptAfterNativeInit;

    public static final String EXTRA_ASSOCIATED_APP_NAME =
            "org.chromium.chrome.browser.firstrun.AssociatedAppName";

    @Override
    public void triggerLayoutInflation() {
        setFinishOnTouchOutside(true);

        mFirstRunFlowSequencer = new FirstRunFlowSequencer(this) {
            @Override
            public void onFlowIsKnown(Bundle freProperties) {
                if (freProperties == null) {
                    completeFirstRunExperience();
                    return;
                }

                @ChildAccountStatus.Status
                int childAccountStatus = freProperties.getInt(
                        SigninFirstRunFragment.CHILD_ACCOUNT_STATUS, ChildAccountStatus.NOT_CHILD);
                onChildAccountKnown(ChildAccountStatus.isChild(childAccountStatus));
            }
        };
        mFirstRunFlowSequencer.start();
        onInitialLayoutInflationComplete();
    }

    /** Called once it is known whether the device has a child account. */
    public void onChildAccountKnown(boolean hasChildAccount) {
        setContentView(LayoutInflater.from(LightweightFirstRunActivity.this)
                               .inflate(R.layout.lightweight_fre_tos, null));

        final Resources resources = getResources();
        NoUnderlineClickableSpan clickableTermsSpan = new NoUnderlineClickableSpan(
                resources, (view) -> showInfoPage(R.string.chrome_terms_of_service_url));
        NoUnderlineClickableSpan clickablePrivacySpan = new NoUnderlineClickableSpan(
                resources, (view) -> showInfoPage(R.string.chrome_privacy_notice_url));
        NoUnderlineClickableSpan clickableFamilyLinkPrivacySpan = new NoUnderlineClickableSpan(
                resources, (view) -> showInfoPage(R.string.family_link_privacy_policy_url));
        String associatedAppName =
                IntentUtils.safeGetStringExtra(getIntent(), EXTRA_ASSOCIATED_APP_NAME);
        if (associatedAppName == null) {
            associatedAppName = "";
        }
        final CharSequence tosAndPrivacyText;
        if (hasChildAccount) {
            tosAndPrivacyText = SpanApplier.applySpans(
                    getString(R.string.lightweight_fre_associated_app_tos_and_privacy_child_account,
                            associatedAppName),
                    new SpanInfo("<LINK1>", "</LINK1>", clickableTermsSpan),
                    new SpanInfo("<LINK2>", "</LINK2>", clickablePrivacySpan),
                    new SpanInfo("<LINK3>", "</LINK3>", clickableFamilyLinkPrivacySpan));
        } else {
            tosAndPrivacyText = SpanApplier.applySpans(
                    getString(R.string.lightweight_fre_associated_app_tos_and_privacy,
                            associatedAppName),
                    new SpanInfo("<LINK1>", "</LINK1>", clickableTermsSpan),
                    new SpanInfo("<LINK2>", "</LINK2>", clickablePrivacySpan));
        }
        TextView tosAndPrivacyTextView =
                (TextView) findViewById(R.id.lightweight_fre_tos_and_privacy);
        tosAndPrivacyTextView.setText(tosAndPrivacyText);
        tosAndPrivacyTextView.setMovementMethod(LinkMovementMethod.getInstance());

        mOkButton = (Button) findViewById(R.id.button_primary);
        int okButtonHorizontalPadding =
                getResources().getDimensionPixelSize(R.dimen.fre_button_padding);
        mOkButton.setPaddingRelative(okButtonHorizontalPadding, mOkButton.getPaddingTop(),
                okButtonHorizontalPadding, mOkButton.getPaddingBottom());
        mOkButton.setOnClickListener(view -> acceptTermsOfService());

        ((Button) findViewById(R.id.button_secondary))
                .setOnClickListener(view -> abortFirstRunExperience());
    }

    @Override
    public void finishNativeInitialization() {
        super.finishNativeInitialization();
        assert !mNativeInitialized;

        mNativeInitialized = true;
        if (mTriggerAcceptAfterNativeInit) acceptTermsOfService();
    }

    @Override
    public void onBackPressed() {
        abortFirstRunExperience();
    }

    public void abortFirstRunExperience() {
        finish();
        notifyCustomTabCallbackFirstRunIfNecessary(getIntent(), false);
    }

    public void completeFirstRunExperience() {
        FirstRunStatus.setLightweightFirstRunFlowComplete(true);
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
}
