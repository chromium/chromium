// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.save_card;

import static org.hamcrest.CoreMatchers.notNullValue;
import static org.hamcrest.CoreMatchers.nullValue;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.isEmptyString;
import static org.junit.Assert.assertEquals;

import android.view.View;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.test.filters.SmallTest;

import com.google.common.collect.ImmutableList;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.components.autofill.payments.LegalMessageLine.Link;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

import java.util.LinkedList;

/** Tests for {@link AutofillSaveCardBottomSheetViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class AutofillSaveCardBottomSheetViewBinderTest extends BlankUiTestActivityTestCase {
    @DrawableRes private static final int TEST_DRAWABLE_RES = R.drawable.arrow_up;

    private PropertyModel.Builder mModel;
    private AutofillSaveCardBottomSheetView mView;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        mModel = new PropertyModel.Builder(AutofillSaveCardBottomSheetProperties.ALL_KEYS);
        mView = new AutofillSaveCardBottomSheetView(getActivity());
        bind(mModel);
    }

    @Test
    @SmallTest
    public void testScrollView() {
        assertEquals(R.id.autofill_save_card_scroll_view, mView.mScrollView.getId());
    }

    @Test
    @SmallTest
    public void testTitle() {
        assertEquals(R.id.autofill_save_card_title_text, mView.mTitle.getId());
        verifyPropertyBoundToTextView(mView.mTitle, AutofillSaveCardBottomSheetProperties.TITLE);
    }

    @Test
    @SmallTest
    public void testDescription() {
        assertEquals(R.id.autofill_save_card_description_text, mView.mDescription.getId());
        verifyPropertyBoundToTextView(
                mView.mDescription, AutofillSaveCardBottomSheetProperties.DESCRIPTION);
    }

    @Test
    @SmallTest
    public void testLogoIcon() {
        assertEquals(R.id.autofill_save_card_icon, mView.mLogoIcon.getId());
        assertThat(mView.mLogoIcon.getDrawable(), nullValue());

        bind(mModel.with(AutofillSaveCardBottomSheetProperties.LOGO_ICON, 0));
        assertThat(mView.mLogoIcon.getDrawable(), nullValue());
        assertEquals(View.GONE, mView.mLogoIcon.getVisibility());

        bind(mModel.with(AutofillSaveCardBottomSheetProperties.LOGO_ICON, TEST_DRAWABLE_RES));
        assertThat(mView.mLogoIcon.getDrawable(), notNullValue());
        assertEquals(View.VISIBLE, mView.mLogoIcon.getVisibility());
    }

    @Test
    @SmallTest
    public void testCard() {
        assertEquals(R.id.autofill_credit_card_chip, mView.mCardView.getId());
    }

    @Test
    @SmallTest
    public void testCardDescription() {
        bind(mModel.with(AutofillSaveCardBottomSheetProperties.CARD_DESCRIPTION, ""));
        assertThat(String.valueOf(mView.mCardView.getContentDescription()), isEmptyString());

        final String descriptionText = "Card Description";
        bind(mModel.with(AutofillSaveCardBottomSheetProperties.CARD_DESCRIPTION, descriptionText));
        assertEquals(descriptionText, String.valueOf(mView.mCardView.getContentDescription()));
    }

    @Test
    @SmallTest
    public void testCardIcon() {
        assertEquals(R.id.autofill_save_card_credit_card_icon, mView.mCardIcon.getId());
        assertThat(mView.mCardIcon.getDrawable(), nullValue());

        bind(mModel.with(AutofillSaveCardBottomSheetProperties.CARD_ICON, 0));
        assertThat(mView.mCardIcon.getDrawable(), nullValue());

        bind(mModel.with(AutofillSaveCardBottomSheetProperties.CARD_ICON, TEST_DRAWABLE_RES));
        assertThat(mView.mCardIcon.getDrawable(), notNullValue());
    }

    @Test
    @SmallTest
    public void testCardLabel() {
        assertEquals(R.id.autofill_save_card_credit_card_label, mView.mCardLabel.getId());
        verifyPropertyBoundToTextView(
                mView.mCardLabel, AutofillSaveCardBottomSheetProperties.CARD_LABEL);
    }

    @Test
    @SmallTest
    public void testCardSubLabel() {
        assertEquals(R.id.autofill_save_card_credit_card_sublabel, mView.mCardSubLabel.getId());
        verifyPropertyBoundToTextView(
                mView.mCardSubLabel, AutofillSaveCardBottomSheetProperties.CARD_SUB_LABEL);
    }

    @Test
    @SmallTest
    public void testLegalMessage() {
        assertEquals(R.id.legal_message, mView.mLegalMessage.getId());
        assertThat(String.valueOf(mView.mLegalMessage.getText()), isEmptyString());

        bind(mModel.with(AutofillSaveCardBottomSheetProperties.LEGAL_MESSAGE, null));
        assertThat(String.valueOf(mView.mLegalMessage.getText()), isEmptyString());
        assertEquals(View.GONE, mView.mLegalMessage.getVisibility());

        bind(
                mModel.with(
                        AutofillSaveCardBottomSheetProperties.LEGAL_MESSAGE,
                        new AutofillSaveCardBottomSheetProperties.LegalMessage(
                                /* lines= */ null, /* link= */ null)));
        assertThat(String.valueOf(mView.mLegalMessage.getText()), isEmptyString());
        assertEquals(View.GONE, mView.mLegalMessage.getVisibility());

        bind(
                mModel.with(
                        AutofillSaveCardBottomSheetProperties.LEGAL_MESSAGE,
                        new AutofillSaveCardBottomSheetProperties.LegalMessage(
                                ImmutableList.of(), this::openLink)));
        assertThat(String.valueOf(mView.mLegalMessage.getText()), isEmptyString());
        assertEquals(View.GONE, mView.mLegalMessage.getVisibility());

        final String messageText = "Legal message line";
        LinkedList<LegalMessageLine> legalMessageLines = new LinkedList<>();
        legalMessageLines.add(new LegalMessageLine(messageText));
        bind(
                mModel.with(
                        AutofillSaveCardBottomSheetProperties.LEGAL_MESSAGE,
                        new AutofillSaveCardBottomSheetProperties.LegalMessage(
                                ImmutableList.copyOf(legalMessageLines), this::openLink)));
        assertEquals(messageText, String.valueOf(mView.mLegalMessage.getText()));
        assertEquals(View.VISIBLE, mView.mLegalMessage.getVisibility());

        LegalMessageLine legalMessageLine = new LegalMessageLine(messageText);
        legalMessageLine.links.add(new Link(0, 5, "https://example.test"));
        legalMessageLines = new LinkedList<>();
        legalMessageLines.add(legalMessageLine);
        bind(
                mModel.with(
                        AutofillSaveCardBottomSheetProperties.LEGAL_MESSAGE,
                        new AutofillSaveCardBottomSheetProperties.LegalMessage(
                                ImmutableList.copyOf(legalMessageLines), this::openLink)));
        assertEquals(messageText, String.valueOf(mView.mLegalMessage.getText()));
        assertEquals(View.VISIBLE, mView.mLegalMessage.getVisibility());
    }

    @Test
    @SmallTest
    public void testAcceptButtonLabel() {
        assertEquals(R.id.autofill_save_card_confirm_button, mView.mAcceptButton.getId());
        verifyPropertyBoundToTextView(
                mView.mAcceptButton, AutofillSaveCardBottomSheetProperties.ACCEPT_BUTTON_LABEL);
    }

    @Test
    @SmallTest
    public void testCancelButtonLabel() {
        assertEquals(R.id.autofill_save_card_cancel_button, mView.mCancelButton.getId());
        verifyPropertyBoundToTextView(
                mView.mCancelButton, AutofillSaveCardBottomSheetProperties.CANCEL_BUTTON_LABEL);
    }

    public void openLink(String url) {}

    private void bind(PropertyModel.Builder modelBuilder) {
        PropertyModelChangeProcessor.create(
                modelBuilder.build(), mView, AutofillSaveCardBottomSheetViewBinder::bind);
    }

    private void verifyPropertyBoundToTextView(
            TextView view, ReadableObjectPropertyKey<String> property) {
        assertThat(String.valueOf(view.getText()), isEmptyString());

        bind(mModel.with(property, null));
        assertThat(String.valueOf(view.getText()), isEmptyString());

        final String messageText = "Test Message";
        bind(mModel.with(property, messageText));
        assertEquals(messageText, String.valueOf(view.getText()));
    }
}
