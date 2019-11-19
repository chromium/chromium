// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data;

import android.content.Context;
import android.support.annotation.Nullable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantVerticalExpander.ChevronStyle;
import org.chromium.content.browser.picker.InputDialogContainer;
import org.chromium.content.browser.picker.InputDialogContainer.InputActionDelegate;
import org.chromium.ui.base.ime.TextInputType;

import java.text.DateFormat;
import java.util.Calendar;
import java.util.Locale;

/**
 * A section which allows the user to specify a single date/time value.
 */
public class AssistantDateSection {
    interface Delegate {
        void onDateTimeChanged(int year, int month, int day, int hour, int minute, int second);
    }

    private final Context mContext;
    private final AssistantVerticalExpander mSectionExpander;
    private final View mSummaryView;
    private final int mTitleToContentPadding;
    private final Locale mLocale;
    private final DateFormat mDateTimeFormat;
    @Nullable
    private Delegate mDelegate;
    @Nullable
    private AssistantDateChoiceOptions mDateChoiceOptions;
    private AssistantDateTime mCurrentValue;

    AssistantDateSection(Context context, ViewGroup parent, Locale locale, DateFormat dateFormat) {
        mContext = context;
        mLocale = locale;
        mCurrentValue = new AssistantDateTime(Calendar.getInstance());
        mDateTimeFormat = dateFormat;
        mTitleToContentPadding = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_payment_request_title_padding);

        LayoutInflater inflater = LayoutInflater.from(context);
        mSectionExpander = new AssistantVerticalExpander(context, null);
        View sectionTitle =
                inflater.inflate(R.layout.autofill_assistant_payment_request_section_title, null);
        mSummaryView = inflater.inflate(R.layout.autofill_assistant_datetime, null);

        View.OnClickListener onClickListener = unusedView -> {
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
                                setCurrentValue(mDateChoiceOptions.getInitialValue());
                            } else {
                                setCurrentValue((long) value);
                            }
                        }
                    });

            inputDialogContainer.showDialog(TextInputType.DATE_TIME,
                    mCurrentValue.getTimeInUtcMillis(),
                    mDateChoiceOptions.getMinValue().getTimeInUtcMillis(),
                    mDateChoiceOptions.getMaxValue().getTimeInUtcMillis(), -1, null);
        };
        mSummaryView.setOnClickListener(onClickListener);
        sectionTitle.setOnClickListener(onClickListener);
        mSectionExpander.getChevronButton().setOnClickListener(onClickListener);

        mSectionExpander.setTitleView(sectionTitle,
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        mSectionExpander.setCollapsedView(mSummaryView,
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        // Adjust margins such that title and collapsed views are indented.
        int horizontalMargin = context.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_bottombar_horizontal_spacing);
        setHorizontalMargins(sectionTitle, horizontalMargin, horizontalMargin);
        setHorizontalMargins(mSectionExpander.getChevronButton(), 0, horizontalMargin);
        setHorizontalMargins(mSummaryView, horizontalMargin, 0);

        mSectionExpander.findViewById(R.id.section_title_add_button).setVisibility(View.GONE);
        mSectionExpander.setFixed(true);
        mSectionExpander.setChevronStyle(ChevronStyle.ALWAYS);

        updateTextView();
        parent.addView(mSectionExpander,
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
    }

    void setDateChoiceOptions(AssistantDateChoiceOptions dateChoiceOptions) {
        mDateChoiceOptions = dateChoiceOptions;
        setCurrentValue(mDateChoiceOptions.getInitialValue());
    }

    void setDelegate(Delegate delegate) {
        mDelegate = delegate;
    }

    View getView() {
        return mSectionExpander;
    }

    void setVisible(boolean visible) {
        mSectionExpander.setVisibility(visible ? View.VISIBLE : View.GONE);
    }

    void setPaddings(int topPadding, int bottomPadding) {
        View titleView = mSectionExpander.getTitleView();
        titleView.setPadding(titleView.getPaddingLeft(), topPadding, titleView.getPaddingRight(),
                mTitleToContentPadding);
        mSectionExpander.getCollapsedView().setPadding(
                mSectionExpander.getCollapsedView().getPaddingLeft(),
                mSectionExpander.getCollapsedView().getPaddingTop(),
                mSectionExpander.getCollapsedView().getPaddingRight(), bottomPadding);
    }

    void setTitle(String title) {
        TextView titleView = mSectionExpander.getTitleView().findViewById(R.id.section_title);
        titleView.setText(title);
    }

    AssistantDateTime getCurrentValue() {
        return mCurrentValue;
    }

    void setCurrentValue(AssistantDateTime value) {
        setCurrentValue(value.getTimeInUtcMillis());
    }

    void setCurrentValue(long millisUtc) {
        mCurrentValue = new AssistantDateTime(millisUtc);
        updateTextView();

        if (mDelegate != null) {
            mDelegate.onDateTimeChanged(mCurrentValue.getYear(), mCurrentValue.getMonth(),
                    mCurrentValue.getDay(), mCurrentValue.getHour(), mCurrentValue.getMinute(),
                    mCurrentValue.getSecond());
        }
    }

    private void updateTextView() {
        TextView dateTimeView = mSummaryView.findViewById(R.id.datetime);
        Calendar calendar = Calendar.getInstance(mLocale);
        calendar.setTimeInMillis(mCurrentValue.getTimeInMillis(mLocale));
        dateTimeView.setText(mDateTimeFormat.format(calendar.getTime()));
    }

    private void setHorizontalMargins(View view, int marginStart, int marginEnd) {
        ViewGroup.MarginLayoutParams lp = (ViewGroup.MarginLayoutParams) view.getLayoutParams();
        lp.setMarginStart(marginStart);
        lp.setMarginEnd(marginEnd);
        view.setLayoutParams(lp);
    }
}
