// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.utils;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.greaterThan;

import android.widget.ArrayAdapter;

import org.chromium.chrome.browser.autofill.editors.common.date_field.DateFieldView;
import org.chromium.chrome.browser.autofill.editors.common.dropdown_field.DropdownFieldView;

public class TestUtils {

    // getAdapter() returns a raw android.widget.Adapter; narrow cast to the known concrete type.
    @SuppressWarnings("unchecked")
    public static void setDropdownValue(DropdownFieldView dropdown, String value) {
        ArrayAdapter<String> adapter = (ArrayAdapter<String>) dropdown.getDropdown().getAdapter();
        int index = adapter.getPosition(value);
        assertThat(index, greaterThan(-1));
        dropdown.getDropdown().setSelection(index);
    }

    public static void setDropdownDate(DateFieldView dateField, int month, int day, int year) {
        setDropdownValue(
                dateField.getMonthPickerForTest(),
                DateFieldView.getMonthName(dateField.getContext(), /* month= */ month));
        setDropdownValue(dateField.getDayPickerForTest(), String.valueOf(day));
        setDropdownValue(dateField.getYearPickerForTest(), String.valueOf(year));
    }
}
