// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.common.date_field;

import static org.chromium.chrome.browser.autofill.editors.common.dropdown_field.DropdownFieldProperties.DROPDOWN_HINT;
import static org.chromium.chrome.browser.autofill.editors.common.dropdown_field.DropdownFieldProperties.DROPDOWN_KEY_VALUE_LIST;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.IS_REQUIRED;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.LABEL;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.VALUE;

import android.content.Context;
import android.text.TextUtils;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.autofill.editors.common.dropdown_field.DropdownFieldProperties;
import org.chromium.chrome.browser.autofill.editors.common.dropdown_field.DropdownFieldView;
import org.chromium.chrome.browser.autofill.editors.common.dropdown_field.DropdownFieldViewBinder;
import org.chromium.chrome.browser.autofill.editors.common.field.FieldView;
import org.chromium.components.autofill.DropdownKeyValue;
import org.chromium.components.autofill.autofill_ai.AttributeInstance;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.time.LocalDate;
import java.time.ZoneId;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

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
    private final TextView mErrorMessage;

    public DateFieldView(Context context, String value) {
        super(context);
        setOrientation(LinearLayout.VERTICAL);

        mLabel = new TextView(context, null, 0, R.style.TextAppearance_TextMediumThick_Primary);
        mLabel.setVisibility(View.GONE);
        mLabel.setId(R.id.date_field_label);
        mLabel.setTextAlignment(View.TEXT_ALIGNMENT_VIEW_START);

        mErrorMessage = new TextView(context, null, 0, R.style.TextAppearance_ErrorCaption);
        mErrorMessage.setVisibility(View.GONE);
        mErrorMessage.setId(R.id.date_field_error_message);
        mErrorMessage.setTextAlignment(View.TEXT_ALIGNMENT_VIEW_START);
        mErrorMessage.setPadding(
                0,
                context.getResources()
                        .getDimensionPixelSize(R.dimen.pref_autofill_field_top_margin),
                0,
                0);

        mSpinnerGroup = new LinearLayout(context);
        mSpinnerGroup.setOrientation(LinearLayout.HORIZONTAL);
        @Nullable LocalDate date = new AttributeInstance.DateValue(value).getDate();
        final String monthValue = date == null ? "" : String.valueOf(date.getMonthValue());
        PropertyModel monthModel =
                new PropertyModel.Builder(DropdownFieldProperties.DROPDOWN_ALL_KEYS)
                        .with(DROPDOWN_KEY_VALUE_LIST, getMonthDropdownValues())
                        .with(
                                DROPDOWN_HINT,
                                context.getString(
                                        R.string.autofill_ai_entity_editor_date_field_month_label))
                        .with(IS_REQUIRED, false)
                        .with(LABEL, "")
                        .with(VALUE, monthValue)
                        .build();
        final String dayValue = date == null ? "" : String.valueOf(date.getDayOfMonth());
        PropertyModel dayModel =
                new PropertyModel.Builder(DropdownFieldProperties.DROPDOWN_ALL_KEYS)
                        .with(DROPDOWN_KEY_VALUE_LIST, getDayDropdownValues())
                        .with(
                                DROPDOWN_HINT,
                                context.getString(
                                        R.string.autofill_ai_entity_editor_date_field_day_label))
                        .with(IS_REQUIRED, false)
                        .with(LABEL, "")
                        .with(VALUE, dayValue)
                        .build();
        // LocalDate always returns the day within the [1; 31] range and month within [1, 12]. The
        // year dropdown is a special case: it's values are always generated within the
        // [X - 90; X + 15] range where X is the current year. The entity attribute's date value can
        // exceed this range. In this case, the year value is used as a hint.
        String yearValue = "";
        String yearHint =
                context.getString(R.string.autofill_ai_entity_editor_date_field_year_label);
        if (date != null) {
            if (yearWithinLimits(date.getYear())) {
                yearValue = String.valueOf(date.getYear());
            } else {
                yearHint = String.valueOf(date.getYear());
            }
        }
        PropertyModel yearModel =
                new PropertyModel.Builder(DropdownFieldProperties.DROPDOWN_ALL_KEYS)
                        .with(DROPDOWN_KEY_VALUE_LIST, getYearDropdownValues())
                        .with(DROPDOWN_HINT, yearHint)
                        .with(IS_REQUIRED, false)
                        .with(LABEL, "")
                        .with(VALUE, yearValue)
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
        addView(mErrorMessage);

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

    public void setValue(String value) {
        // VALUE of the date fields is implemented as a String instead of the LocalDate to keep
        // it consistent with the VALUE property for other fields. That's why the parsing still
        // needs to happen.
        @Nullable LocalDate date = new AttributeInstance.DateValue(value).getDate();
        if (date == null) {
            mMonthDropdown.getFieldModel().set(VALUE, "");
            mDayDropdown.getFieldModel().set(VALUE, "");
            mYearDropdown.getFieldModel().set(VALUE, "");
        } else {
            mMonthDropdown.getFieldModel().set(VALUE, String.valueOf(date.getMonthValue()));
            mDayDropdown.getFieldModel().set(VALUE, String.valueOf(date.getDayOfMonth()));
            // Again, set the initial hint value if the year doesn't fall within the range. This
            // scenario can't happen in the `EntityEditor`: the date fields are initialized with the
            // initial attribute values. It's not possible to select a year which is outside the
            // range afterwards.
            String yearValue =
                    yearWithinLimits(date.getYear()) ? String.valueOf(date.getYear()) : "";
            mYearDropdown.getFieldModel().set(VALUE, yearValue);
        }
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

    private List<DropdownKeyValue> getMonthDropdownValues() {
        List<DropdownKeyValue> monthList = new ArrayList<>();
        for (int month = 1; month <= 12; month++) {
            monthList.add(
                    new DropdownKeyValue(String.valueOf(month), getMonthName(getContext(), month)));
        }
        return monthList;
    }

    @VisibleForTesting
    static final String getMonthName(Context context, int month) {
        if (month < 1 || month > 12) {
            return "";
        }
        Locale locale = context.getResources().getConfiguration().getLocales().get(0);
        return DateTimeFormatter.ofPattern("MMM", locale).format(getDateWithMonth(month));
    }

    public void setErrorMessage(@Nullable String errorMessage) {
        mErrorMessage.setVisibility(TextUtils.isEmpty(errorMessage) ? View.GONE : View.VISIBLE);
        mErrorMessage.setText(errorMessage);
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
        for (int year = getMaxYear(); year >= getMinYear(); year--) {
            yearList.add(new DropdownKeyValue(String.valueOf(year), String.valueOf(year)));
        }
        return yearList;
    }

    private static int getMinYear() {
        return getCurrentYear() - 90;
    }

    private static int getMaxYear() {
        return getCurrentYear() + 15;
    }

    private static boolean yearWithinLimits(int year) {
        return year >= getMinYear() && year <= getMaxYear();
    }

    private static int getCurrentYear() {
        return getCurrentDate().getYear();
    }

    private static LocalDate getDateWithMonth(int month) {
        return getCurrentDate().withMonth(month);
    }

    private static LocalDate getCurrentDate() {
        return LocalDate.now(ZoneId.systemDefault());
    }

    DropdownFieldView getMonthPickerForTest() {
        return mMonthDropdown;
    }

    DropdownFieldView getDayPickerForTest() {
        return mDayDropdown;
    }

    DropdownFieldView getYearPickerForTest() {
        return mYearDropdown;
    }
}
