// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.AssistantChevronStyle;
import org.chromium.chrome.browser.autofill_assistant.LayoutUtils;
import org.chromium.content.browser.input.PopupItemType;
import org.chromium.content.browser.input.SelectPopupDialog;
import org.chromium.content.browser.input.SelectPopupItem;
import org.chromium.content.browser.picker.InputDialogContainer;
import org.chromium.content.browser.picker.InputDialogContainer.InputActionDelegate;
import org.chromium.ui.base.ime.TextInputType;

import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.List;
import java.util.Locale;

/**
 * A section which allows the user to specify a single date/time value.
 */
public class AssistantDateSection {
    interface Delegate {
        void onDateChanged(@Nullable AssistantDateTime date);
        void onTimeSlotChanged(@Nullable Integer index);
    }

    private final Context mContext;
    private final LinearLayout mRootLayout;
    private final TextView mDateSummaryView;
    private final TextView mTimeSummaryView;
    private final int mTitleToContentPadding;
    private final Locale mLocale;
    private final DateFormat mDateTimeFormat;
    @Nullable
    private Delegate mDelegate;
    @Nullable
    private AssistantDateChoiceOptions mDateChoiceOptions;
    private @Nullable AssistantDateTime mCurrentValue;
    private @Nullable AssistantDateTime mCalendarFallbackValue;
    private @Nullable Integer mCurrentTimeSlot;
    private String mDateNotSetErrorMessage;
    private String mTimeNotSetErrorMessage;

    AssistantDateSection(Context context, ViewGroup parent, Locale locale, DateFormat dateFormat) {
        mContext = context;
        mLocale = locale;
        mDateTimeFormat = dateFormat;
        mTitleToContentPadding = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_payment_request_title_padding);

        LayoutInflater inflater = LayoutUtils.createInflater(context);
        mRootLayout = (LinearLayout) inflater.inflate(R.layout.autofill_assistant_datetime, null);
        mRootLayout.setVisibility(View.GONE);

        AssistantVerticalExpander dateExpander = mRootLayout.findViewById(R.id.date_expander);
        AssistantVerticalExpander timeExpander = mRootLayout.findViewById(R.id.time_expander);

        setupExpander(context, dateExpander);
        setupExpander(context, timeExpander);
        mDateSummaryView = (TextView) dateExpander.getCollapsedView();
        mTimeSummaryView = (TextView) timeExpander.getCollapsedView();

        dateExpander.getTitleAndChevronContainer().setOnClickListener(unusedView -> {
            if (mDateChoiceOptions == null) {
                return;
            }
            InputDialogContainer inputDialogContainer =
                    new InputDialogContainer(mContext, new InputActionDelegate() {
                        @Override
                        public void cancelDateTimeDialog() {
                            // Do nothing.
                        }

                        @Override
                        public void replaceDateTime(double value) {
                            // User tapped the 'clear' button.
                            if (Double.isNaN(value)) {
                                setCurrentDate(null);
                            } else {
                                setCurrentDate(new AssistantDateTime((long) value));
                            }
                        }
                    });

            double selectedValue = Double.NaN;
            if (mCurrentValue != null) {
                selectedValue = mCurrentValue.getTimeInUtcMillis();
            } else if (mCalendarFallbackValue != null) {
                selectedValue = mCalendarFallbackValue.getTimeInUtcMillis();
            }
            inputDialogContainer.showDialog(TextInputType.DATE, selectedValue,
                    mDateChoiceOptions.getMinDate().getTimeInUtcMillis(),
                    mDateChoiceOptions.getMaxDate().getTimeInUtcMillis(), -1, null);
        });

        timeExpander.getTitleAndChevronContainer().setOnClickListener(unusedView -> {
            if (mDateChoiceOptions == null) {
                return;
            }
            List<SelectPopupItem> items = new ArrayList<>();
            for (int i = 0; i < mDateChoiceOptions.getTimeSlots().size(); i++) {
                items.add(new SelectPopupItem(
                        mDateChoiceOptions.getTimeSlots().get(i), PopupItemType.ENABLED));
            }

            SelectPopupDialog dialog = new SelectPopupDialog(context,
                    (indices)
                            -> {
                        if (indices == null) {
                            return;
                        }
                        setCurrentTimeSlot(indices[0]);
                    },
                    items, false,
                    mCurrentTimeSlot != null ? new int[] {mCurrentTimeSlot} : new int[] {});
            dialog.show();
        });

        updateDateSummaryView();
        updateTimeSummaryView();
        parent.addView(mRootLayout,
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
    }

