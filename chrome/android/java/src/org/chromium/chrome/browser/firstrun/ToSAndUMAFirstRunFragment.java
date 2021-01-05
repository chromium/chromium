// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.content.res.Resources;
import android.os.Bundle;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.version.ChromeVersionInfo;
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

    private static boolean sShowUmaCheckBoxForTesting;

    protected boolean mNativeInitialized;

    private Button mAcceptButton;
    private CheckBox mSendReportCheckBox;
    private TextView mTosAndPrivacy;
    private View mTitle;
    private View mProgressSpinner;
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

        mSendReportCheckBox.setChecked(FirstRunActivity.DEFAULT_METRICS_AND_CRASH_REPORTING);
        if (!canShowUmaCheckBox()) {
            mSendReportCheckBox.setVisibility(View.GONE);
        }

        mTosAndPrivacy.setMovementMethod(LinkMovementMethod.getInstance());

        Resources resources = getResources();
        NoUnderlineClickableSpan clickableGoogleTermsSpan =
                new NoUnderlineClickableSpan(resources, (view1) -> {
                    if (!isAdded()) return;
                    getPageDelegate().showInfoPage(R.string.google_terms_of_service_url);
                });
        NoUnderlineClickableSpan clickableChromeAdditionalTermsSpan =
                new NoUnderlineClickableSpan(resources, (view1) -> {
                    if (!isAdded()) return;
                    getPageDelegate().showInfoPage(R.string.chrome_additional_terms_of_service_url);
                });
        NoUnderlineClickableSpan clickableFamilyLinkPrivacySpan =
                new NoUnderlineClickableSpan(resources, (view1) -> {
                    if (!isAdded()) return;
                    getPageDelegate().showInfoPage(R.string.family_link_privacy_policy_url);
                });

        final CharSequence tosText;
        Bundle freProperties = getPageDelegate().getProperties();
        @ChildAccountStatus.Status
        int childAccountStatus = freProperties.getInt(
                SigninFirstRunFragment.CHILD_ACCOUNT_STATUS, ChildAccountStatus.NOT_CHILD);
        if (childAccountStatus == ChildAccountStatus.REGULAR_CHILD) {
            tosText = SpanApplier.applySpans(getString(R.string.fre_tos_and_privacy_child_account),
                    new SpanInfo("<LINK1>", "</LINK1>", clickableGoogleTermsSpan),
                    new SpanInfo("<LINK2>", "</LINK2>", clickableChromeAdditionalTermsSpan),
                    new SpanInfo("<LINK3>", "</LINK3>", clickableFamilyLinkPrivacySpan));
        } else {
            tosText = SpanApplier.applySpans(getString(R.string.fre_tos),
                    new SpanInfo("<LINK1>", "</LINK1>", clickableGoogleTermsSpan),
                    new SpanInfo("<LINK2>", "</LINK2>", clickableChromeAdditionalTermsSpan));
        }
        mTosAndPrivacy.setText(tosText);

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
    public void setInitialA11yFocus() {
        // Ignore calls before view is created.
        if (mTitle == null) return;
        mTitle.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
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
        boolean allowCrashUpload = canShowUmaCheckBox() && mSendReportCheckBox.isChecked();
        getPageDelegate().acceptTermsOfService(allowCrashUpload);
    }

    private void setSpinnerVisible(boolean spinnerVisible) {
        // When the progress spinner is visible, we hide the other UI elements so that
        // the user can't interact with them.
        boolean otherElementVisible = !spinnerVisible;

        setTosAndUmaVisible(otherElementVisible);
        mTitle.setVisibility(otherElementVisible ? View.VISIBLE : View.INVISIBLE);
        mProgressSpinner.setVisibility(spinnerVisible ? View.VISIBLE : View.GONE);
    }

    // Exposed methods for ToSAndUMACCTFirstRunFragment

    protected void setTosAndUmaVisible(boolean isVisible) {
        int visibility = isVisible ? View.VISIBLE : View.GONE;

        mAcceptButton.setVisibility(visibility);
        mTosAndPrivacy.setVisibility(visibility);
        // Avoid updating visibility if the UMA check box can't be shown right now.
        if (canShowUmaCheckBox()) {
            mSendReportCheckBox.setVisibility(visibility);
        }
    }

    protected View getToSAndPrivacyText() {
        return mTosAndPrivacy;
    }

    /**
     * @return Whether the check box for Uma metrics can be shown. It should be used in conjunction
     *         with whether other non-spinner elements can generally be shown.
     */
    protected boolean canShowUmaCheckBox() {
        return sShowUmaCheckBoxForTesting || ChromeVersionInfo.isOfficialBuild();
    }

    @VisibleForTesting
    public static void setShowUmaCheckBoxForTesting(boolean showForTesting) {
        sShowUmaCheckBoxForTesting = showForTesting;
    }
}
