// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.common.date_field;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.app.Activity;
import android.view.View;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.autofill.editors.common.field.FieldView;

import java.time.LocalDate;

/** Unit test for {@link DateFieldView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DateFieldViewTest {
    private Activity mActivity;
    private DateFieldView mDateFieldView;

    @Before
    public void setUp() {
        mActivity = Robolectric.setupActivity(Activity.class);
    }

    @Test
    public void testSetLabel() {
        mDateFieldView = new DateFieldView(mActivity, /* value= */ "");
        mDateFieldView.setLabel("Test Label", /* isRequired= */ false);
        TextView labelView = mDateFieldView.findViewById(R.id.date_field_label);
        assertNotNull(labelView);
        assertEquals(View.VISIBLE, labelView.getVisibility());
        assertEquals("Test Label", labelView.getText());
    }

    @Test
    public void testSetLabel_Required() {
        mDateFieldView = new DateFieldView(mActivity, /* value= */ "");
        mDateFieldView.setLabel("Test Label", /* isRequired= */ true);
        TextView labelView = mDateFieldView.findViewById(R.id.date_field_label);
        assertNotNull(labelView);
        assertEquals(View.VISIBLE, labelView.getVisibility());
        assertEquals("Test Label" + FieldView.REQUIRED_FIELD_INDICATOR, labelView.getText());
    }

    @Test
    public void testDropdownsExistence() {
        mDateFieldView = new DateFieldView(mActivity, /* value= */ "");
        assertNotNull(mDateFieldView.findViewById(R.id.date_field_month_dropdown));
        assertNotNull(mDateFieldView.findViewById(R.id.date_field_day_dropdown));
        assertNotNull(mDateFieldView.findViewById(R.id.date_field_year_dropdown));
    }

    @Test
    public void testEmptyInitialValue() {
        mDateFieldView = new DateFieldView(mActivity, /* value= */ "");
        assertEquals(
                mActivity.getString(R.string.autofill_ai_entity_editor_date_field_month_label),
                mDateFieldView.getMonthPickerForTest().getDropdown().getSelectedItem());
        assertEquals(
                mActivity.getString(R.string.autofill_ai_entity_editor_date_field_day_label),
                mDateFieldView.getDayPickerForTest().getDropdown().getSelectedItem());
        assertEquals(
                mActivity.getString(R.string.autofill_ai_entity_editor_date_field_year_label),
                mDateFieldView.getYearPickerForTest().getDropdown().getSelectedItem());
    }

    @Test
    public void testNonEmptyInitialValue() {
        mDateFieldView =
                new DateFieldView(mActivity, /* value= */ LocalDate.of(2026, 2, 15).toString());
        assertEquals(
                DateFieldView.getMonthName(mActivity, /* month= */ 2),
                mDateFieldView.getMonthPickerForTest().getDropdown().getSelectedItem());
        assertEquals("15", mDateFieldView.getDayPickerForTest().getDropdown().getSelectedItem());
        assertEquals("2026", mDateFieldView.getYearPickerForTest().getDropdown().getSelectedItem());
    }

    @Test
    public void testInitialValueNotInRange() {
        mDateFieldView =
                new DateFieldView(mActivity, /* value= */ LocalDate.of(1800, 1, 1).toString());
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
    public void testSetValue() {
        mDateFieldView = new DateFieldView(mActivity, /* value= */ "");
        mDateFieldView.setValue(LocalDate.of(2026, 3, 16).toString());
        assertEquals(
                DateFieldView.getMonthName(mActivity, /* month= */ 3),
                mDateFieldView.getMonthPickerForTest().getDropdown().getSelectedItem());
        assertEquals("16", mDateFieldView.getDayPickerForTest().getDropdown().getSelectedItem());
        assertEquals("2026", mDateFieldView.getYearPickerForTest().getDropdown().getSelectedItem());
    }

    @Test
    public void testSetValueNotInRange() {
        mDateFieldView =
                new DateFieldView(mActivity, /* value= */ LocalDate.of(2026, 2, 15).toString());
        mDateFieldView.setValue(LocalDate.of(1810, 12, 12).toString());
        assertEquals(
                DateFieldView.getMonthName(mActivity, /* month= */ 12),
                mDateFieldView.getMonthPickerForTest().getDropdown().getSelectedItem());
        assertEquals("12", mDateFieldView.getDayPickerForTest().getDropdown().getSelectedItem());
        assertEquals(
                mActivity.getString(R.string.autofill_ai_entity_editor_date_field_year_label),
                mDateFieldView.getYearPickerForTest().getDropdown().getSelectedItem());
        // Make sure the hint is selected by checking the the selected item position is 0.
        assertEquals(
                0, mDateFieldView.getYearPickerForTest().getDropdown().getSelectedItemPosition());
    }

    @Test
    public void testInitialAndUpdatedValuesNotInRange() {
        mDateFieldView =
                new DateFieldView(mActivity, /* value= */ LocalDate.of(1800, 2, 15).toString());
        mDateFieldView.setValue(LocalDate.of(1810, 11, 11).toString());
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
    public void testSetErrorMessage() {
        mDateFieldView = new DateFieldView(mActivity, /* value= */ "2026-02-15");

        TextView errorMessage = mDateFieldView.findViewById(R.id.date_field_error_message);
        assertEquals(View.GONE, errorMessage.getVisibility());

        mDateFieldView.setErrorMessage("Test error message");
        assertEquals(View.VISIBLE, errorMessage.getVisibility());
        assertEquals("Test error message", errorMessage.getText());

        mDateFieldView.setErrorMessage(null);
        assertEquals(View.GONE, errorMessage.getVisibility());
    }
}