    /** Common view configuration for the vertical expanders. */
    private void setupExpander(Context context, AssistantVerticalExpander expander) {
        View titleView = LayoutUtils.createInflater(context).inflate(
                R.layout.autofill_assistant_payment_request_section_title, null);
        TextView summaryView = new TextView(context);
        summaryView.setSingleLine();

        expander.setTitleView(titleView,
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        expander.setCollapsedView(summaryView,
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        expander.setFixed(true);
        expander.setChevronStyle(AssistantChevronStyle.ALWAYS);
        expander.findViewById(R.id.section_title_add_button).setVisibility(View.GONE);
    }

    void setDateChoiceOptions(AssistantDateChoiceOptions dateChoiceOptions) {
        mDateChoiceOptions = dateChoiceOptions;
    }

    void setDelegate(Delegate delegate) {
        mDelegate = delegate;
    }

    View getView() {
        return mRootLayout;
    }

    void setVisible(boolean visible) {
        mRootLayout.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    void setPaddings(int topPadding, int bottomPadding) {
        AssistantVerticalExpander dateExpander = mRootLayout.findViewById(R.id.date_expander);
        AssistantVerticalExpander timeExpander = mRootLayout.findViewById(R.id.time_expander);

        View dateTitleView = dateExpander.getTitleView();
        View timeTitleView = timeExpander.getTitleView();
        dateTitleView.setPadding(dateTitleView.getPaddingLeft(), topPadding,
                dateTitleView.getPaddingRight(), mTitleToContentPadding);
        timeTitleView.setPadding(timeTitleView.getPaddingLeft(), topPadding,
                timeTitleView.getPaddingRight(), mTitleToContentPadding);

        View dateChevronView = dateExpander.getChevronButton();
        View timeChevronView = timeExpander.getChevronButton();
        dateChevronView.setPadding(dateChevronView.getPaddingLeft(), topPadding,
                dateChevronView.getPaddingRight(), dateChevronView.getPaddingBottom());
        timeChevronView.setPadding(timeChevronView.getPaddingLeft(), topPadding,
                timeChevronView.getPaddingRight(), timeChevronView.getPaddingBottom());

        mRootLayout.setPadding(mRootLayout.getPaddingLeft(), mRootLayout.getPaddingTop(),
                mRootLayout.getPaddingRight(), bottomPadding);
    }

    void setDateTitle(String title) {
        AssistantVerticalExpander dateExpander = mRootLayout.findViewById(R.id.date_expander);
        TextView titleView = dateExpander.getTitleView().findViewById(R.id.section_title);
        titleView.setText(title);
    }

    void setTimeTitle(String title) {
        AssistantVerticalExpander dateExpander = mRootLayout.findViewById(R.id.time_expander);
        TextView titleView = dateExpander.getTitleView().findViewById(R.id.section_title);
        titleView.setText(title);
    }

    void setCurrentDate(@Nullable AssistantDateTime value) {
        mCurrentValue = value;
        updateDateSummaryView();

        if (mDelegate != null) {
            mDelegate.onDateChanged(mCurrentValue);
        }
    }

    /**
     * Sets the date that the calendar should fall back to if {@code mCurrentValue} is not set. The
     * default is the current date.
     */
    void setCalendarFallbackDate(@Nullable AssistantDateTime value) {
        mCalendarFallbackValue = value;
    }

    void setDateNotSetErrorMessage(String errorMessage) {
        mDateNotSetErrorMessage = errorMessage;
        updateDateSummaryView();
    }

    void setTimeNotSetErrorMessage(String errorMessage) {
        mTimeNotSetErrorMessage = errorMessage;
        updateTimeSummaryView();
    }

    /**
     * @param index the current timeslot. Can be null to indicate that no timeslot is selected.
     */
    void setCurrentTimeSlot(@Nullable Integer index) {
        mCurrentTimeSlot = index;
        updateTimeSummaryView();

        if (mDelegate != null) {
            mDelegate.onTimeSlotChanged(mCurrentTimeSlot);
        }
    }

    private void updateDateSummaryView() {
        if (mCurrentValue == null) {
            mDateSummaryView.setText(mDateNotSetErrorMessage);
            ApiCompatibilityUtils.setTextAppearance(
                    mDateSummaryView, R.style.TextAppearance_ErrorCaption);
            return;
        }

        Calendar calendar = Calendar.getInstance(mLocale);
        calendar.setTimeInMillis(mCurrentValue.getTimeInMillis(mLocale));
        mDateSummaryView.setText(mDateTimeFormat.format(calendar.getTime()));
        ApiCompatibilityUtils.setTextAppearance(
                mDateSummaryView, R.style.TextAppearance_TextMedium_Secondary);
    }

    private void updateTimeSummaryView() {
        if (mDateChoiceOptions == null || mCurrentTimeSlot == null) {
            mTimeSummaryView.setText(mTimeNotSetErrorMessage);
            ApiCompatibilityUtils.setTextAppearance(
                    mTimeSummaryView, R.style.TextAppearance_ErrorCaption);
            return;
        }
        mTimeSummaryView.setText(mDateChoiceOptions.getTimeSlots().get(mCurrentTimeSlot));
        ApiCompatibilityUtils.setTextAppearance(
                mTimeSummaryView, R.style.TextAppearance_TextMedium_Secondary);
    }
}
