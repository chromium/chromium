// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.autofill.iban;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.isEmptyString;
import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for {@link AutofillSaveIbanBottomSheetViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillSaveIbanBottomSheetViewBinderTest {
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
    public void testTitle() {
        verifyPropertyBoundToTextView(
                mView.mContentView.findViewById(R.id.autofill_local_save_iban_title_text),
                AutofillSaveIbanBottomSheetProperties.TITLE);
    }

    @Test
    @SmallTest
    public void testIbanLabel() {
        verifyPropertyBoundToTextView(
                mView.mContentView.findViewById(R.id.autofill_save_iban_label),
                AutofillSaveIbanBottomSheetProperties.IBAN_LABEL);
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
