// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownFieldProperties.DROPDOWN_ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownFieldProperties.DROPDOWN_KEY_VALUE_LIST;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.IS_REQUIRED;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.LABEL;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.VALIDATOR;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.VALUE;

import android.app.Activity;
import android.text.TextUtils;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownKeyValue;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.Arrays;
import java.util.List;

/** Unit test for {@link DropdownFieldView}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class DropdownFieldViewUnitTest {
    private Activity mActivity;
    private ViewGroup mContentView;

    @Before
    public void setUp() {
        mActivity = Robolectric.setupActivity(Activity.class);
        mContentView = new LinearLayout(mActivity);
    }

    private PropertyModel buildDefaultPropertyModel() {
        List<DropdownKeyValue> keyValues =
                Arrays.asList(
                        new DropdownKeyValue("key1", "value1"),
                        new DropdownKeyValue("key2", "value2"));
        return new PropertyModel.Builder(DROPDOWN_ALL_KEYS)
                .with(IS_REQUIRED, false)
                .with(DROPDOWN_KEY_VALUE_LIST, keyValues)
                .with(LABEL, "label")
                .build();
    }

    private DropdownFieldView attachDropdownFieldView(PropertyModel model) {
        DropdownFieldView field = new DropdownFieldView(mActivity, mContentView, model);
        PropertyModelChangeProcessor.create(
                model, field, EditorDialogViewBinder::bindDropdownFieldView);
        return field;
    }

    /** Test that no error message is displayed if there aren't any validation errors. */
    @Test
    public void testNoErrors() {
        PropertyModel model = buildDefaultPropertyModel();
        model.set(VALIDATOR, EditorFieldValidator.builder().build());
        DropdownFieldView field = attachDropdownFieldView(model);
        model.set(VALUE, "value2");

        assertTrue(field.validate());
        assertTrue(TextUtils.isEmpty(field.getErrorLabelForTests().getText()));
    }

    /** Test that the initial error message is cleared when the user changes the dropdown value. */
    @Test
    public void testInitialErrorMessage() {
        PropertyModel model = buildDefaultPropertyModel();
        model.set(
                VALIDATOR,
                EditorFieldValidator.builder()
                        .withInitialErrorMessage("Initial error message")
                        .build());
        DropdownFieldView field = attachDropdownFieldView(model);
        model.set(VALUE, "value2");

        // Check initial state
        assertEquals(1, field.getDropdown().getSelectedItemPosition());
        assertFalse(field.validate());
        assertFalse(TextUtils.isEmpty(field.getErrorLabelForTests().getText()));

        // Change value.
        field.getDropdown().setSelection(0);
        assertTrue(field.validate());
        assertTrue(TextUtils.isEmpty(field.getErrorLabelForTests().getText()));
    }

    /**
     * Test that the error message is not cleared when the user changes the dropdown value if the
     * error message is due to a predicate failing.
     */
    @Test
    public void testEditKeepInvalid() {
        PropertyModel model = buildDefaultPropertyModel();
        model.set(
                VALIDATOR,
                EditorFieldValidator.builder()
                        .withValidationPredicate(
                                (value) -> {
                                    return false;
                                },
                                "Error Message")
                        .build());
        DropdownFieldView field = attachDropdownFieldView(model);
        model.set(VALUE, "value2");

        // Check initial state
        assertEquals(1, field.getDropdown().getSelectedItemPosition());
        assertFalse(field.validate());
        assertFalse(TextUtils.isEmpty(field.getErrorLabelForTests().getText()));

        // Change value.
        field.getDropdown().setSelection(0);
        assertFalse(field.validate());
        assertFalse(TextUtils.isEmpty(field.getErrorLabelForTests().getText()));
    }
}
