// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.ColorFilter;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.os.Build;
import android.os.Handler;
import android.support.v4.widget.TextViewCompat;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.PopupWindow;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Calendar;
/**
 * Helper methods that can be used across multiple Autofill UIs.
 */
public class AutofillUiUtils {
    /**
     * Interface to provide the horizontal and vertical offset for the tooltip.
     */
    public interface OffsetProvider {
        /** Returns the X offset for the tooltip. */
        int getXOffset(TextView textView);
        /** Returns the Y offset for the tooltip. */
        int getYOffset(TextView textView);
    }

    // 200ms is chosen small enough not to be detectable to human eye, but big
    // enough for to avoid any race conditions on modern machines.
    private static final int TOOLTIP_DEFERRED_PERIOD_MS = 200;
    public static final int EXPIRATION_FIELDS_LENGTH = 2;

    @IntDef({ErrorType.EXPIRATION_MONTH, ErrorType.EXPIRATION_YEAR, ErrorType.EXPIRATION_DATE,
            ErrorType.CVC, ErrorType.CVC_AND_EXPIRATION, ErrorType.NOT_ENOUGH_INFO, ErrorType.NONE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ErrorType {
        int EXPIRATION_MONTH = 1;
        int EXPIRATION_YEAR = 2;
        int EXPIRATION_DATE = 3;
        int CVC = 4;
        int CVC_AND_EXPIRATION = 5;
        int NOT_ENOUGH_INFO = 6;
        int NONE = 7;
    }
    /**
     * Show Tooltip UI.
     *
     * @param context Context required to get resources.
     * @param popup {@PopupWindow} that shows tooltip UI.
     * @param text  Text to be shown in tool tip UI.
     * @param offsetProvider Interface to provide the X and Y offsets.
     * @param anchorView Anchor view under which tooltip popup has to be shown
     * @param dismissAction Tooltip dismissive action.
     */
    public static void showTooltip(Context context, PopupWindow popup, int text,
            OffsetProvider offsetProvider, View anchorView, final Runnable dismissAction) {
        TextView textView = new TextView(context);
        textView.setText(text);
        TextViewCompat.setTextAppearance(textView, R.style.TextAppearance_WhiteBody);
        Resources resources = context.getResources();
        int hPadding = resources.getDimensionPixelSize(R.dimen.autofill_tooltip_horizontal_padding);
        int vPadding = resources.getDimensionPixelSize(R.dimen.autofill_tooltip_vertical_padding);
        textView.setPadding(hPadding, vPadding, hPadding, vPadding);
        textView.measure(MeasureSpec.UNSPECIFIED, MeasureSpec.UNSPECIFIED);

        popup.setContentView(textView);
        popup.setHeight(ViewGroup.LayoutParams.WRAP_CONTENT);
        popup.setWidth(ViewGroup.LayoutParams.WRAP_CONTENT);
        popup.setOutsideTouchable(true);
        popup.setBackgroundDrawable(ApiCompatibilityUtils.getDrawable(
                resources, R.drawable.store_locally_tooltip_background));

        // An alternate solution is to extend TextView and override onConfigurationChanged. However,
        // due to lemon compression, onConfigurationChanged never gets called.
        final ComponentCallbacks componentCallbacks = new ComponentCallbacks() {
            @Override
            public void onConfigurationChanged(Configuration configuration) {
                // If the popup was already showing dismiss it. This may happen during an
                // orientation change.
                if (configuration.orientation == Configuration.ORIENTATION_LANDSCAPE
                        && popup != null) {
                    popup.dismiss();
                }
            }

            @Override
            public void onLowMemory() {}
        };

        ContextUtils.getApplicationContext().registerComponentCallbacks(componentCallbacks);

        popup.setOnDismissListener(() -> {
            Handler h = new Handler();
            h.postDelayed(dismissAction, TOOLTIP_DEFERRED_PERIOD_MS);
            ContextUtils.getApplicationContext().unregisterComponentCallbacks(componentCallbacks);
        });

        popup.showAsDropDown(anchorView, offsetProvider.getXOffset(textView),
                offsetProvider.getYOffset(textView));
        textView.announceForAccessibility(textView.getText());
    }

    /**
     * Determines what type of error, if any, is present in the expiration date fields of the
     * prompt.
     *
     * @param monthInput EditText for the month field.
     * @param yearInput EditText for the year field.
     * @param didFocusOnMonth True if the month field was ever in focus.
     * @param didFocusOnYear True if the year field was ever in focus.
     * @return The ErrorType value representing the type of error found for the expiration date
     *         unmask fields.
     */
    @ErrorType
    public static int getExpirationDateErrorType(EditText monthInput, EditText yearInput,
            boolean didFocusOnMonth, boolean didFocusOnYear) {
        Calendar calendar = Calendar.getInstance();
        int thisYear = calendar.get(Calendar.YEAR);
        int thisMonth = calendar.get(Calendar.MONTH) + 1; // calendar month is 0-based

        int month = getMonth(monthInput);
        if (month == -1) {
            if (monthInput.getText().length() == EXPIRATION_FIELDS_LENGTH
                    || (!monthInput.isFocused() && didFocusOnMonth)) {
                return ErrorType.EXPIRATION_MONTH;
            }
            // If year was focused before, proceed to check if year is valid.
            if (!didFocusOnYear) {
                return ErrorType.NOT_ENOUGH_INFO;
            }
        }

        int year = getFourDigitYear(yearInput);
        if (year == -1) {
            if (yearInput.getText().length() == EXPIRATION_FIELDS_LENGTH
                    || (!yearInput.isFocused() && didFocusOnYear)) {
                return ErrorType.EXPIRATION_YEAR;
            }
            return ErrorType.NOT_ENOUGH_INFO;
        }
        // Year is valid but month is still being edited.
        if (month == -1) {
            return ErrorType.NOT_ENOUGH_INFO;
        }
        if (year == thisYear && month < thisMonth) {
            return ErrorType.EXPIRATION_DATE;
        }

        return ErrorType.NONE;
    }

    /**
     * @param yearInput EditText for the year field.
     * @return The expiration year the user entered.
     *         Two digit values (such as 17) will be converted to 4 digit years (such as 2017).
     *         Returns -1 if the input is empty or otherwise not a valid year (previous year or
     *         more than 10 years in the future).
     */
    public static int getFourDigitYear(EditText yearInput) {
        Calendar calendar = Calendar.getInstance();
        int thisYear = calendar.get(Calendar.YEAR);
        try {
            int year = Integer.parseInt(yearInput.getText().toString());
            if (year < 0) return -1;
            if (year < 100) year += thisYear - thisYear % 100;
            if (year < thisYear || year > thisYear + 10) return -1;
            return year;
        } catch (NumberFormatException e) {
            return -1;
        }
    }

    /**
     * @param monthInput EditText for the month field.
     * @return The expiration month the user entered.
     *         Returns -1 if not a valid month.
     */
    @VisibleForTesting
    static int getMonth(EditText monthInput) {
        try {
            int month = Integer.parseInt(monthInput.getText().toString());
            if (month < 1 || month > 12) {
                return -1;
            }
            return month;
        } catch (NumberFormatException e) {
            return -1;
        }
    }

    /**
     * @param context Context required to get resources.
     * @param errorType Type of the error used to get the resource string.
     * @return Error string retrieved from the string resources.
     */
    public static String getErrorMessage(Context context, @ErrorType int errorType) {
        Resources resources = context.getResources();
        switch (errorType) {
            case ErrorType.EXPIRATION_MONTH:
                return resources.getString(
                        R.string.autofill_card_unmask_prompt_error_try_again_expiration_month);
            case ErrorType.EXPIRATION_YEAR:
                return resources.getString(
                        R.string.autofill_card_unmask_prompt_error_try_again_expiration_year);
            case ErrorType.EXPIRATION_DATE:
                return resources.getString(
                        R.string.autofill_card_unmask_prompt_error_try_again_expiration_date);
            case ErrorType.CVC:
                return resources.getString(
                        R.string.autofill_card_unmask_prompt_error_try_again_cvc);
            case ErrorType.CVC_AND_EXPIRATION:
                return resources.getString(
                        R.string.autofill_card_unmask_prompt_error_try_again_cvc_and_expiration);
            case ErrorType.NONE:
            case ErrorType.NOT_ENOUGH_INFO:
            default:
                return "";
        }
    }

    /**
     * Shows (or removes) the appropriate error message and apply the error filter to the
     * appropriate fields depending on the error type.
     *
     * @param errorType The type of error detected.
     * @param context Context required to get resources,
     * @param errorMessageTextView TextView to display the error message.
     */
    public static void showDetailedErrorMessage(
            @ErrorType int errorType, Context context, TextView errorMessageTextView) {
        switch (errorType) {
            case ErrorType.NONE:
            case ErrorType.NOT_ENOUGH_INFO:
                clearInputError(errorMessageTextView);
                break;
            default:
                String errorMessage = getErrorMessage(context, errorType);
                showErrorMessage(errorMessage, errorMessageTextView);
        }
    }

    /**
     * Sets the error message on the inputs.
     * @param message The error message to show.
     * @param errorMessageTextView TextView used to display the error message.
     */
    public static void showErrorMessage(String message, TextView errorMessageTextView) {
        assert message != null;

        // Set the message to display;
        errorMessageTextView.setText(message);
        errorMessageTextView.setVisibility(View.VISIBLE);

        // A null message is passed in during card verification, which also makes an announcement.
        // Announcing twice in a row may cancel the first announcement.
        errorMessageTextView.announceForAccessibility(message);
    }

    /**
     * Removes the error message on the inputs.
     * @param errorMessageTextView TextView used to display the error message.
     */
    public static void clearInputError(TextView errorMessageTextView) {
        errorMessageTextView.setText(null);
        errorMessageTextView.setVisibility(View.GONE);
    }

    /**
     * Applies the error filter to the invalid fields based on the errorType.
     *
     * @param errorType The ErrorType value representing the type of error found for the unmask
     *                  fields.
     * @param context Context required to get resources,
     * @param monthInput EditText for the month field.
     * @param yearInput EditText for the year field.
     * @param cvcInput EditText for the cvc field.
     */
    public static void updateColorForInputs(@ErrorType int errorType, Context context,
            EditText monthInput, EditText yearInput, EditText cvcInput) {
        // The rest of this code makes L-specific assumptions about the background being used to
        // draw the TextInput.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;

        ColorFilter filter =
                new PorterDuffColorFilter(ApiCompatibilityUtils.getColor(context.getResources(),
                                                  R.color.input_underline_error_color),
                        PorterDuff.Mode.SRC_IN);

        // Decide on what field(s) to apply the filter.
        boolean filterMonth = errorType == ErrorType.EXPIRATION_MONTH
                || errorType == ErrorType.EXPIRATION_DATE
                || errorType == ErrorType.CVC_AND_EXPIRATION;
        boolean filterYear = errorType == ErrorType.EXPIRATION_YEAR
                || errorType == ErrorType.EXPIRATION_DATE
                || errorType == ErrorType.CVC_AND_EXPIRATION;

        updateColorForInput(monthInput, filterMonth ? filter : null);
        updateColorForInput(yearInput, filterYear ? filter : null);

        if (cvcInput != null) {
            boolean filterCvc =
                    errorType == ErrorType.CVC || errorType == ErrorType.CVC_AND_EXPIRATION;
            updateColorForInput(cvcInput, filterCvc ? filter : null);
        }
    }

    /**
     * Sets the stroke color for the given input.
     * @param input The input to modify.
     * @param filter The color filter to apply to the background.
     */
    public static void updateColorForInput(EditText input, ColorFilter filter) {
        input.getBackground().mutate().setColorFilter(filter);
    }
}
