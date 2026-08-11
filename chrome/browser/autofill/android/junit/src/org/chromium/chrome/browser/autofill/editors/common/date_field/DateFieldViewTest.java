// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.common.date_field;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.isEmptyString;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.chrome.browser.autofill.editors.common.date_field.DateFieldProperties.DATE_ALL_KEYS;
import static org.chromium.chrome.browser.autofill.editors.common.date_field.DateFieldProperties.DATE_VALID;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.ERROR_MESSAGE;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.IS_REQUIRED;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.LABEL;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.VALUE;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.VALUE_CHANGED_CALLBACK;
import static org.chromium.chrome.browser.autofill.editors.utils.TestUtils.setDropdownDate;
import static org.chromium.chrome.browser.autofill.editors.utils.TestUtils.setDropdownValue;

import android.text.InputType;
import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import com.google.android.material.textfield.TextInputLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.autofill.editors.common.field.FieldView;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;

import java.time.LocalDate;

/** Unit test for {@link DateFieldView}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.AUTOFILL_AI_USE_MATERIAL_DATE_PICKER_IN_ENTITY_EDITOR)
public class DateFieldViewTest {
    private TestActivity mActivity;
    private DateFieldView mDateFieldView;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Callback<String> mValueChangedCallback;

    @Before
    public void setUp() {
        mActivity = Robolectric.setupActivity(TestActivity.class);
    }

    private PropertyModel getDateFieldModel(@Nullable LocalDate date) {
        return new PropertyModel.Builder(DATE_ALL_KEYS)
                .with(IS_REQUIRED, false)
                .with(LABEL, "Date field label")
                .with(VALUE, date == null ? "" : date.toString())
                .with(VALUE_CHANGED_CALLBACK, mValueChangedCallback)
                .build();
    }

    private String getMonthLabel() {
        return mActivity.getString(R.string.autofill_ai_entity_editor_date_field_month_label);
    }

    private String getDayLabel() {
        return mActivity.getString(R.string.autofill_ai_entity_editor_date_field_day_label);
    }

    private String getYearLabel() {
        return mActivity.getString(R.string.autofill_ai_entity_editor_date_field_year_label);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_USE_MATERIAL_DATE_PICKER_IN_ENTITY_EDITOR)
    public void testSetLabel() {
        mDateFieldView =
                new DateFieldView(
                        mActivity, mActivity.getSupportFragmentManager(), getDateFieldModel(null));
        mDateFieldView.setLabel("Test Label", /* isRequired= */ false);
        TextView labelView = mDateFieldView.findViewById(R.id.date_field_label);
        assertNotNull(labelView);
        assertEquals(View.VISIBLE, labelView.getVisibility());
        assertEquals("Test Label", labelView.getText());
    }

    @Test
    public void testNotRequiredField() {
        PropertyModel model =
                new PropertyModel.Builder(DATE_ALL_KEYS)
                        .with(IS_REQUIRED, false)
                        .with(LABEL, "Date field label")
                        .with(VALUE, "")
                        .build();
        mDateFieldView = new DateFieldView(mActivity, mActivity.getSupportFragmentManager(), model);
        assertFalse(mDateFieldView.isRequired());
    }

    @Test
    public void testRequiredField() {
        PropertyModel model =
                new PropertyModel.Builder(DATE_ALL_KEYS)
                        .with(IS_REQUIRED, true)
                        .with(LABEL, "Date field label")
                        .with(VALUE, "")
                        .build();
        mDateFieldView = new DateFieldView(mActivity, mActivity.getSupportFragmentManager(), model);
        assertTrue(mDateFieldView.isRequired());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_USE_MATERIAL_DATE_PICKER_IN_ENTITY_EDITOR)
    public void testSetLabel_Required() {
        mDateFieldView =
                new DateFieldView(
                        mActivity, mActivity.getSupportFragmentManager(), getDateFieldModel(null));
        mDateFieldView.setLabel("Test Label", /* isRequired= */ true);
        TextView labelView = mDateFieldView.findViewById(R.id.date_field_label);
        assertNotNull(labelView);
        assertEquals(View.VISIBLE, labelView.getVisibility());
        assertEquals("Test Label" + FieldView.REQUIRED_FIELD_INDICATOR, labelView.getText());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_USE_MATERIAL_DATE_PICKER_IN_ENTITY_EDITOR)
    public void testDropdownsExistence() {
        mDateFieldView =
                new DateFieldView(
                        mActivity, mActivity.getSupportFragmentManager(), getDateFieldModel(null));
        assertNotNull(mDateFieldView.findViewById(R.id.date_field_month_dropdown));
        assertNotNull(mDateFieldView.findViewById(R.id.date_field_day_dropdown));
        assertNotNull(mDateFieldView.findViewById(R.id.date_field_year_dropdown));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_USE_MATERIAL_DATE_PICKER_IN_ENTITY_EDITOR)
    public void testEmptyInitialValue() {
        mDateFieldView =
                new DateFieldView(
                        mActivity, mActivity.getSupportFragmentManager(), getDateFieldModel(null));
        assertTrue(mDateFieldView.getFieldModel().get(DATE_VALID));
        assertEquals(
                getMonthLabel(),
                mDateFieldView.getMonthPickerForTest().getDropdown().getSelectedItem());
        assertEquals(
                getMonthLabel(),
                mDateFieldView.getMonthPickerForTest().getDropdown().getContentDescription());
        assertEquals(
                getDayLabel(),
                mDateFieldView.getDayPickerForTest().getDropdown().getSelectedItem());
        assertEquals(
                getDayLabel(),
                mDateFieldView.getDayPickerForTest().getDropdown().getContentDescription());
        assertEquals(
                getYearLabel(),
                mDateFieldView.getYearPickerForTest().getDropdown().getSelectedItem());
        assertEquals(
                getYearLabel(),
                mDateFieldView.getYearPickerForTest().getDropdown().getContentDescription());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_USE_MATERIAL_DATE_PICKER_IN_ENTITY_EDITOR)
    public void testNonEmptyInitialValue() {
        mDateFieldView =
                new DateFieldView(
                        mActivity,
                        mActivity.getSupportFragmentManager(),
                        getDateFieldModel(LocalDate.of(2026, 2, 15)));
        assertTrue(mDateFieldView.getFieldModel().get(DATE_VALID));
        assertEquals(
                DateFieldView.getMonthName(mActivity, /* month= */ 2),
                mDateFieldView.getMonthPickerForTest().getDropdown().getSelectedItem());
        assertEquals(
                getMonthLabel() + " Feb",
                mDateFieldView.getMonthPickerForTest().getDropdown().getContentDescription());
        assertEquals("15", mDateFieldView.getDayPickerForTest().getDropdown().getSelectedItem());
        assertEquals(
                getDayLabel() + " 15",
                mDateFieldView.getDayPickerForTest().getDropdown().getContentDescription());
        assertEquals("2026", mDateFieldView.getYearPickerForTest().getDropdown().getSelectedItem());
        assertEquals(
                getYearLabel() + " 2026",
                mDateFieldView.getYearPickerForTest().getDropdown().getContentDescription());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_USE_MATERIAL_DATE_PICKER_IN_ENTITY_EDITOR)
    public void testInitialValueNotInRange() {
        mDateFieldView =
                new DateFieldView(
                        mActivity,
                        mActivity.getSupportFragmentManager(),
                        getDateFieldModel(LocalDate.of(1800, 1, 1)));
        assertTrue(mDateFieldView.getFieldModel().get(DATE_VALID));
        assertEquals(
                DateFieldView.getMonthName(mActivity, /* month= */ 1),
                mDateFieldView.getMonthPickerForTest().getDropdown().getSelectedItem());
        assertEquals("1", mDateFieldView.getDayPickerForTest().getDropdown().getSelectedItem());
        assertEquals("1800", mDateFieldView.getYearPickerForTest().getDropdown().getSelectedItem());
        // Make sure the hint is selected by checking the the selected item position is 0.
        assertEquals(
                0, mDateFieldView.getYearPickerForTest().getDropdown().getSelectedItemPosition());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_USE_MATERIAL_DATE_PICKER_IN_ENTITY_EDITOR)
    public void testSetValue() {
        mDateFieldView =
                new DateFieldView(
                        mActivity, mActivity.getSupportFragmentManager(), getDateFieldModel(null));
        assertTrue(mDateFieldView.getFieldModel().get(DATE_VALID));

        mDateFieldView.setValue(LocalDate.of(2026, 3, 16).toString());
        assertTrue(mDateFieldView.getFieldModel().get(DATE_VALID));
        assertEquals(
                DateFieldView.getMonthName(mActivity, /* month= */ 3),
                mDateFieldView.getMonthPickerForTest().getDropdown().getSelectedItem());
        assertEquals(
                getMonthLabel() + " " + DateFieldView.getMonthName(mActivity, /* month= */ 3),
                mDateFieldView.getMonthPickerForTest().getDropdown().getContentDescription());
        assertEquals("16", mDateFieldView.getDayPickerForTest().getDropdown().getSelectedItem());
        assertEquals(
                getDayLabel() + " 16",
                mDateFieldView.getDayPickerForTest().getDropdown().getContentDescription());
        assertEquals("2026", mDateFieldView.getYearPickerForTest().getDropdown().getSelectedItem());
        assertEquals(
                getYearLabel() + " 2026",
                mDateFieldView.getYearPickerForTest().getDropdown().getContentDescription());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_USE_MATERIAL_DATE_PICKER_IN_ENTITY_EDITOR)
    public void testSetValueNotInRange() {
        mDateFieldView =
                new DateFieldView(
                        mActivity,
                        mActivity.getSupportFragmentManager(),
                        getDateFieldModel(LocalDate.of(2026, 2, 15)));
        assertTrue(mDateFieldView.getFieldModel().get(DATE_VALID));

        mDateFieldView.setValue(LocalDate.of(1810, 12, 12).toString());
        // The date is invalid because 1810 is not within the range and it wasn't the initial year.
        assertFalse(mDateFieldView.getFieldModel().get(DATE_VALID));
        assertEquals(
                DateFieldView.getMonthName(mActivity, /* month= */ 12),
                mDateFieldView.getMonthPickerForTest().getDropdown().getSelectedItem());
        assertEquals("12", mDateFieldView.getDayPickerForTest().getDropdown().getSelectedItem());
        assertEquals(
                getYearLabel(),
                mDateFieldView.getYearPickerForTest().getDropdown().getSelectedItem());
        // Make sure the hint is selected by checking the the selected item position is 0.
        assertEquals(
                0, mDateFieldView.getYearPickerForTest().getDropdown().getSelectedItemPosition());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_USE_MATERIAL_DATE_PICKER_IN_ENTITY_EDITOR)
    public void testInitialAndUpdatedValuesNotInRange() {
        mDateFieldView =
                new DateFieldView(
                        mActivity,
                        mActivity.getSupportFragmentManager(),
                        getDateFieldModel(LocalDate.of(1800, 2, 15)));
        assertTrue(mDateFieldView.getFieldModel().get(DATE_VALID));

        mDateFieldView.setValue(LocalDate.of(1810, 11, 11).toString());
        // The date is invalid because 1810 is not within the range and it wasn't the initial year.
        // The DateField selects the hint in the year dropdown, which is 1800 in this case cause the
        // initial date was not in the range.
        assertTrue(mDateFieldView.getFieldModel().get(DATE_VALID));
        assertEquals(
                DateFieldView.getMonthName(mActivity, /* month= */ 11),
                mDateFieldView.getMonthPickerForTest().getDropdown().getSelectedItem());
        assertEquals("11", mDateFieldView.getDayPickerForTest().getDropdown().getSelectedItem());
        // The initial "1800" year should be selected because it's a hint which is used whenever the
        // year is not within the range.
        assertEquals("1800", mDateFieldView.getYearPickerForTest().getDropdown().getSelectedItem());
        // Make sure the hint is selected by checking the the selected item position is 0.
        assertEquals(
                0, mDateFieldView.getYearPickerForTest().getDropdown().getSelectedItemPosition());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_USE_MATERIAL_DATE_PICKER_IN_ENTITY_EDITOR)
    public void testSetErrorMessage() {
        mDateFieldView =
                new DateFieldView(
                        mActivity,
                        mActivity.getSupportFragmentManager(),
                        getDateFieldModel(LocalDate.of(2026, 2, 15)));
        assertTrue(mDateFieldView.getFieldModel().get(DATE_VALID));

        TextView errorMessage = mDateFieldView.findViewById(R.id.date_field_error_message);
        assertEquals(View.GONE, errorMessage.getVisibility());

        mDateFieldView.setErrorMessage("Test error message");
        assertEquals(View.VISIBLE, errorMessage.getVisibility());
        assertEquals("Test error message", errorMessage.getText());

        mDateFieldView.setErrorMessage(null);
        assertEquals(View.GONE, errorMessage.getVisibility());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_USE_MATERIAL_DATE_PICKER_IN_ENTITY_EDITOR)
    public void testSetValidDate() {
        mDateFieldView =
                new DateFieldView(
                        mActivity, mActivity.getSupportFragmentManager(), getDateFieldModel(null));
        assertTrue(mDateFieldView.getFieldModel().get(DATE_VALID));

        LocalDate currentDate = LocalDate.of(2026, 8, 31);
        assertTrue(mDateFieldView.getFieldModel().get(DATE_VALID));
        setDropdownValue(
                mDateFieldView.getMonthPickerForTest(),
                DateFieldView.getMonthName(mActivity, currentDate.getMonthValue()));
        // Only the month value is selected, the overall date is not complete yet.
        assertThat(mDateFieldView.getFieldModel().get(VALUE), isEmptyString());
        // The callback must still be called.
        verify(mValueChangedCallback, times(1)).onResult("");

        setDropdownValue(
                mDateFieldView.getDayPickerForTest(), String.valueOf(currentDate.getDayOfMonth()));
        // Only the moth and day values are selected, the overall date is not complete yet.
        assertThat(mDateFieldView.getFieldModel().get(VALUE), isEmptyString());
        // The callback must still be called a second time.
        verify(mValueChangedCallback, times(2)).onResult("");

        setDropdownValue(
                mDateFieldView.getYearPickerForTest(), String.valueOf(currentDate.getYear()));
        assertEquals(currentDate.toString(), mDateFieldView.getFieldModel().get(VALUE));
        verify(mValueChangedCallback, times(1)).onResult(currentDate.toString());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_USE_MATERIAL_DATE_PICKER_IN_ENTITY_EDITOR)
    public void testSetInvalidDate() {
        mDateFieldView =
                new DateFieldView(
                        mActivity, mActivity.getSupportFragmentManager(), getDateFieldModel(null));
        assertTrue(mDateFieldView.getFieldModel().get(DATE_VALID));

        // Set the date to February 31st, which is always invalid.
        setDropdownValue(
                mDateFieldView.getMonthPickerForTest(),
                DateFieldView.getMonthName(mActivity, /* month= */ 2));
        // Only the month value is selected, the overall date is not complete yet.
        assertFalse(mDateFieldView.getFieldModel().get(DATE_VALID));
        assertThat(mDateFieldView.getFieldModel().get(VALUE), isEmptyString());

        setDropdownValue(mDateFieldView.getDayPickerForTest(), "31");
        // Only the moth and day values are selected, the overall date is not complete yet.
        assertFalse(mDateFieldView.getFieldModel().get(DATE_VALID));
        assertThat(mDateFieldView.getFieldModel().get(VALUE), isEmptyString());

        // Make sure that invalid dates are not propagated to the model. They should be handled by
        // the data validation logic.
        setDropdownValue(mDateFieldView.getYearPickerForTest(), "2026");
        assertFalse(mDateFieldView.getFieldModel().get(DATE_VALID));
        assertThat(mDateFieldView.getFieldModel().get(VALUE), isEmptyString());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_USE_MATERIAL_DATE_PICKER_IN_ENTITY_EDITOR)
    public void testChangeValidToInvalidDate() {
        LocalDate currentDate = LocalDate.of(2026, 8, 31);
        mDateFieldView =
                new DateFieldView(
                        mActivity,
                        mActivity.getSupportFragmentManager(),
                        getDateFieldModel(currentDate));
        assertTrue(mDateFieldView.getFieldModel().get(DATE_VALID));
        assertEquals(currentDate.toString(), mDateFieldView.getFieldModel().get(VALUE));

        // Set the date to February 31st, which is always invalid.
        setDropdownDate(
                mDateFieldView, /* month= */ 2, /* day= */ 31, /* year= */ currentDate.getYear());
        // Make sure that the date is not propagated to the model because it's invalid. The old
        // value should be preserved.
        assertFalse(mDateFieldView.getFieldModel().get(DATE_VALID));
        assertEquals(currentDate.toString(), mDateFieldView.getFieldModel().get(VALUE));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_USE_MATERIAL_DATE_PICKER_IN_ENTITY_EDITOR)
    public void testChangeInvalidToValidDate() {
        mDateFieldView =
                new DateFieldView(
                        mActivity, mActivity.getSupportFragmentManager(), getDateFieldModel(null));
        assertTrue(mDateFieldView.getFieldModel().get(DATE_VALID));

        // Set the date to February 31st, which is always invalid.
        setDropdownDate(mDateFieldView, /* month= */ 2, /* day= */ 31, /* year= */ 2026);
        // Make sure that the date is not propagated to the model because it's invalid.
        assertFalse(mDateFieldView.getFieldModel().get(DATE_VALID));
        assertThat(mDateFieldView.getFieldModel().get(VALUE), isEmptyString());

        LocalDate currentDate = LocalDate.of(2026, 8, 31);
        // Set a valid date in the dropdown.
        setDropdownDate(
                mDateFieldView,
                currentDate.getMonthValue(),
                currentDate.getDayOfMonth(),
                currentDate.getYear());
        // Make sure that the date is propagated to the model.
        assertTrue(mDateFieldView.getFieldModel().get(DATE_VALID));
        assertEquals(currentDate.toString(), mDateFieldView.getFieldModel().get(VALUE));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_USE_MATERIAL_DATE_PICKER_IN_ENTITY_EDITOR)
    public void testResetToEmptyDate() {
        LocalDate currentDate = LocalDate.of(2026, 8, 31);
        mDateFieldView =
                new DateFieldView(
                        mActivity,
                        mActivity.getSupportFragmentManager(),
                        getDateFieldModel(currentDate));
        assertTrue(mDateFieldView.getFieldModel().get(DATE_VALID));
        assertEquals(currentDate.toString(), mDateFieldView.getFieldModel().get(VALUE));

        setDropdownValue(
                mDateFieldView.getMonthPickerForTest(),
                DateFieldView.getMonthDropdownHint(mActivity));
        // Only the month is reset, the change should not be propagated to the model.
        assertFalse(mDateFieldView.getFieldModel().get(DATE_VALID));
        assertEquals(currentDate.toString(), mDateFieldView.getFieldModel().get(VALUE));

        setDropdownValue(
                mDateFieldView.getDayPickerForTest(), DateFieldView.getDayDropdownHint(mActivity));
        // Only the month and the day are reset, the change should not be propagated to the model.
        assertFalse(mDateFieldView.getFieldModel().get(DATE_VALID));
        assertEquals(currentDate.toString(), mDateFieldView.getFieldModel().get(VALUE));

        setDropdownValue(
                mDateFieldView.getYearPickerForTest(),
                DateFieldView.getYearDropdownHint(mActivity));
        // Make sure that a completely reset date is propagated to the model.
        assertTrue(mDateFieldView.getFieldModel().get(DATE_VALID));
        assertThat(mDateFieldView.getFieldModel().get(VALUE), isEmptyString());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_USE_MATERIAL_DATE_PICKER_IN_ENTITY_EDITOR)
    public void testResetsTheErrorMessageAfterSelection() {
        LocalDate currentDate = LocalDate.of(2026, 8, 31);
        mDateFieldView =
                new DateFieldView(
                        mActivity,
                        mActivity.getSupportFragmentManager(),
                        getDateFieldModel(currentDate));
        assertTrue(mDateFieldView.getFieldModel().get(DATE_VALID));

        // Set the hint and check that date is invalid.
        setDropdownValue(
                mDateFieldView.getMonthPickerForTest(),
                DateFieldView.getMonthDropdownHint(mActivity));
        assertFalse(mDateFieldView.getFieldModel().get(DATE_VALID));

        // The error message should be reset.
        setDropdownValue(
                mDateFieldView.getMonthPickerForTest(),
                DateFieldView.getMonthName(mActivity, currentDate.getMonthValue()));
        assertTrue(TextUtils.isEmpty(mDateFieldView.getFieldModel().get(ERROR_MESSAGE)));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_USE_MATERIAL_DATE_PICKER_IN_ENTITY_EDITOR)
    public void testDateFieldValidator() {
        mDateFieldView =
                new DateFieldView(
                        mActivity, mActivity.getSupportFragmentManager(), getDateFieldModel(null));
        mDateFieldView.setValidator(new DateFieldValidator("Error message"));
        assertTrue(mDateFieldView.validate());
        assertTrue(TextUtils.isEmpty(mDateFieldView.getFieldModel().get(ERROR_MESSAGE)));

        // Set only the month -> validation should fail.
        setDropdownValue(
                mDateFieldView.getMonthPickerForTest(),
                DateFieldView.getMonthName(mActivity, /* month= */ 2));
        // The error message must be set only after the validation fails.
        assertTrue(TextUtils.isEmpty(mDateFieldView.getFieldModel().get(ERROR_MESSAGE)));
        assertFalse(mDateFieldView.validate());
        assertEquals("Error message", mDateFieldView.getFieldModel().get(ERROR_MESSAGE));

        // Set only the month and the day -> validation should fail.
        setDropdownValue(mDateFieldView.getDayPickerForTest(), "20");
        // The error message must be reset after the user interaction.
        assertTrue(TextUtils.isEmpty(mDateFieldView.getFieldModel().get(ERROR_MESSAGE)));
        assertFalse(mDateFieldView.validate());
        assertEquals("Error message", mDateFieldView.getFieldModel().get(ERROR_MESSAGE));

        // Set the whole date -> validation should succeed.
        setDropdownValue(mDateFieldView.getYearPickerForTest(), "2026");
        // The error message must be reset after the user interaction.
        assertTrue(TextUtils.isEmpty(mDateFieldView.getFieldModel().get(ERROR_MESSAGE)));
        // The date is valid now.
        assertTrue(mDateFieldView.validate());
        assertTrue(TextUtils.isEmpty(mDateFieldView.getFieldModel().get(ERROR_MESSAGE)));

        // Set an invalid 02/31/2026 date -> validation should fail.
        setDropdownValue(mDateFieldView.getDayPickerForTest(), "31");
        // The error message must be set only after the validation fails.
        assertTrue(TextUtils.isEmpty(mDateFieldView.getFieldModel().get(ERROR_MESSAGE)));
        assertFalse(mDateFieldView.validate());
        assertEquals("Error message", mDateFieldView.getFieldModel().get(ERROR_MESSAGE));
    }

    @Test
    public void testMaterialDatePickerVisibleWhenFeatureEnabled() {
        mDateFieldView =
                new DateFieldView(
                        mActivity, mActivity.getSupportFragmentManager(), getDateFieldModel(null));

        assertNull(mDateFieldView.findViewById(R.id.date_field_label));
        assertNull(mDateFieldView.getDayPickerForTest());
        assertNull(mDateFieldView.getMonthPickerForTest());
        assertNull(mDateFieldView.getYearPickerForTest());
        assertNull(mDateFieldView.findViewById(R.id.date_field_error_message));

        TextInputLayout layout = mDateFieldView.findViewById(R.id.text_input_layout);
        assertNotNull(layout);
        // Make sure the correct icon is displayed at the end.
        assertEquals(TextInputLayout.END_ICON_CUSTOM, layout.getEndIconMode());
        assertNotNull(layout.getEndIconDrawable());
        assertEquals(
                R.drawable.calendar_month_24dp,
                shadowOf(layout.getEndIconDrawable()).getCreatedFromResId());

        TextView textView = mDateFieldView.findViewById(R.id.text_view);
        assertTrue(textView.isFocusable());
        assertFalse(textView.isFocusableInTouchMode());
        assertEquals(InputType.TYPE_NULL, textView.getInputType());
        assertTrue(textView.isClickable());
    }

    @Test
    public void testMaterialDatePickerOpensOnClick() {
        mDateFieldView =
                new DateFieldView(
                        mActivity, mActivity.getSupportFragmentManager(), getDateFieldModel(null));

        TextView textView = mDateFieldView.findViewById(R.id.text_view);
        textView.performClick();

        // Make sure the MaterialDatePicker transaction is executed.
        mActivity.getSupportFragmentManager().executePendingTransactions();

        assertNotNull(
                mActivity.getSupportFragmentManager().findFragmentByTag("MaterialDatePicker"));
    }
}
