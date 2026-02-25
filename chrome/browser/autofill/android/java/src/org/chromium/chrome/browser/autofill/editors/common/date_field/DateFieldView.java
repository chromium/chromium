// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.common.date_field;

import static org.chromium.chrome.browser.autofill.editors.common.dropdown_field.DropdownFieldProperties.DROPDOWN_HINT;
import static org.chromium.chrome.browser.autofill.editors.common.dropdown_field.DropdownFieldProperties.DROPDOWN_KEY_VALUE_LIST;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.IS_REQUIRED;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.LABEL;

import android.content.Context;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.autofill.editors.common.dropdown_field.DropdownFieldProperties;
import org.chromium.chrome.browser.autofill.editors.common.dropdown_field.DropdownFieldView;
import org.chromium.chrome.browser.autofill.editors.common.dropdown_field.DropdownFieldViewBinder;
import org.chromium.chrome.browser.autofill.editors.common.field.FieldView;
import org.chromium.components.autofill.DropdownKeyValue;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.time.LocalDate;
import java.time.ZoneId;
import java.util.ArrayList;
import java.util.List;

/**
 * A view that combines three dropdowns for date selection (month, day, year).
 *
 * <p>TODO: crbug.com/487565242 - Consider using ModalDatePicker instead.
 */
@NullMarked
public class DateFieldView extends LinearLayout implements FieldView {
    private final LinearLayout mSpinnerGroup;
    private final TextView mLabel;
    private final DropdownFieldView mMonthDropdown;
    private final DropdownFieldView mDayDropdown;
    private final DropdownFieldView mYearDropdown;

    public DateFieldView(Context context) {
        super(context);
        setOrientation(LinearLayout.VERTICAL);

        mLabel = new TextView(context, null, 0, R.style.TextAppearance_TextMediumThick_Primary);
        mLabel.setVisibility(View.GONE);
        mLabel.setId(R.id.date_field_label);

        mSpinnerGroup = new LinearLayout(context);
        mSpinnerGroup.setOrientation(LinearLayout.HORIZONTAL);
        PropertyModel monthModel =
                new PropertyModel.Builder(DropdownFieldProperties.DROPDOWN_ALL_KEYS)
                        .with(DROPDOWN_KEY_VALUE_LIST, getMonthDropdownValues())
                        .with(DROPDOWN_HINT, "Month")
                        .with(IS_REQUIRED, false)
                        .with(LABEL, "")
                        .build();
        PropertyModel dayModel =
                new PropertyModel.Builder(DropdownFieldProperties.DROPDOWN_ALL_KEYS)
                        .with(DROPDOWN_KEY_VALUE_LIST, getDayDropdownValues())
                        .with(DROPDOWN_HINT, "Day")
                        .with(IS_REQUIRED, false)
                        .with(LABEL, "")
                        .build();
        PropertyModel yearModel =
                new PropertyModel.Builder(DropdownFieldProperties.DROPDOWN_ALL_KEYS)
                        .with(DROPDOWN_KEY_VALUE_LIST, getYearDropdownValues())
                        .with(DROPDOWN_HINT, "Year")
                        .with(IS_REQUIRED, false)
                        .with(LABEL, "")
                        .build();

        mMonthDropdown = new DropdownFieldView(context, this, monthModel);
        mMonthDropdown.getLayout().setId(R.id.date_field_month_dropdown);
        mDayDropdown = new DropdownFieldView(context, this, dayModel);
        mDayDropdown.getLayout().setId(R.id.date_field_day_dropdown);
        mYearDropdown = new DropdownFieldView(context, this, yearModel);
        mYearDropdown.getLayout().setId(R.id.date_field_year_dropdown);

        PropertyModelChangeProcessor.create(
                monthModel, mMonthDropdown, DropdownFieldViewBinder::bindDropdownFieldView);
        PropertyModelChangeProcessor.create(
                dayModel, mDayDropdown, DropdownFieldViewBinder::bindDropdownFieldView);
        PropertyModelChangeProcessor.create(
                yearModel, mYearDropdown, DropdownFieldViewBinder::bindDropdownFieldView);

        mMonthDropdown
                .getLayout()
                .setPaddingRelative(
                        /* start= */ 0,
                        /* top= */ 0,
                        /* end= */ getResources()
                                .getDimensionPixelSize(
                                        R.dimen.autofill_editor_date_field_element_padding),
                        /* bottom= */ 0);
        mYearDropdown
                .getLayout()
                .setPaddingRelative(
                        /* start= */ getResources()
                                .getDimensionPixelSize(
                                        R.dimen.autofill_editor_date_field_element_padding),
                        /* top= */ 0,
                        /* end= */ 0,
                        /* bottom= */ 0);

        mSpinnerGroup.addView(
                mMonthDropdown.getLayout(),
                new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 3.0f));
        mSpinnerGroup.addView(
                mDayDropdown.getLayout(),
                new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 2.0f));
        mSpinnerGroup.addView(
                mYearDropdown.getLayout(),
                new LinearLayout.LayoutParams(0, LayoutParams.WRAP_CONTENT, 3.0f));

        addView(mLabel);
        addView(mSpinnerGroup);

        setPaddingRelative(
                /* start= */ 0,
                /* top= */ getResources()
                        .getDimensionPixelSize(R.dimen.editor_dialog_section_large_spacing),
                /* end= */ 0,
                /* bottom= */ getResources()
                        .getDimensionPixelSize(R.dimen.editor_dialog_section_large_spacing));
    }

    public void setLabel(String label, boolean isRequired) {
        mLabel.setVisibility(View.VISIBLE);
        mLabel.setText(isRequired ? label + FieldView.REQUIRED_FIELD_INDICATOR : label);
    }

    @Override
    public void scrollToAndFocus() {
        // TODO: crbug.com/467563819 - Implement.
    }

    @Override
    public boolean isRequired() {
        return false;
    }

    @Override
    public boolean validate() {
        return true;
    }

    private static List<DropdownKeyValue> getMonthDropdownValues() {
        // TODO: crbug.com/476755159 - Add internationalization.
        return List.of(
                new DropdownKeyValue("1", "Jan"),
                new DropdownKeyValue("2", "Feb"),
                new DropdownKeyValue("3", "Mar"),
                new DropdownKeyValue("4", "Apr"),
                new DropdownKeyValue("5", "May"),
                new DropdownKeyValue("6", "Jun"),
                new DropdownKeyValue("7", "Jul"),
                new DropdownKeyValue("8", "Aug"),
                new DropdownKeyValue("9", "Sep"),
                new DropdownKeyValue("10", "Oct"),
                new DropdownKeyValue("11", "Nov"),
                new DropdownKeyValue("12", "Dec"));
    }

    private static List<DropdownKeyValue> getDayDropdownValues() {
        List<DropdownKeyValue> dayList = new ArrayList();
        for (int day = 1; day < 31; day++) {
            dayList.add(new DropdownKeyValue(String.valueOf(day), String.valueOf(day)));
        }
        return dayList;
    }

    private static List<DropdownKeyValue> getYearDropdownValues() {
        List<DropdownKeyValue> yearList = new ArrayList<>();
        final int currentYear = getCurrentYear();
        for (int year = currentYear + 15; year > currentYear - 91; year--) {
            yearList.add(new DropdownKeyValue(String.valueOf(year), String.valueOf(year)));
        }
        return yearList;
    }

    private static int getCurrentYear() {
        return LocalDate.now(ZoneId.systemDefault()).getYear();
    }
}
