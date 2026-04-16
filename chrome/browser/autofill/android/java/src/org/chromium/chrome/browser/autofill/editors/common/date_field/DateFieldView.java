// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.common.date_field;

import static org.chromium.chrome.browser.autofill.editors.common.date_field.DateFieldProperties.DATE_VALID;
import static org.chromium.chrome.browser.autofill.editors.common.dropdown_field.DropdownFieldProperties.DROPDOWN_CALLBACK;
import static org.chromium.chrome.browser.autofill.editors.common.dropdown_field.DropdownFieldProperties.DROPDOWN_HINT;
import static org.chromium.chrome.browser.autofill.editors.common.dropdown_field.DropdownFieldProperties.DROPDOWN_KEY_VALUE_LIST;
import static org.chromium.chrome.browser.autofill.editors.common.field.FieldProperties.ERROR_MESSAGE;
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
import org.chromium.chrome.browser.autofill.editors.common.field.EditorFieldValidator;
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
    private final PropertyModel mPropertyModel;
    private final LinearLayout mSpinnerGroup;
    private final TextView mLabel;
    private final DropdownFieldView mMonthDropdown;
    private final DropdownFieldView mDayDropdown;
    private final DropdownFieldView mYearDropdown;
    private final TextView mErrorMessage;
    private @Nullable EditorFieldValidator mValidator;

    // TODO: crbug.com/489405975 - Remove PropertyModel references.
    public DateFieldView(Context context, PropertyModel propertyModel) {
        super(context);
        setOrientation(LinearLayout.VERTICAL);

        mPropertyModel = propertyModel;

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

        final String value = mPropertyModel.get(VALUE);
        @Nullable LocalDate date = new AttributeInstance.DateValue(value).getDate();
        final String monthValue = date == null ? "" : String.valueOf(date.getMonthValue());
        PropertyModel monthModel =
                new PropertyModel.Builder(DropdownFieldProperties.DROPDOWN_ALL_KEYS)
                        .with(DROPDOWN_KEY_VALUE_LIST, getMonthDropdownValues())
                        .with(DROPDOWN_HINT, getMonthDropdownHint(context))
                        .with(DROPDOWN_CALLBACK, this::onDropdownItemSelected)
                        .with(IS_REQUIRED, false)
                        .with(LABEL, "")
                        .with(VALUE, monthValue)
                        .build();
        final String dayValue = date == null ? "" : String.valueOf(date.getDayOfMonth());
        PropertyModel dayModel =
                new PropertyModel.Builder(DropdownFieldProperties.DROPDOWN_ALL_KEYS)
                        .with(DROPDOWN_KEY_VALUE_LIST, getDayDropdownValues())
                        .with(DROPDOWN_HINT, getDayDropdownHint(context))
                        .with(DROPDOWN_CALLBACK, this::onDropdownItemSelected)
                        .with(IS_REQUIRED, false)
                        .with(LABEL, "")
                        .with(VALUE, dayValue)
                        .build();
        // LocalDate always returns the day within the [1; 31] range and month within [1, 12]. The
        // year dropdown is a special case: it's values are always generated within the
        // [X - 90; X + 15] range where X is the current year. The entity attribute's date value can
        // exceed this range. In this case, the year value is used as a hint.
        String yearValue = "";
        String yearHint = getYearDropdownHint(context);
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
                        .with(DROPDOWN_CALLBACK, this::onDropdownItemSelected)
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
        // Initialize the `DATE_VALID` property;
        updateDateValid();
    }

    /**
     * @return The PropertyModel that the DropdownFieldView represents.
     */
    public PropertyModel getFieldModel() {
        return mPropertyModel;
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
        // Update the `DATE_VALID` property.
        updateDateValid();
    }

    @Override
    public void scrollToAndFocus() {
        // Just request focus on the first dropdown.
        mMonthDropdown.scrollToAndFocus();
    }

    @Override
    public boolean isRequired() {
        return mPropertyModel.get(IS_REQUIRED);
    }

    public void setValidator(@Nullable EditorFieldValidator validator) {
        mValidator = validator;
    }

    @Override
    public boolean validate() {
        updateDateValid();
        if (mValidator != null) {
            mValidator.validate(mPropertyModel);
        }
        return TextUtils.isEmpty(mPropertyModel.get(ERROR_MESSAGE));
    }

    private void onDropdownItemSelected(String unused) {
        // Always validate the date to update the `DATE_VALID` property.
        updateDateValid();
        // Reset the `ERROR_MESSAGE` after user interaction.
        mPropertyModel.set(ERROR_MESSAGE, null);
        if (isEmptyDateSelected()) {
            assert mPropertyModel.get(DATE_VALID) : "An empty date is valid";
            // First case: the user has completely reset the date field. Propagate an empty value to
            // the model.
            mPropertyModel.set(VALUE, "");
            return;
        }

        @Nullable LocalDate date = getSelectedDate();
        if (date != null) {
            assert mPropertyModel.get(DATE_VALID) : "A non-null date is valid";
            // Second case: the user has selected a valid date. Propagate it to the model. Partially
            // valid dates are never propagated to the model.
            mPropertyModel.set(VALUE, date.toString());
            return;
        }
        assert !mPropertyModel.get(DATE_VALID)
                : "The date is invalid if it can't be contructed from the selected items";
    }

    /**
     * The date picker is considered empty if all 3 dropdowns are initialized with the hint values.
     * The only special case is when the initial date is out of range: an empty date can't be
     * selected in this case.
     *
     * @return True if the date picker is empty.
     */
    private boolean isEmptyDateSelected() {
        if (!getYearDropdownHint(getContext())
                .equals(mYearDropdown.getFieldModel().get(DROPDOWN_HINT))) {
            // An empty date can't be selected if initial date was out of range.
            return false;
        }

        // The dropdown field `VALUE` is empty if the hint is selected.
        return TextUtils.isEmpty(mMonthDropdown.getFieldModel().get(VALUE))
                && TextUtils.isEmpty(mDayDropdown.getFieldModel().get(VALUE))
                && TextUtils.isEmpty(mYearDropdown.getFieldModel().get(VALUE));
    }

    /**
     * Try to construct the date from the selected dropdown values. The returned date can be `null`
     * in the following cases: 1. A hint is selected in any of the dropdowns. 2. An invalid date
     * combination (like February 31st) is selected.
     */
    private @Nullable LocalDate getSelectedDate() {
        @Nullable String monthValue = mMonthDropdown.getFieldModel().get(VALUE);
        @Nullable String dayValue = mDayDropdown.getFieldModel().get(VALUE);
        @Nullable String yearValue = mYearDropdown.getFieldModel().get(VALUE);
        if (TextUtils.isEmpty(yearValue)
                && !getYearDropdownHint(getContext())
                        .equals(mYearDropdown.getFieldModel().get(DROPDOWN_HINT))) {
            // Year is a special case because the dropdown's hint can be the initial date's year
            // when that year does not fall within the range. The dropdown's VALUE property will be
            // `null` in that case.
            yearValue = mYearDropdown.getFieldModel().get(DROPDOWN_HINT);
        }
        // This call returns `null` if the date combination is invalid (like February 31st).
        return new AttributeInstance.DateValue(dayValue, monthValue, yearValue).getDate();
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
    public static final String getMonthName(Context context, int month) {
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
        List<DropdownKeyValue> dayList = new ArrayList<>();
        for (int day = 1; day <= 31; day++) {
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

    private void updateDateValid() {
        mPropertyModel.set(DATE_VALID, isEmptyDateSelected() || (getSelectedDate() != null));
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

    @VisibleForTesting
    static String getMonthDropdownHint(Context context) {
        return context.getString(R.string.autofill_ai_entity_editor_date_field_month_label);
    }

    @VisibleForTesting
    static String getDayDropdownHint(Context context) {
        return context.getString(R.string.autofill_ai_entity_editor_date_field_day_label);
    }

    @VisibleForTesting
    static String getYearDropdownHint(Context context) {
        return context.getString(R.string.autofill_ai_entity_editor_date_field_year_label);
    }

    public DropdownFieldView getMonthPickerForTest() {
        return mMonthDropdown;
    }

    public DropdownFieldView getDayPickerForTest() {
        return mDayDropdown;
    }

    public DropdownFieldView getYearPickerForTest() {
        return mYearDropdown;
    }
}
