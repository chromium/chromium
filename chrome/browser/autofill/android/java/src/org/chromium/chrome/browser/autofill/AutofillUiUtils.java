// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.ColorFilter;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.text.style.ImageSpan;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.PopupWindow;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Calendar;
import java.util.LinkedList;

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
        textView.setTextAppearance(R.style.TextAppearance_TextMedium_Primary_Baseline_Light);
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
        ColorFilter filter = new PorterDuffColorFilter(
                context.getColor(R.color.input_underline_error_color), PorterDuff.Mode.SRC_IN);

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

    /**
     * Appends the title string with a logo and sets it as the text on the TextView.
     *
     * @param context The context used for fetching the required resources.
     * @param titleTextView The TextView containing the title that the title and the logo should be
     *         set on.
     * @param title The title string for the TextView.
     * @param logoResourceId The resource id for the icon to inlined within the title string.
     */
    public static void inlineTitleStringWithLogo(
            Context context, TextView titleTextView, String title, int logoResourceId) {
        Drawable mInlineTitleIcon = ResourcesCompat.getDrawable(
                context.getResources(), logoResourceId, context.getTheme());
        // The first character will be replaced by the logo, and the consecutive spaces after
        // are used as padding.
        SpannableString titleWithLogo = new SpannableString("   " + title);
        // How much the original logo should scale up in size to match height of text.
        float scaleFactor = titleTextView.getTextSize() / mInlineTitleIcon.getIntrinsicHeight();
        mInlineTitleIcon.setBounds(
                /* left */ 0, /* top */
                0,
                /* right */ (int) (scaleFactor * mInlineTitleIcon.getIntrinsicWidth()),
                /* bottom */ (int) (scaleFactor * mInlineTitleIcon.getIntrinsicHeight()));
        titleWithLogo.setSpan(new ImageSpan(mInlineTitleIcon, ImageSpan.ALIGN_CENTER),
                /* start */ 0,
                /* end */ 1,
                /* flags */ Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
        titleTextView.setText(titleWithLogo, TextView.BufferType.SPANNABLE);
    }

    /**
     * Generates a SpannableString from a list of {@link LegalMessageLine}.
     *
     * @param context The context used for fetching the required resources.
     * @param legalMessageLines The list of LegalMessageLines to be represented as a string.
     * @param underlineLinks True if the links in the legal message lines are to be underlined.
     * @param onClickCallback The callback for the link clicks.
     * @return A {@link SpannableStringBuilder} that can directly be set on a TextView.
     */
    public static SpannableStringBuilder getSpannableStringForLegalMessageLines(Context context,
            LinkedList<LegalMessageLine> legalMessageLines, boolean underlineLinks,
            Callback<String> onClickCallback) {
        SpannableStringBuilder spannableStringBuilder = new SpannableStringBuilder();
        for (LegalMessageLine line : legalMessageLines) {
            SpannableString text = new SpannableString(line.text);
            for (final LegalMessageLine.Link link : line.links) {
                if (underlineLinks) {
                    text.setSpan(new ClickableSpan() {
                        @Override
                        public void onClick(View view) {
                            onClickCallback.onResult(link.url);
                        }
                    }, link.start, link.end, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
                } else {
                    text.setSpan(new NoUnderlineClickableSpan(
                                         context, view -> onClickCallback.onResult(link.url)),
                            link.start, link.end, Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
                }
            }
            spannableStringBuilder.append(text);
        }
        return spannableStringBuilder;
    }

    /**
     * Returns a {@link SpannableString} containing a {@link NoUnderlineClickableSpan} for the text
     * contained within the tags <link1></link1>.
     * @param context The context required to fetch the resources.
     * @param stringResourceId The resource id of the string on which the clickable span should be
     *         applied.
     * @param url The url that should be opened when the clickable span is clicked.
     * @param onClickCallback The callback for the link clicks.
     * @return {@link SpannableString} that can be directly set on the TextView.
     */
    public static SpannableString getSpannableStringWithClickableSpansToOpenLinksInCustomTabs(
            Context context, int stringResourceId, String url, Callback<String> onClickCallback) {
        return SpanApplier.applySpans(context.getString(stringResourceId),
                new SpanApplier.SpanInfo("<link1>", "</link1>",
                        new NoUnderlineClickableSpan(
                                context, view -> onClickCallback.onResult(url))));
    }

    /**
     * Adds dimension params to card art URL for credit cards.
     * @param customIconURL A FIFE URL to fetch the card art icon.
     * @param width in pixels.
     * @param height in pixels.
     * @return {@link GURL} formatted with the icon dimensions to fetch the card art icon.
     */
    public static GURL getCCIconURLWithParams(GURL customIconURL, @Px int width, @Px int height) {
        // TODO(crbug.com/1313616): There is only one gstatic card art image we are using currently.
        // Remove this logic and append FIFE URL suffix by default when the static image is
        // deprecated.
        // Check if the image is gstatic stored in Static Content Service. If not append the
        // dimension params to the FIFE URL.
        if (customIconURL.getSpec().equals(
                    "https://www.gstatic.com/autofill/virtualcard/icon/capitalone.png")) {
            return customIconURL;
        }
        // Params can be added to a FIFE URL by appending them at the end like URL[=params]. "w"
        // option is used to set the width in pixels, "h" is used to set the height in pixels,
        // and "n" represents center cropping the image.
        StringBuilder url = new StringBuilder(customIconURL.getSpec());
        url.append("=w").append(width).append("-h").append(height).append("-n");
        return new GURL(url.toString());
    }

    /**
     * If the card has a valid card art URL, it tries to fetch the bitmap of the required size from
     * PersonalDataManager. If it is not available in cache, then the bitmap of the required size is
     * fetched and stored in cache for the next time.
     * @param context Context required to get resources.
     * @param card The credit card for which the icon is to be retrieved.
     * @param widthId Resource Id for the width spec.
     * @param heightId Resource Id for the height spec.
     * @return {@link Drawable} that can be set as the card icon.
     */
    public static Drawable getCardIcon(
            Context context, CreditCard card, int widthId, int heightId) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ENABLE_CARD_ART_IMAGE)) {
            if (card.getCardArtUrl() != null && card.getCardArtUrl().isValid()) {
                Resources resources = context.getResources();
                Bitmap customIconBitmap =
                        PersonalDataManager.getInstance()
                                .getCustomImageForAutofillSuggestionIfAvailable(
                                        getCCIconURLWithParams(card.getCardArtUrl(),
                                                resources.getDimensionPixelSize(widthId),
                                                resources.getDimensionPixelSize(heightId)));
                if (customIconBitmap != null) {
                    // TODO(crbug.com/1313616): We have one gstatic card art image that is available
                    // in a single size. All other card art images can be fetched in the desired
                    // size. Scale the bitmap to match the desired size. This might not be required
                    // when this gstatic card art image is deprecated.
                    Bitmap scaledBitmap = Bitmap.createScaledBitmap(customIconBitmap,
                            resources.getDimensionPixelSize(widthId),
                            resources.getDimensionPixelSize(heightId), true);
                    return new BitmapDrawable(resources, scaledBitmap);
                }
            }
        }
        return AppCompatResources.getDrawable(context, card.getIssuerIconDrawableId());
    }

    /**
     * If the {@code cardArtUrl} is valid, it tries to fetch the bitmap of the required size from
     * PersonalDataManager. If it is not available in cache, then the bitmap of the required size is
     * fetched and stored in cache for the next time.
     * @param context Context required to get resources.
     * @param cardArtUrl The URL to fetch the icon.
     * @param defaultIconId Resource Id for the default (network) icon if the card art could not be
     *        retrieved.
     * @param widthId Resource Id for the width spec.
     * @param heightId Resource Id for the height spec.
     * @return {@link Drawable} that can be set as the card icon.
     */
    public static Drawable getCardIcon(
            Context context, GURL cardArtUrl, int defaultIconId, int widthId, int heightId) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ENABLE_CARD_ART_IMAGE)) {
            if (cardArtUrl.isValid()) {
                Resources resources = context.getResources();
                Bitmap customIconBitmap =
                        PersonalDataManager.getInstance()
                                .getCustomImageForAutofillSuggestionIfAvailable(
                                        getCCIconURLWithParams(cardArtUrl,
                                                resources.getDimensionPixelSize(widthId),
                                                resources.getDimensionPixelSize(heightId)));
                if (customIconBitmap != null) {
                    // Scale the icon to the desired dimension.
                    Bitmap scaledBitmap = Bitmap.createScaledBitmap(customIconBitmap,
                            resources.getDimensionPixelSize(widthId),
                            resources.getDimensionPixelSize(heightId), true);
                    return new BitmapDrawable(resources, scaledBitmap);
                }
            }
        }
        return AppCompatResources.getDrawable(context, defaultIconId);
    }
}
