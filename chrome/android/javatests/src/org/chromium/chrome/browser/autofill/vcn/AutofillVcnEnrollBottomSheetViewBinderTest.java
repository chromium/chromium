// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.notNullValue;
import static org.hamcrest.CoreMatchers.nullValue;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.isEmptyString;

import android.graphics.Bitmap;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.autofill.vcn.AutofillVcnEnrollBottomSheetProperties.Description;
import org.chromium.chrome.browser.autofill.vcn.AutofillVcnEnrollBottomSheetProperties.IssuerIcon;
import org.chromium.chrome.browser.autofill.vcn.AutofillVcnEnrollBottomSheetProperties.LegalMessages;
import org.chromium.chrome.browser.autofill.vcn.AutofillVcnEnrollBottomSheetProperties.LinkOpener;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.autofill.VirtualCardEnrollmentLinkType;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.components.autofill.payments.LegalMessageLine.Link;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

import java.util.Arrays;
import java.util.LinkedList;

/** Tests for {@link AutofillVcnEnrollBottomSheetViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public final class AutofillVcnEnrollBottomSheetViewBinderTest
        extends BlankUiTestActivityTestCase implements LinkOpener {
    private PropertyModel.Builder mModel;
    private AutofillVcnEnrollBottomSheetView mView;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        mModel = new PropertyModel.Builder(AutofillVcnEnrollBottomSheetProperties.ALL_KEYS);
        mView = new AutofillVcnEnrollBottomSheetView(getActivity());
        bind(mModel);
    }

    // Builds the model from the given builder and binds it to the view.
    private void bind(PropertyModel.Builder modelBuilder) {
        PropertyModelChangeProcessor.create(
                modelBuilder.build(), mView, AutofillVcnEnrollBottomSheetViewBinder::bind);
    }

    // LinkOpener:
    @Override
    public void openLink(String url, @VirtualCardEnrollmentLinkType int linkType) {}

    @Test
    @SmallTest
    public void testMessageTextInDialogTitle() {
        assertThat(String.valueOf(mView.mDialogTitle.getText()), isEmptyString());

        bind(mModel.with(AutofillVcnEnrollBottomSheetProperties.MESSAGE_TEXT, null));
        assertThat(String.valueOf(mView.mDialogTitle.getText()), isEmptyString());

        bind(mModel.with(AutofillVcnEnrollBottomSheetProperties.MESSAGE_TEXT, "Message text"));
        assertThat(String.valueOf(mView.mDialogTitle.getText()), equalTo("Message text"));
    }

    @Test
    @SmallTest
    public void testDescriptionText() {
        assertThat(String.valueOf(mView.mVirtualCardDescription.getText()), isEmptyString());

        bind(mModel.with(AutofillVcnEnrollBottomSheetProperties.DESCRIPTION, null));
        assertThat(String.valueOf(mView.mVirtualCardDescription.getText()), isEmptyString());

        bind(mModel.with(AutofillVcnEnrollBottomSheetProperties.DESCRIPTION,
                new Description(null, null, null,
                        VirtualCardEnrollmentLinkType.VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK,
                        /*linkOpener=*/null)));
        assertThat(String.valueOf(mView.mVirtualCardDescription.getText()), isEmptyString());

        bind(mModel.with(AutofillVcnEnrollBottomSheetProperties.DESCRIPTION,
                new Description("", "", "",
                        VirtualCardEnrollmentLinkType.VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK,
                        /*linkOpener=*/this)));
        assertThat(String.valueOf(mView.mVirtualCardDescription.getText()), isEmptyString());

        bind(mModel.with(AutofillVcnEnrollBottomSheetProperties.DESCRIPTION,
                new Description("Description text", "", "",
                        VirtualCardEnrollmentLinkType.VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK,
                        /*linkOpener=*/this)));
        assertThat(String.valueOf(mView.mVirtualCardDescription.getText()), isEmptyString());

        bind(mModel.with(AutofillVcnEnrollBottomSheetProperties.DESCRIPTION,
                new Description("Description text", "text", "",
                        VirtualCardEnrollmentLinkType.VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK,
                        /*linkOpener=*/this)));
        assertThat(String.valueOf(mView.mVirtualCardDescription.getText()), isEmptyString());

        bind(mModel.with(AutofillVcnEnrollBottomSheetProperties.DESCRIPTION,
                new Description("Description text", "text", "https://example.test",
                        VirtualCardEnrollmentLinkType.VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK,
                        /*linkOpener=*/this)));
        assertThat(String.valueOf(mView.mVirtualCardDescription.getText()),
                equalTo("Description text"));
    }

    @Test
    @SmallTest
    public void testCardContainerAccessibilityDescription() {
        ReadableObjectPropertyKey<String> descriptionProperty =
                AutofillVcnEnrollBottomSheetProperties.CARD_CONTAINER_ACCESSIBILITY_DESCRIPTION;

        bind(mModel.with(descriptionProperty, ""));
        assertThat(String.valueOf(mView.mCardContainer.getContentDescription()), isEmptyString());

        bind(mModel.with(descriptionProperty, "Content description"));
        assertThat(String.valueOf(mView.mCardContainer.getContentDescription()),
                equalTo("Content description"));
    }

    @Test
    @SmallTest
    public void testIssuerIcon() {
        assertThat(mView.mIssuerIcon.getDrawable(), nullValue());

        bind(mModel.with(AutofillVcnEnrollBottomSheetProperties.ISSUER_ICON,
                new IssuerIcon(createBitmap(/*dimensions=*/10, /*color=*/0xFFFF0000), /*width=*/5,
                        /*height=*/5)));
        assertThat(mView.mIssuerIcon.getDrawable(), notNullValue());
    }

    private static Bitmap createBitmap(int dimensions, int color) {
        int[] colors = new int[dimensions * dimensions];
        Arrays.fill(colors, color);
        return Bitmap.createBitmap(colors, dimensions, dimensions, Bitmap.Config.ARGB_8888);
    }

    @Test
    @SmallTest
    public void testCardLabel() {
        runTextViewTest(mView.mCardLabel, AutofillVcnEnrollBottomSheetProperties.CARD_LABEL);
    }

    @Test
    @SmallTest
    public void testCardDescription() {
        runTextViewTest(
                mView.mCardDescription, AutofillVcnEnrollBottomSheetProperties.CARD_DESCRIPTION);
    }

    private void runTextViewTest(TextView view, ReadableObjectPropertyKey<String> property) {
        assertThat(String.valueOf(view.getText()), isEmptyString());

        bind(mModel.with(property, null));
        assertThat(String.valueOf(view.getText()), isEmptyString());

        bind(mModel.with(property, "Text view content"));
        assertThat(String.valueOf(view.getText()), equalTo("Text view content"));
    }

    @Test
    @SmallTest
    public void testGoogleLegalMessages() {
        runLegalMessageTest(mView.mGoogleLegalMessage,
                AutofillVcnEnrollBottomSheetProperties.GOOGLE_LEGAL_MESSAGES);
    }

    @Test
    @SmallTest
    public void testIssuerLegalMessages() {
        runLegalMessageTest(mView.mIssuerLegalMessage,
                AutofillVcnEnrollBottomSheetProperties.ISSUER_LEGAL_MESSAGES);
    }

    private void runLegalMessageTest(
            TextView view, ReadableObjectPropertyKey<LegalMessages> property) {
        assertThat(String.valueOf(view.getText()), isEmptyString());

        bind(mModel.with(property, null));
        assertThat(String.valueOf(view.getText()), isEmptyString());
        assertThat(view.getVisibility(), equalTo(View.GONE));

        bind(mModel.with(property,
                new LegalMessages(null,
                        VirtualCardEnrollmentLinkType.VIRTUAL_CARD_ENROLLMENT_ISSUER_TOS_LINK,
                        /*linkOpener=*/null)));
        assertThat(String.valueOf(view.getText()), isEmptyString());
        assertThat(view.getVisibility(), equalTo(View.GONE));

        bind(mModel.with(property,
                new LegalMessages(new LinkedList<LegalMessageLine>(),
                        VirtualCardEnrollmentLinkType.VIRTUAL_CARD_ENROLLMENT_ISSUER_TOS_LINK,
                        /*linkOpener=*/this)));
        assertThat(String.valueOf(view.getText()), isEmptyString());
        assertThat(view.getVisibility(), equalTo(View.GONE));

        LinkedList<LegalMessageLine> lines = new LinkedList<>();
        lines.add(new LegalMessageLine("Legal message line"));
        bind(mModel.with(property,
                new LegalMessages(lines,
                        VirtualCardEnrollmentLinkType.VIRTUAL_CARD_ENROLLMENT_ISSUER_TOS_LINK,
                        /*linkOpener=*/this)));
        assertThat(String.valueOf(view.getText()), equalTo("Legal message line"));
        assertThat(view.getVisibility(), equalTo(View.VISIBLE));

        LegalMessageLine line = new LegalMessageLine("Legal message line");
        line.links.add(new Link(0, 5, "https://example.test"));
        lines = new LinkedList<>();
        lines.add(line);
        bind(mModel.with(property,
                new LegalMessages(lines,
                        VirtualCardEnrollmentLinkType.VIRTUAL_CARD_ENROLLMENT_ISSUER_TOS_LINK,
                        /*linkOpener=*/this)));
        assertThat(String.valueOf(view.getText()), equalTo("Legal message line"));
        assertThat(view.getVisibility(), equalTo(View.VISIBLE));
    }

    @Test
    @SmallTest
    public void testAcceptButtonLabel() {
        runButtonLabelTest(
                mView.mAcceptButton, AutofillVcnEnrollBottomSheetProperties.ACCEPT_BUTTON_LABEL);
    }

    @Test
    @SmallTest
    public void testCancelButtonLabel() {
        runButtonLabelTest(
                mView.mCancelButton, AutofillVcnEnrollBottomSheetProperties.CANCEL_BUTTON_LABEL);
    }

    private void runButtonLabelTest(Button button, ReadableObjectPropertyKey<String> property) {
        assertThat(String.valueOf(button.getText()), isEmptyString());

        bind(mModel.with(property, null));
        assertThat(String.valueOf(button.getText()), isEmptyString());

        bind(mModel.with(property, "Button Action Text"));
        assertThat(String.valueOf(button.getText()), equalTo("Button Action Text"));
    }
}
