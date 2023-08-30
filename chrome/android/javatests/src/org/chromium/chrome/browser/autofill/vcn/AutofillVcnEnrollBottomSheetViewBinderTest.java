// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.isEmptyString;

import android.widget.Button;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

/** Tests for {@link AutofillVcnEnrollBottomSheetViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public final class AutofillVcnEnrollBottomSheetViewBinderTest extends BlankUiTestActivityTestCase {
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
