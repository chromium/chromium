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
import android.widget.AutoCompleteTextView;
import android.widget.LinearLayout;
import android.widget.Spinner;

import com.google.android.material.textfield.TextInputLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.autofill.DropdownKeyValue;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Unit test for {@link DropdownFieldView}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class DropdownFieldViewUnitTest {
    private Activity mActivity;
    private ViewGroup mContentView;

    @Before
    public void setUp() {
        mActivity = Robolectric.setupActivity(Activity.class);
        mActivity.setTheme(R.style.Theme_MaterialComponents);
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

    private PropertyModel buildPropertyModelNoValues() {
        return new PropertyModel.Builder(DROPDOWN_ALL_KEYS)
                .with(IS_REQUIRED, false)
                .with(DROPDOWN_KEY_VALUE_LIST, Collections.emptyList())
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
    @Features.DisableFeatures(ChromeFeatureList.ANDROID_SETTINGS_CONTAINMENT)
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
        assertTrue(field.getDropdown() instanceof Spinner);
        assertEquals(1, ((Spinner) field.getDropdown()).getSelectedItemPosition());
        assertFalse(field.validate());
        assertFalse(TextUtils.isEmpty(field.getErrorLabelForTests().getText()));

        // Change value.
        ((Spinner) field.getDropdown()).setSelection(0);
        assertTrue(field.validate());
        assertTrue(TextUtils.isEmpty(field.getErrorLabelForTests().getText()));
    }

    /**
     * Test that the error message is not cleared when the user changes the dropdown value if the
     * error message is due to a predicate failing.
     */
    @Test
    @Features.DisableFeatures(ChromeFeatureList.ANDROID_SETTINGS_CONTAINMENT)
    public void testEditKeepInvalid() {
        PropertyModel model = buildDefaultPropertyModel();
        model.set(
                VALIDATOR,
                EditorFieldValidator.builder()
                        .withValidationPredicate((value) -> false, "Error Message")
                        .build());
        DropdownFieldView field = attachDropdownFieldView(model);
        model.set(VALUE, "value2");

        // Check initial state
        assertEquals(1, ((Spinner) field.getDropdown()).getSelectedItemPosition());
        assertFalse(field.validate());
        assertFalse(TextUtils.isEmpty(field.getErrorLabelForTests().getText()));

        // Change value.
        ((Spinner) field.getDropdown()).setSelection(0);
        assertFalse(field.validate());
        assertFalse(TextUtils.isEmpty(field.getErrorLabelForTests().getText()));
    }

    /**
     * Test that the error message is not cleared when the user changes the dropdown value if the
     * error message is due to a predicate failing.
     */
    @Test
    @Features.EnableFeatures(ChromeFeatureList.ANDROID_SETTINGS_CONTAINMENT)
    public void testEditKeepInvalidWithContainmentOn() {
        PropertyModel model = buildDefaultPropertyModel();
        model.set(
                VALIDATOR,
                EditorFieldValidator.builder()
                        .withValidationPredicate(value -> false, "Error Message")
                        .build());
        DropdownFieldView field = attachDropdownFieldView(model);
        model.set(VALUE, "value2");

        // Check initial state
        assertFalse(field.validate());
        assertEquals(
                "Error Message",
                ((TextInputLayout) field.getLayout().findViewById(R.id.dropdown_container))
                        .getError());

        // Change value.
        assertTrue(field.getDropdown() instanceof AutoCompleteTextView);
        ((AutoCompleteTextView) field.getDropdown()).setText("value1", false);
        assertFalse(field.validate());
        assertEquals(
                "Error Message",
                ((TextInputLayout) field.getLayout().findViewById(R.id.dropdown_container))
                        .getError());
    }

    /** Test that the content description for dropdown elements is correctly set. */
    @Test
    @DisabledTest(message = "Flaky test. See https://crbug.com/416758468")
    public void testContentDescriptionIsCorrect() {
        PropertyModel model = buildDefaultPropertyModel();
        DropdownFieldView field = attachDropdownFieldView(model);

        assertTrue(field.getDropdown().isImportantForAccessibility());
        assertFalse(field.getLabel().isImportantForAccessibility());

        assertEquals("label/value1", field.getDropdown().getContentDescription());
        model.set(VALUE, "value2");
        assertEquals("label/value2", field.getDropdown().getContentDescription());
    }

    /**
     * Test that the content description for dropdown elements is correctly set even if option list
     * is empty.
     */
    @Test
    @DisabledTest(message = "Flaky test. See https://crbug.com/416761636")
    public void testContentDescriptionIsCorrectDropdownListEmpty() {
        PropertyModel model = buildPropertyModelNoValues();
        DropdownFieldView field = attachDropdownFieldView(model);

        assertTrue(field.getDropdown().isImportantForAccessibility());
        assertFalse(field.getLabel().isImportantForAccessibility());
    }

    /** Test that the content description for dropdown elements is correctly set. */
    @Test
    @Features.EnableFeatures(ChromeFeatureList.ANDROID_SETTINGS_CONTAINMENT)
    public void testContentDescriptionIsCorrectWithContainmentOn() {
        PropertyModel model = buildDefaultPropertyModel();
        DropdownFieldView field = attachDropdownFieldView(model);

        assertTrue(field.getDropdown().isImportantForAccessibility());

        assertEquals("label/value1", field.getDropdown().getContentDescription());
        model.set(VALUE, "value2");
        assertEquals("label/value2", field.getDropdown().getContentDescription());
    }

    /**
     * Test that the content description for dropdown elements is correctly set even if option list
     * is empty.
     */
    @Test
    @Features.EnableFeatures(ChromeFeatureList.ANDROID_SETTINGS_CONTAINMENT)
    public void testContentDescriptionIsCorrectDropdownListEmptyWithContainmentOn() {
        PropertyModel model = buildPropertyModelNoValues();
        DropdownFieldView field = attachDropdownFieldView(model);

        assertTrue(field.getDropdown().isImportantForAccessibility());

        assertEquals("label", field.getDropdown().getContentDescription());
    }
}
