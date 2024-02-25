// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.IS_REQUIRED;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.LABEL;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.VALIDATOR;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.VALUE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.TextFieldProperties.TEXT_ALL_KEYS;

import android.app.Activity;
import android.text.Editable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AutoCompleteTextView;
import android.widget.LinearLayout;

import com.google.android.material.textfield.TextInputEditText;
import com.google.android.material.textfield.TextInputLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.text.EmptyTextWatcher;

/** Unit test for {@link TextFieldView}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class TextFieldViewUnitTest {
    private Activity mActivity;
    private ViewGroup mContentView;
    private View mOtherFocusableField;

    @Before
    public void setUp() {
        mActivity = Robolectric.setupActivity(Activity.class);
        mContentView = new LinearLayout(mActivity);
        mOtherFocusableField = new TextInputEditText(mActivity);
        mContentView.addView(mOtherFocusableField);
    }

    private PropertyModel buildDefaultPropertyModel() {
        return new PropertyModel.Builder(TEXT_ALL_KEYS)
                .with(LABEL, "label")
                .with(VALUE, "value")
                .build();
    }

    private TextFieldView attachTextFieldView(PropertyModel model) {
        TextFieldView field = new TextFieldView(mActivity, model);
        mContentView.addView(field);
        PropertyModelChangeProcessor.create(
                model, field, EditorDialogViewBinder::bindTextFieldView);
        return field;
    }

    /**
     * Test that the text field does not show an error message if there are no validation issues.
     */
    @Test
    public void testNoError() {
        PropertyModel model = buildDefaultPropertyModel();
        model.set(IS_REQUIRED, true);
        model.set(
                VALIDATOR,
                EditorFieldValidator.builder()
                        .withRequiredErrorMessage("required error message")
                        .build());
        TextFieldView field = attachTextFieldView(model);

        assertTrue(field.validate());
        assertNull(field.getInputLayoutForTesting().getError());
    }

    /** Test the error message is hidden when the user edits the field. */
    @Test
    public void testEditHideErrorMessage() {
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
        TextFieldView field = attachTextFieldView(model);

        AutoCompleteTextView fieldEditText = field.getEditText();
        TextInputLayout inputLayout = field.getInputLayoutForTesting();

        // Set and check initial state
        assertFalse(field.validate());
        fieldEditText.requestFocus();
        assertTrue(fieldEditText.hasFocus());
        assertTrue(inputLayout.getError() != null);

        // Editing field should clear error message.
        fieldEditText.setText("edited");
        assertNull(inputLayout.getError());
    }

    /**
     * Test that the error message is shown again when the user focuses a different field after
     * editing it if the field is still invalid.
     */
    @Test
    public void testUnfocusFieldStillInvalid() {
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
        TextFieldView field = attachTextFieldView(model);

        AutoCompleteTextView fieldEditText = field.getEditText();
        TextInputLayout inputLayout = field.getInputLayoutForTesting();

        // Set and check initial state
        assertFalse(field.validate());
        fieldEditText.requestFocus();
        fieldEditText.setText("edited");
        assertTrue(fieldEditText.hasFocus());
        assertNull(inputLayout.getError());

        // Error should be shown again when field loses focus.
        mOtherFocusableField.requestFocus();
        assertFalse(fieldEditText.hasFocus());
        assertTrue(inputLayout.getError() != null);
    }

    /**
     * Test that the error message is hidden when a field loses focuses when a user corrects the
     * field's invalid value.
     */
    @Test
    public void testUnfocusFieldNowValid() {
        PropertyModel model = buildDefaultPropertyModel();
        model.set(
                VALIDATOR,
                EditorFieldValidator.builder()
                        .withValidationPredicate(
                                (value) -> {
                                    return "valid".equals(value);
                                },
                                "Error Message")
                        .build());
        TextFieldView field = attachTextFieldView(model);

        AutoCompleteTextView fieldEditText = field.getEditText();
        TextInputLayout inputLayout = field.getInputLayoutForTesting();

        // Set and check initial state
        assertFalse(field.validate());
        fieldEditText.requestFocus();
        assertTrue(fieldEditText.hasFocus());
        assertTrue(inputLayout.getError() != null);

        fieldEditText.setText("valid");
        mOtherFocusableField.requestFocus();
        assertFalse(fieldEditText.hasFocus());
        assertNull(inputLayout.getError());
    }

    /**
     * Test that the initial error message is not hidden if the user focuses the field but does not
     * edit it.
     */
    @Test
    public void testInitialErrorMessageValueNotEdited() {
        PropertyModel model = buildDefaultPropertyModel();
        model.set(
                VALIDATOR,
                EditorFieldValidator.builder()
                        .withInitialErrorMessage("Initial error message")
                        .build());
        TextFieldView field = attachTextFieldView(model);

        AutoCompleteTextView fieldEditText = field.getEditText();
        TextInputLayout inputLayout = field.getInputLayoutForTesting();

        // Set and check initial state
        assertFalse(field.validate());
        fieldEditText.requestFocus();
        assertTrue(fieldEditText.hasFocus());
        assertTrue(inputLayout.getError() != null);

        mOtherFocusableField.requestFocus();
        assertFalse(fieldEditText.hasFocus());
        assertTrue(inputLayout.getError() != null);
    }

    /**
     * Test that the initial error message is hidden when a field loses focus if the user edits the
     * value.
     */
    @Test
    public void testInitialErrorMessageValueEdited() {
        PropertyModel model = buildDefaultPropertyModel();
        model.set(
                VALIDATOR,
                EditorFieldValidator.builder()
                        .withInitialErrorMessage("Initial error message")
                        .build());
        TextFieldView field = attachTextFieldView(model);

        AutoCompleteTextView fieldEditText = field.getEditText();
        TextInputLayout inputLayout = field.getInputLayoutForTesting();

        // Set and check initial state
        assertFalse(field.validate());
        fieldEditText.requestFocus();
        assertTrue(fieldEditText.hasFocus());
        assertTrue(inputLayout.getError() != null);

        fieldEditText.setText("other");
        mOtherFocusableField.requestFocus();
        assertFalse(fieldEditText.hasFocus());
        assertNull(inputLayout.getError());
    }

    /**
     * Test that the initial error message is not hidden if a field's value is programmatically
     * changed via {@link TextFieldView#setValue()} while it has focus.
     */
    @Test
    public void testInitialErrorMessageSetValue() {
        PropertyModel model = buildDefaultPropertyModel();
        model.set(
                VALIDATOR,
                EditorFieldValidator.builder()
                        .withInitialErrorMessage("Initial error message")
                        .build());
        final TextFieldView field = attachTextFieldView(model);

        // Set text formatter in order to test nested calls of {@link TextFieldView#setValue(()}.
        field.setTextFormatter(
                new EmptyTextWatcher() {
                    @Override
                    public void afterTextChanged(Editable s) {
                        if (!s.toString().startsWith("formatted")) {
                            s.insert(0, "formatted");
                        }
                    }
                });

        AutoCompleteTextView fieldEditText = field.getEditText();
        TextInputLayout inputLayout = field.getInputLayoutForTesting();

        // Set and check initial state
        assertFalse(field.validate());
        fieldEditText.requestFocus();
        assertTrue(fieldEditText.hasFocus());
        assertTrue(inputLayout.getError() != null);

        field.setValue("other");
        mOtherFocusableField.requestFocus();
        assertFalse(fieldEditText.hasFocus());
        assertTrue(inputLayout.getError() != null);
    }

    /**
     * test that calling {@link #validate()} multiple times does not clear the initial error
     * message.
     */
    @Test
    public void testValidatingMultipleTimesInitialMessage() {
        PropertyModel model = buildDefaultPropertyModel();
        model.set(
                VALIDATOR,
                EditorFieldValidator.builder()
                        .withInitialErrorMessage("Initial error message")
                        .build());
        TextFieldView field = attachTextFieldView(model);

        TextInputLayout inputLayout = field.getInputLayoutForTesting();

        // Set and check initial state
        assertFalse(field.validate());
        assertTrue(inputLayout.getError() != null);

        // Calling {@link #validate()} multiple times should have no effect.
        assertFalse(field.validate());
        assertTrue(inputLayout.getError() != null);
    }

    /**
     * Test that calling {@link #validate()} shows the error message in the case that the error
     * message is hidden by the user editing the field.
     *
     * <p>This behavior is needed when validation is triggered by submitting the form via the
     * virtual keyboard.
     */
    @Test
    public void testValidateShowsErrorMessage() {
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
        TextFieldView field = attachTextFieldView(model);

        AutoCompleteTextView fieldEditText = field.getEditText();
        TextInputLayout inputLayout = field.getInputLayoutForTesting();

        // Set and check initial state
        assertFalse(field.validate());
        fieldEditText.requestFocus();
        assertTrue(fieldEditText.hasFocus());
        fieldEditText.setText("edited");
        assertNull(inputLayout.getError());

        assertFalse(field.validate());
        assertTrue(inputLayout.getError() != null);
    }

    /** Test that {@link #validate()} returns true when there is no validator. */
    @Test
    public void testNullValidator() {
        PropertyModel model = buildDefaultPropertyModel();
        TextFieldView field = attachTextFieldView(model);

        assertTrue(field.validate());
    }
}
