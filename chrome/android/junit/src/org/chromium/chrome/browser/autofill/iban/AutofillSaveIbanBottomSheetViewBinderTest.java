// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.autofill.iban;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.notNullValue;
import static org.hamcrest.CoreMatchers.nullValue;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.isEmptyString;
import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.components.autofill.payments.LegalMessageLine.Link;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Tests for {@link AutofillSaveIbanBottomSheetViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillSaveIbanBottomSheetViewBinderTest {
    @DrawableRes private static final int TEST_DRAWABLE_RES = R.drawable.arrow_up;

    private PropertyModel.Builder mModelBuilder;
    private PropertyModel mModel;
    private AutofillSaveIbanBottomSheetView mView;

    @Before
    public void setUpTest() throws Exception {
        MockitoAnnotations.initMocks(this);
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        // Set a MaterialComponents theme which is required for the `OutlinedBox` text field.
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mView = new AutofillSaveIbanBottomSheetView(activity);
        mModelBuilder = new PropertyModel.Builder(AutofillSaveIbanBottomSheetProperties.ALL_KEYS);
        bind(mModelBuilder);
    }

    @Test
    @SmallTest
    public void testScrollView() {
        assertEquals(R.id.autofill_save_iban_scroll_view, mView.mScrollView.getId());
    }

    @Test
    @SmallTest
    public void testLogoIcon() {
        assertEquals(R.id.autofill_save_iban_google_pay_icon, mView.mLogoIcon.getId());
        assertThat(mView.mLogoIcon.getDrawable(), nullValue());

        bind(
                mModelBuilder.with(
                        AutofillSaveIbanBottomSheetProperties.LOGO_ICON, TEST_DRAWABLE_RES));
        assertThat(mView.mLogoIcon.getDrawable(), notNullValue());
    }

    @Test
    @SmallTest
    public void testTitle() {
        verifyPropertyBoundToTextView(
                mView.mContentView.findViewById(R.id.autofill_save_iban_title_text),
                AutofillSaveIbanBottomSheetProperties.TITLE);
    }

    @Test
    @SmallTest
    public void testDescription() {
        verifyPropertyBoundToTextView(
                mView.mContentView.findViewById(R.id.autofill_save_iban_description_text),
                AutofillSaveIbanBottomSheetProperties.DESCRIPTION);
    }

    @Test
    @SmallTest
    public void testIbanValue() {
        verifyPropertyBoundToTextView(
                mView.mContentView.findViewById(R.id.autofill_save_iban_value),
                AutofillSaveIbanBottomSheetProperties.IBAN_VALUE);
    }

    @Test
    @SmallTest
    public void testAcceptButton() {
        verifyPropertyBoundToTextView(
                mView.mContentView.findViewById(R.id.autofill_save_iban_confirm_button),
                AutofillSaveIbanBottomSheetProperties.ACCEPT_BUTTON_LABEL);
    }

    @Test
    @SmallTest
    public void testCancelButton() {
        verifyPropertyBoundToTextView(
                mView.mContentView.findViewById(R.id.autofill_save_iban_cancel_button),
                AutofillSaveIbanBottomSheetProperties.CANCEL_BUTTON_LABEL);
    }

    @Test
    @SmallTest
    public void testLegalMessage() {
        // Test empty legal message.
        bind(
                mModelBuilder.with(
                        AutofillSaveIbanBottomSheetProperties.LEGAL_MESSAGE,
                        new AutofillSaveIbanBottomSheetProperties.LegalMessage(
                                Collections.EMPTY_LIST, (unused) -> {})));
        assertThat(String.valueOf(mView.mLegalMessage.getText()), isEmptyString());
        assertEquals(View.GONE, mView.mLegalMessage.getVisibility());

        // Test non-empty legal message.
        final String messageText = "Legal message line";
        List<LegalMessageLine> legalMessageLines = new ArrayList<>();
        LegalMessageLine legalMessageLine = new LegalMessageLine(messageText);
        legalMessageLine.links.add(new Link(0, 5, "https://example.test"));
        legalMessageLines.add(legalMessageLine);
        bind(
                mModelBuilder.with(
                        AutofillSaveIbanBottomSheetProperties.LEGAL_MESSAGE,
                        new AutofillSaveIbanBottomSheetProperties.LegalMessage(
                                legalMessageLines, (unused) -> {})));
        assertEquals(messageText, String.valueOf(mView.mLegalMessage.getText()));
        assertEquals(View.VISIBLE, mView.mLegalMessage.getVisibility());
    }

    private void bind(PropertyModel.Builder modelBuilder) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel = modelBuilder.build();
                    PropertyModelChangeProcessor.create(
                            mModel, mView, AutofillSaveIbanBottomSheetViewBinder::bind);
                });
    }

    private void verifyPropertyBoundToTextView(
            TextView view, ReadableObjectPropertyKey<String> property) {
        assertThat(String.valueOf(view.getText()), isEmptyString());

        bind(mModelBuilder.with(property, "Text view content"));
        assertThat(String.valueOf(view.getText()), equalTo("Text view content"));
    }
}
