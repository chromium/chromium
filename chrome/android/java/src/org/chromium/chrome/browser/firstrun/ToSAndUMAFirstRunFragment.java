// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.content.res.Resources;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.view.ViewCompat;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.components.signin.ChildAccountStatus;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/**
 * The First Run Experience fragment that allows the user to accept Terms of Service ("ToS") and
 * Privacy Notice, and to opt-in to the usage statistics and crash reports collection ("UMA",
 * User Metrics Analysis) as defined in the Chrome Privacy Notice.
 */
public class ToSAndUMAFirstRunFragment extends Fragment implements FirstRunFragment {
    /** FRE page that instantiates this fragment. */
    public static class Page implements FirstRunPage<ToSAndUMAFirstRunFragment> {
        @Override
        public boolean shouldSkipPageOnCreate() {
            return FirstRunStatus.shouldSkipWelcomePage();
        }

        @Override
        public ToSAndUMAFirstRunFragment instantiateFragment() {
            return new ToSAndUMAFirstRunFragment();
        }
    }

    private Button mAcceptButton;
    private CheckBox mSendReportCheckBox;
    private TextView mTosAndPrivacy;
    private View mTitle;
    private View mProgressSpinner;
    private boolean mNativeInitialized;
    private boolean mTriggerAcceptAfterNativeInit;

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fre_tosanduma, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        mTitle = view.findViewById(R.id.title);
        mProgressSpinner = view.findViewById(R.id.progress_spinner);
        mProgressSpinner.setVisibility(View.GONE);
        mAcceptButton = (Button) view.findViewById(R.id.terms_accept);
        mSendReportCheckBox = (CheckBox) view.findViewById(R.id.send_report_checkbox);
        mTosAndPrivacy = (TextView) view.findViewById(R.id.tos_and_privacy);

        mAcceptButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                acceptTermsOfService();
            }
        });

        if (ChromeVersionInfo.isOfficialBuild()) {
            int paddingStart = getResources().getDimensionPixelSize(
                    R.dimen.fre_tos_checkbox_padding);
            ViewCompat.setPaddingRelative(mSendReportCheckBox,
                    ViewCompat.getPaddingStart(mSendReportCheckBox) + paddingStart,
                    mSendReportCheckBox.getPaddingTop(),
                    ViewCompat.getPaddingEnd(mSendReportCheckBox),
                    mSendReportCheckBox.getPaddingBottom());

            mSendReportCheckBox.setChecked(FirstRunActivity.DEFAULT_METRICS_AND_CRASH_REPORTING);
        } else {
            mSendReportCheckBox.setVisibility(View.GONE);
        }

        mTosAndPrivacy.setMovementMethod(LinkMovementMethod.getInstance());

        Resources resources = getResources();
        NoUnderlineClickableSpan clickableTermsSpan =
                new NoUnderlineClickableSpan(resources, (view1) -> {
                    if (!isAdded()) return;
                    getPageDelegate().showInfoPage(R.string.chrome_terms_of_service_url);
                });

        NoUnderlineClickableSpan clickablePrivacySpan =
                new NoUnderlineClickableSpan(resources, (view1) -> {
                    if (!isAdded()) return;
                    getPageDelegate().showInfoPage(R.string.chrome_privacy_notice_url);
                });

        NoUnderlineClickableSpan clickableFamilyLinkPrivacySpan =
                new NoUnderlineClickableSpan(resources, (view1) -> {
                    if (!isAdded()) return;
                    getPageDelegate().showInfoPage(R.string.family_link_privacy_policy_url);
                });

        final CharSequence tosAndPrivacyText;
        Bundle freProperties = getPageDelegate().getProperties();
        @ChildAccountStatus.Status
        int childAccountStatus = freProperties.getInt(
                SigninFirstRunFragment.CHILD_ACCOUNT_STATUS, ChildAccountStatus.NOT_CHILD);
        if (childAccountStatus == ChildAccountStatus.REGULAR_CHILD) {
            tosAndPrivacyText =
                    SpanApplier.applySpans(getString(R.string.fre_tos_and_privacy_child_account),
                            new SpanInfo("<LINK1>", "</LINK1>", clickableTermsSpan),
                            new SpanInfo("<LINK2>", "</LINK2>", clickablePrivacySpan),
                            new SpanInfo("<LINK3>", "</LINK3>", clickableFamilyLinkPrivacySpan));
        } else {
            tosAndPrivacyText = SpanApplier.applySpans(getString(R.string.fre_tos_and_privacy),
                    new SpanInfo("<LINK1>", "</LINK1>", clickableTermsSpan),
                    new SpanInfo("<LINK2>", "</LINK2>", clickablePrivacySpan));
        }
        mTosAndPrivacy.setText(tosAndPrivacyText);

        // If this page should be skipped, it can be one of the following cases:
        //   1. Native hasn't been initialized yet and this page will be skipped once that happens.
        //   2. The user has moved back to this page after advancing past it. In this case, this
        //      may not even be the same object as before, as the fragment may have been re-created.
        //
        // In case 1, hide all the elements except for Chrome logo and the spinner until native gets
        // initialized at which point the activity will skip the page.
        // We distinguish case 1 from case 2 by the value of |mNativeInitialized|, as that is set
        // via onAttachFragment() from FirstRunActivity - which is before this onViewCreated().
        if (!mNativeInitialized && FirstRunStatus.shouldSkipWelcomePage()) {
            setSpinnerVisible(true);
        }
    }

    @Override
    public void setUserVisibleHint(boolean isVisibleToUser) {
        super.setUserVisibleHint(isVisibleToUser);

        // This may be called before onViewCreated(), in which case the below is not yet relevant.
        if (mTitle == null) return;

        if (!isVisibleToUser) {
            // Restore original enabled & visibility states, in case the user returns to the page.
            setSpinnerVisible(false);
        } else {
            // On certain versions of Android, the checkbox will appear unchecked upon revisiting
            // the page.  Force it to the end state of the drawable animation as a work around.
            // crbug.com/666258
            mSendReportCheckBox.jumpDrawablesToCurrentState();
        }
    }

    @Override
    public void onNativeInitialized() {
        assert !mNativeInitialized;

        mNativeInitialized = true;
        if (mTriggerAcceptAfterNativeInit) acceptTermsOfService();
    }

    private void acceptTermsOfService() {
        if (!mNativeInitialized) {
            mTriggerAcceptAfterNativeInit = true;
            setSpinnerVisible(true);
            return;
        }

        mTriggerAcceptAfterNativeInit = false;
        getPageDelegate().acceptTermsOfService(mSendReportCheckBox.isChecked());
    }

    private void setSpinnerVisible(boolean spinnerVisible) {
        // When the progress spinner is visible, we hide the other UI elements so that
        // the user can't interact with them.
        int otherElementsVisible = spinnerVisible ? View.INVISIBLE : View.VISIBLE;
        mTitle.setVisibility(otherElementsVisible);
        mAcceptButton.setVisibility(otherElementsVisible);
        mTosAndPrivacy.setVisibility(otherElementsVisible);
        mSendReportCheckBox.setVisibility(otherElementsVisible);
        mProgressSpinner.setVisibility(spinnerVisible ? View.VISIBLE : View.GONE);
    }
}
