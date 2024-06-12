// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.text.SpannableString;
import android.view.View;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeStringConstants;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.components.autofill.AutofillFeatures;
import org.chromium.components.autofill.VirtualCardEnrollmentLinkType;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link AutofillVirtualCardEnrollmentDialog}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    AutofillFeatures.AUTOFILL_ENABLE_NEW_CARD_ART_AND_NETWORK_IMAGES,
    AutofillFeatures.AUTOFILL_ENABLE_CARD_ART_IMAGE
})
public class AutofillVirtualCardEnrollmentDialogTest {
    private static final String LEGAL_MESSAGE_URL = "http://www.google.com";
    private static final String ACCEPT_BUTTON_TEXT = "Yes";
    private static final String DECLINE_BUTTON_TEXT = "No thanks";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Callback<Integer> mResultHandlerMock;
    @Mock private AutofillVirtualCardEnrollmentDialog.LinkClickCallback mOnLinkClickedMock;
    @Mock private PersonalDataManager mPersonalDataManager;
    private FakeModalDialogManager mModalDialogManager;
    private AutofillVirtualCardEnrollmentDialog mDialog;
    private VirtualCardEnrollmentFields mVirtualCardEnrollmentFields;

    @Before
    public void setUp() {
        mModalDialogManager = new FakeModalDialogManager(ModalDialogType.APP);
        mVirtualCardEnrollmentFields =
                VirtualCardEnrollmentFields.create("Visa", "1234", 0, new GURL(""));
        mVirtualCardEnrollmentFields.mGoogleLegalMessages.add(createLegalMessageLine("google"));
        mVirtualCardEnrollmentFields.mIssuerLegalMessages.add(createLegalMessageLine("issuer"));
        mDialog =
                new AutofillVirtualCardEnrollmentDialog(
                        ApplicationProvider.getApplicationContext(),
                        mModalDialogManager,
                        mPersonalDataManager,
                        mVirtualCardEnrollmentFields,
                        ACCEPT_BUTTON_TEXT,
                        DECLINE_BUTTON_TEXT,
                        mOnLinkClickedMock,
                        mResultHandlerMock);
        mDialog.show();
    }

    @Test
    @SmallTest
    public void dialogShown() {
        assertThat(mModalDialogManager.getShownDialogModel()).isNotNull();
        // The callback should not have been called yet.
        verify(mResultHandlerMock, never()).onResult(any());
    }

