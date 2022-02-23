// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
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
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowActivity;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.LegalMessageLine;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.text.NoUnderlineClickableSpan;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link AutofillVirtualCardEnrollmentDialog}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillVirtualCardEnrollmentDialogTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private Callback<Boolean> mCallbackMock;
    private FakeModalDialogManager mModalDialogManager;
    private AutofillVirtualCardEnrollmentDialog mDialog;
    private VirtualCardEnrollmentFields mVirtualCardEnrollmentFields;

    @Before
    public void setUp() {
        mModalDialogManager = new FakeModalDialogManager();
        mVirtualCardEnrollmentFields = VirtualCardEnrollmentFields.create(
                "card label", Bitmap.createBitmap(100, 100, Bitmap.Config.ALPHA_8));
        mVirtualCardEnrollmentFields.mGoogleLegalMessages.add(createLegalMessageLine("google"));
        mVirtualCardEnrollmentFields.mIssuerLegalMessages.add(createLegalMessageLine("issuer"));
        mDialog =
                new AutofillVirtualCardEnrollmentDialog(ApplicationProvider.getApplicationContext(),
                        mModalDialogManager, mVirtualCardEnrollmentFields, mCallbackMock);
        mDialog.show();
    }

    @Test
    @SmallTest
    public void dialogShown() {
        assertThat(mModalDialogManager.getShownDialogModel()).isNotNull();
        // The callback should not have been called yet.
        verify(mCallbackMock, never()).onResult(any());
    }

    @Test
    @SmallTest
    public void positiveButtonPressed() {
        assertThat(mModalDialogManager.getShownDialogModel()).isNotNull();
        mModalDialogManager.clickPositiveButton();
        assertThat(mModalDialogManager.getShownDialogModel()).isNull();
        // Check that callback was called with true.
        verify(mCallbackMock).onResult(true);
    }

    @Test
    @SmallTest
    public void negativeButtonPressed() {
        assertThat(mModalDialogManager.getShownDialogModel()).isNotNull();
        mModalDialogManager.clickNegativeButton();
        assertThat(mModalDialogManager.getShownDialogModel()).isNull();
        // Check that callback was called with false;
        verify(mCallbackMock).onResult(false);
    }

    @Test
    @SmallTest
    public void learnMoreTextClicked() {
        // Create activity and its shadow to see if CustomTabActivity is launched.
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        ShadowActivity shadowActivity = Shadows.shadowOf(activity);
        // Create a new AutofillVirtualCardEnrollmentDialog with Activity as the context instead.
        mDialog = new AutofillVirtualCardEnrollmentDialog(
                activity, mModalDialogManager, mVirtualCardEnrollmentFields, mCallbackMock);
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
        // Check that the CustomTabActivity was started as expected.
        Intent startedIntent = shadowActivity.getNextStartedActivity();
        assertThat(startedIntent.getComponent())
                .isEqualTo(new ComponentName(activity, CustomTabActivity.class));
    }

    @Test
    @SmallTest
    public void googleLegalMessageClicked() {
        // Create activity and its shadow to see if CustomTabActivity is launched.
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        ShadowActivity shadowActivity = Shadows.shadowOf(activity);
        // Create a new AutofillVirtualCardEnrollmentDialog with Activity as the context instead.
        mDialog = new AutofillVirtualCardEnrollmentDialog(
                activity, mModalDialogManager, mVirtualCardEnrollmentFields, mCallbackMock);
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
        // Check that the CustomTabActivity was started as expected.
        Intent startedIntent = shadowActivity.getNextStartedActivity();
        assertThat(startedIntent.getComponent())
                .isEqualTo(new ComponentName(activity, CustomTabActivity.class));
    }

    @Test
    @SmallTest
    public void issuerLegalMessageClicked() {
        // Create activity and its shadow to see if CustomTabActivity is launched.
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        ShadowActivity shadowActivity = Shadows.shadowOf(activity);
        // Create a new AutofillVirtualCardEnrollmentDialog with Activity as the context instead.
        mDialog = new AutofillVirtualCardEnrollmentDialog(
                activity, mModalDialogManager, mVirtualCardEnrollmentFields, mCallbackMock);
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
        // Check that the CustomTabActivity was started as expected.
        Intent startedIntent = shadowActivity.getNextStartedActivity();
        assertThat(startedIntent.getComponent())
                .isEqualTo(new ComponentName(activity, CustomTabActivity.class));
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
        links.add(new LegalMessageLine.Link(1, 3, "http://www.google.com"));
        LegalMessageLine legalMessageLine = new LegalMessageLine(text);
        legalMessageLine.links.addAll(links);
        return legalMessageLine;
    }
}
