// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

import static org.junit.Assert.assertFalse;

import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownFieldProperties.DROPDOWN_ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownFieldProperties.DROPDOWN_KEY_VALUE_LIST;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.FOCUSED;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.LABEL;

import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.MediumTest;

import com.google.android.material.textfield.TextInputEditText;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownKeyValue;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.Arrays;
import java.util.List;

/** Javatests for {@link DropdownFieldView} */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class DropdownFieldViewTest {
    private ViewGroup mContentView;
    private View mOtherFocusableField;

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Before
    public void setUpTest() throws Exception {
        mActivityTestRule.launchActivity(null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContentView = new LinearLayout(mActivityTestRule.getActivity());
                    mOtherFocusableField = new TextInputEditText(mActivityTestRule.getActivity());
                    mContentView.addView(mOtherFocusableField);
                });
    }

    /**
     * Test that focusing the dropdown, then focusing a different field clears the FOCUSED property.
     */
    @Test
    @MediumTest
    @UiThreadTest
    public void testFocus() {
        List<DropdownKeyValue> keyValues =
                Arrays.asList(
                        new DropdownKeyValue("key1", "value1"),
                        new DropdownKeyValue("key2", "value2"));
        PropertyModel model =
                new PropertyModel.Builder(DROPDOWN_ALL_KEYS)
                        .with(DROPDOWN_KEY_VALUE_LIST, keyValues)
                        .with(LABEL, "label")
                        .build();

        DropdownFieldView dropdown =
                new DropdownFieldView(mActivityTestRule.getActivity(), mContentView, model);
        PropertyModelChangeProcessor.create(
                model, dropdown, EditorDialogViewBinder::bindDropdownFieldView);
        model.set(FOCUSED, true);

        mOtherFocusableField.requestFocus();
        assertFalse(model.get(FOCUSED));
    }
}