    @Test
    @SmallTest
    public void positiveButtonPressed() {
        assertThat(mModalDialogManager.getShownDialogModel()).isNotNull();
        mModalDialogManager.clickPositiveButton();
        assertThat(mModalDialogManager.getShownDialogModel()).isNull();
        // Check that callback was called with positive button clicked as dismissal cause.
        verify(mResultHandlerMock).onResult(DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }

    @Test
    @SmallTest
    public void negativeButtonPressed() {
        assertThat(mModalDialogManager.getShownDialogModel()).isNotNull();
        mModalDialogManager.clickNegativeButton();
        assertThat(mModalDialogManager.getShownDialogModel()).isNull();
        // Check that callback was called with negative button clicked as dismissal cause.
        verify(mResultHandlerMock).onResult(DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }

    @Test
    @SmallTest
    public void learnMoreTextClicked() {
        // Create activity.
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        // Create a new AutofillVirtualCardEnrollmentDialog with Activity as the context instead.
        mDialog =
                new AutofillVirtualCardEnrollmentDialog(
                        activity,
                        mModalDialogManager,
                        mPersonalDataManager,
                        mVirtualCardEnrollmentFields,
                        ACCEPT_BUTTON_TEXT,
                        DECLINE_BUTTON_TEXT,
                        mOnLinkClickedMock,
                        mResultHandlerMock);
        mDialog.show();
        // Make sure that the dialog was shown properly.
        assertThat(mModalDialogManager.getShownDialogModel()).isNotNull();
        // Get the clickable span.
        SpannableString virtualCardEducationText =
                getSpannableStringForViewFromCurrentDialog(R.id.virtual_card_education);
        // Assert that the message is not empty.
        assertThat(virtualCardEducationText.length()).isGreaterThan(0);

        // Assert that the text of this span is correct.
        NoUnderlineClickableSpan learnMoreSpan =
                getOnlyClickableSpanFromString(virtualCardEducationText);
        assertThat(getHighlightedTextFromSpannableString(virtualCardEducationText, learnMoreSpan))
                .isEqualTo("Learn more about virtual cards");
        // Click on the link. The callback doesn't use the view so it can be null.
        learnMoreSpan.onClick(null);
        // Verify that the callback is called with url for learn more page and enum type
        // corresponding to the learn more link.
        verify(mOnLinkClickedMock)
                .call(
                        ChromeStringConstants.AUTOFILL_VIRTUAL_CARD_ENROLLMENT_SUPPORT_URL,
                        VirtualCardEnrollmentLinkType.VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK);
    }

    @Test
    @SmallTest
    public void googleLegalMessageClicked() {
        // Create activity.
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        // Create a new AutofillVirtualCardEnrollmentDialog with Activity as the context instead.
        mDialog =
                new AutofillVirtualCardEnrollmentDialog(
                        activity,
                        mModalDialogManager,
                        mPersonalDataManager,
                        mVirtualCardEnrollmentFields,
                        ACCEPT_BUTTON_TEXT,
                        DECLINE_BUTTON_TEXT,
                        mOnLinkClickedMock,
                        mResultHandlerMock);
        mDialog.show();
        // Make sure that the dialog was shown properly.
        assertThat(mModalDialogManager.getShownDialogModel()).isNotNull();
        // Get the clickable span.
        SpannableString googleLegalMessageText =
                getSpannableStringForViewFromCurrentDialog(R.id.google_legal_message);
        // Assert that the message is not empty.
        assertThat(googleLegalMessageText.length()).isGreaterThan(0);

        // Assert that the text of this span is correct.
        NoUnderlineClickableSpan googleSpan =
                getOnlyClickableSpanFromString(googleLegalMessageText);
        assertThat(getHighlightedTextFromSpannableString(googleLegalMessageText, googleSpan))
                .isEqualTo("oo");
        // Click on the link. The callback doesn't use the view so it can be null.
        googleSpan.onClick(null);
        // Verify that the callback is called with LEGAL_MESSAGE_URL and enum type corresponding to
        // Google legal message lines.
        verify(mOnLinkClickedMock)
                .call(
                        LEGAL_MESSAGE_URL,
                        VirtualCardEnrollmentLinkType
                                .VIRTUAL_CARD_ENROLLMENT_GOOGLE_PAYMENTS_TOS_LINK);
    }

    @Test
    @SmallTest
    public void issuerLegalMessageClicked() {
        // Create activity.
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        // Create a new AutofillVirtualCardEnrollmentDialog with Activity as the context instead.
        mDialog =
                new AutofillVirtualCardEnrollmentDialog(
                        activity,
                        mModalDialogManager,
                        mPersonalDataManager,
                        mVirtualCardEnrollmentFields,
                        ACCEPT_BUTTON_TEXT,
                        DECLINE_BUTTON_TEXT,
                        mOnLinkClickedMock,
                        mResultHandlerMock);
        mDialog.show();
        // Make sure that the dialog was shown properly.
        assertThat(mModalDialogManager.getShownDialogModel()).isNotNull();
        // Get the clickable span.
        SpannableString issuerLegalMessageText =
                getSpannableStringForViewFromCurrentDialog(R.id.issuer_legal_message);
        // Assert that the message is not empty.
        assertThat(issuerLegalMessageText.length()).isGreaterThan(0);

        // Assert that the text of this span is correct.
        NoUnderlineClickableSpan issuerSpan =
                getOnlyClickableSpanFromString(issuerLegalMessageText);
        assertThat(getHighlightedTextFromSpannableString(issuerLegalMessageText, issuerSpan))
                .isEqualTo("ss");
        // Click on the link. The callback doesn't use the view so it can be null.
        issuerSpan.onClick(null);
        // Verify that the callback is called with LEGAL_MESSAGE_URL and enum type corresponding to
        // issuer legal message lines.
        verify(mOnLinkClickedMock)
                .call(
                        LEGAL_MESSAGE_URL,
                        VirtualCardEnrollmentLinkType.VIRTUAL_CARD_ENROLLMENT_ISSUER_TOS_LINK);
    }

    private SpannableString getSpannableStringForViewFromCurrentDialog(int textViewId) {
        View customView =
                mModalDialogManager.getShownDialogModel().get(ModalDialogProperties.CUSTOM_VIEW);
        return (SpannableString) ((TextView) customView.findViewById(textViewId)).getText();
    }

    private NoUnderlineClickableSpan getOnlyClickableSpanFromString(SpannableString string) {
        NoUnderlineClickableSpan[] spans =
                string.getSpans(0, string.length(), NoUnderlineClickableSpan.class);
        // Assert that there is only one NoUnderlineClickableSpan.
        assertThat(spans.length).isEqualTo(1);
        return spans[0];
    }

    private String getHighlightedTextFromSpannableString(
            SpannableString spannableString, NoUnderlineClickableSpan clickableSpan) {
        int start = spannableString.getSpanStart(clickableSpan);
        int end = spannableString.getSpanEnd(clickableSpan);
        return spannableString.subSequence(start, end).toString();
    }

    private static LegalMessageLine createLegalMessageLine(String text) {
        List<LegalMessageLine.Link> links = new ArrayList<>();
        links.add(new LegalMessageLine.Link(1, 3, LEGAL_MESSAGE_URL));
        LegalMessageLine legalMessageLine = new LegalMessageLine(text);
        legalMessageLine.links.addAll(links);
        return legalMessageLine;
    }
}
