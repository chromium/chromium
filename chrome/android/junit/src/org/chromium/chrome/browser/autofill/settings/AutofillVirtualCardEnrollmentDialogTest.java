// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Bitmap;
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
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeStringConstants;
import org.chromium.chrome.browser.autofill.LegalMessageLine;
import org.chromium.chrome.browser.ui.autofill.FakeModalDialogManager;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.text.NoUnderlineClickableSpan;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link AutofillVirtualCardEnrollmentDialog}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillVirtualCardEnrollmentDialogTest {
    private static final String LEGAL_MESSAGE_URL = "http://www.google.com";
    private static final String ACCEPT_BUTTON_TEXT = "Yes";
    private static final String DECLINE_BUTTON_TEXT = "No thanks";

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private Callback<Integer> mResultHandlerMock;
    @Mock
    private Callback<String> mOnEducationTextLinkClickedMock;
    @Mock
    private Callback<String> mOnGoogleLegalMessageLinkClickedMock;
    @Mock
    private Callback<String> mOnIssuerLegalMessageLinkClickedMock;
    private FakeModalDialogManager mModalDialogManager;
    private AutofillVirtualCardEnrollmentDialog mDialog;
    private VirtualCardEnrollmentFields mVirtualCardEnrollmentFields;

    @Before
    public void setUp() {
        mModalDialogManager = new FakeModalDialogManager(ModalDialogType.APP);
        mVirtualCardEnrollmentFields = VirtualCardEnrollmentFields.create(
                "card label", Bitmap.createBitmap(100, 100, Bitmap.Config.ALPHA_8));
        mVirtualCardEnrollmentFields.mGoogleLegalMessages.add(createLegalMessageLine("google"));
        mVirtualCardEnrollmentFields.mIssuerLegalMessages.add(createLegalMessageLine("issuer"));
        mDialog = new AutofillVirtualCardEnrollmentDialog(
                ApplicationProvider.getApplicationContext(), mModalDialogManager,
                mVirtualCardEnrollmentFields, ACCEPT_BUTTON_TEXT, DECLINE_BUTTON_TEXT,
                mOnEducationTextLinkClickedMock, mOnGoogleLegalMessageLinkClickedMock,
                mOnIssuerLegalMessageLinkClickedMock, mResultHandlerMock);
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
    public void dialogDismissed() {
        assertThat(mModalDialogManager.getShownDialogModel()).isNotNull();
        // Simulate dialog dismissal by native.
        mDialog.dismiss(DialogDismissalCause.DISMISSED_BY_NATIVE);
        assertThat(mModalDialogManager.getShownDialogModel()).isNull();
        // Check that callback was called with dismissed by native as the dismissal cause.
        verify(mResultHandlerMock).onResult(DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    @Test
    @SmallTest
    public void learnMoreTextClicked() {
        // Create activity.
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        // Create a new AutofillVirtualCardEnrollmentDialog with Activity as the context instead.
        mDialog = new AutofillVirtualCardEnrollmentDialog(activity, mModalDialogManager,
                mVirtualCardEnrollmentFields, ACCEPT_BUTTON_TEXT, DECLINE_BUTTON_TEXT,
                mOnEducationTextLinkClickedMock, mOnGoogleLegalMessageLinkClickedMock,
                mOnIssuerLegalMessageLinkClickedMock, mResultHandlerMock);
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
        // Verify that the callback is called with url for learn more page.
        verify(mOnEducationTextLinkClickedMock)
                .onResult(ChromeStringConstants.AUTOFILL_VIRTUAL_CARD_ENROLLMENT_SUPPORT_URL);
    }

    @Test
    @SmallTest
    public void googleLegalMessageClicked() {
        // Create activity.
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        // Create a new AutofillVirtualCardEnrollmentDialog with Activity as the context instead.
        mDialog = new AutofillVirtualCardEnrollmentDialog(activity, mModalDialogManager,
                mVirtualCardEnrollmentFields, ACCEPT_BUTTON_TEXT, DECLINE_BUTTON_TEXT,
                mOnEducationTextLinkClickedMock, mOnGoogleLegalMessageLinkClickedMock,
                mOnIssuerLegalMessageLinkClickedMock, mResultHandlerMock);
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
        // Verify that the callback is called with LEGAL_MESSAGE_URL.
        verify(mOnGoogleLegalMessageLinkClickedMock).onResult(LEGAL_MESSAGE_URL);
    }

    @Test
    @SmallTest
    public void issuerLegalMessageClicked() {
        // Create activity.
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        // Create a new AutofillVirtualCardEnrollmentDialog with Activity as the context instead.
        mDialog = new AutofillVirtualCardEnrollmentDialog(activity, mModalDialogManager,
                mVirtualCardEnrollmentFields, ACCEPT_BUTTON_TEXT, DECLINE_BUTTON_TEXT,
                mOnEducationTextLinkClickedMock, mOnGoogleLegalMessageLinkClickedMock,
                mOnIssuerLegalMessageLinkClickedMock, mResultHandlerMock);
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
        // Verify that the callback is called with LEGAL_MESSAGE_URL.
        verify(mOnIssuerLegalMessageLinkClickedMock).onResult(LEGAL_MESSAGE_URL);
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
