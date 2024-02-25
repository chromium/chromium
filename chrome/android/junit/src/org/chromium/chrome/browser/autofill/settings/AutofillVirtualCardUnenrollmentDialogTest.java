// Copyright 2022 The Chromium Authors
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
import android.text.SpannableString;

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
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;
import org.chromium.ui.text.NoUnderlineClickableSpan;

/** Unit tests for {@link AutofillVirtualCardUnenrollmentDialog} */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillVirtualCardUnenrollmentDialogTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Callback<Boolean> mCallbackMock;
    private FakeModalDialogManager mModalDialogManager;
    private AutofillVirtualCardUnenrollmentDialog mDialog;

    @Before
    public void setUp() {
        mModalDialogManager = new FakeModalDialogManager(ModalDialogType.APP);
        mDialog =
                new AutofillVirtualCardUnenrollmentDialog(
                        ApplicationProvider.getApplicationContext(),
                        mModalDialogManager,
                        mCallbackMock);
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
        // Create a new AutofillVirtualCardUnenrollmentDialog with Activity as the context instead.
        mDialog =
                new AutofillVirtualCardUnenrollmentDialog(
                        activity, mModalDialogManager, mCallbackMock);
        mDialog.show();
        // Make sure that the dialog was shown properly.
        assertThat(mModalDialogManager.getShownDialogModel()).isNotNull();
        // Get the "Learn more about virtual cards" span.
        SpannableString str =
                (SpannableString)
                        mModalDialogManager
                                .getShownDialogModel()
                                .get(ModalDialogProperties.MESSAGE_PARAGRAPH_1);
        // Assert that the message is not empty.
        assertThat(str.length()).isGreaterThan(0);
        NoUnderlineClickableSpan[] spans =
                str.getSpans(0, str.length(), NoUnderlineClickableSpan.class);
        // Assert that there is only one NoUnderlineClickableSpan.
        assertThat(spans.length).isEqualTo(1);
        // Assert that the text of this span is correct.
        NoUnderlineClickableSpan learnMoreSpan = spans[0];
        int start = str.getSpanStart(learnMoreSpan);
        int end = str.getSpanEnd(learnMoreSpan);
        assertThat(str.subSequence(start, end).toString())
                .isEqualTo("Learn more about virtual cards");
        // Click on the link. The callback doesn't use the view so it can be null.
        learnMoreSpan.onClick(null);
        // Check that the CustomTabActivity was started as expected.
        Intent startedIntent = shadowActivity.getNextStartedActivity();
        assertThat(startedIntent.getComponent())
                .isEqualTo(new ComponentName(activity, CustomTabActivity.class));
    }
}
