// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.PorterDuffXfermode;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.text.InputType;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.text.style.ImageSpan;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.PopupWindow;
import android.widget.TextView;

import androidx.annotation.DimenRes;
import androidx.annotation.IntDef;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.ContextCompat;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.components.autofill.FieldType;
import org.chromium.components.autofill.ImageSize;
import org.chromium.components.autofill.ImageType;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Calendar;
import java.util.List;
import java.util.Optional;

/** Helper methods that can be used across multiple Autofill UIs. */
@NullMarked
public class AutofillUiUtils {
    // URL for the "Payment methods" page on the Google Wallet website. To manage a specific FOP,
    // append its instrument id as a query parameter using '&id='.
    private static final String MANAGE_PAYMENT_METHODS_URL =
            "https://pay.google.com/pay?p=paymentmethods&utm_source=chrome&utm_medium=settings&utm_campaign=payment_methods";
    // URL for the "Payment methods" page on the Google Wallet sandbox website. To manage a specific
    // sandbox FOP, append its instrument id as a query parameter using '&id='.
    private static final String MANAGE_SANDBOX_PAYMENT_METHODS_URL =
            "https://pay.sandbox.google.com/pay?p=paymentmethods&utm_source=chrome&utm_medium=settings&utm_campaign=payment_methods";
    public static final String CAPITAL_ONE_ICON_URL =
            "https://www.gstatic.com/autofill/virtualcard/icon/capitalone.png";

    /** Interface to provide the horizontal and vertical offset for the tooltip. */
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

    @IntDef({
        ErrorType.EXPIRATION_MONTH,
        ErrorType.EXPIRATION_YEAR,
        ErrorType.EXPIRATION_DATE,
        ErrorType.CVC,
        ErrorType.CVC_AND_EXPIRATION,
        ErrorType.NOT_ENOUGH_INFO,
        ErrorType.NONE
    })
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

    /** Contains dimensional specs for icons by the {@code AutofillImageFetcher}. */
    public static class IconSpecs {
        private final Context mContext;
        private final @ImageType int mImageType;
        private final @DimenRes int mWidthId;
        private final @DimenRes int mHeightId;
        private final @DimenRes int mCornerRadiusId;
        private final @DimenRes int mBorderWidthId;

        /**
         * @param context to get the resources.
         * @param imageType the type of the image supported by the {@link AutofillImageFetcher}.
         * @param widthId Resource Id for the icon's width spec.
         * @param heightId Resource Id for the icon's height spec.
         */
        private IconSpecs(
                Context context,
                @ImageType int imageType,
                @DimenRes int widthId,
                @DimenRes int heightId) {
            this(context, imageType, widthId, heightId, 0, 0);
        }

        /**
         * @param context to get the resources.
         * @param imageType the type of the image supported by the {@link AutofillImageFetcher}.
         * @param widthId Resource Id for the icon's width spec.
         * @param heightId Resource Id for the icon's height spec.
         * @param cornerRadiusId Resource Id for the icon's corner radius spec.
         * @param borderWidthId Resource Id for the icon's border width spec.
         */
        private IconSpecs(
                Context context,
                @ImageType int imageType,
                @DimenRes int widthId,
                @DimenRes int heightId,
                @DimenRes int cornerRadiusId,
                @DimenRes int borderWidthId) {
            mContext = context;
            mImageType = imageType;
            mWidthId = widthId;
            mHeightId = heightId;
            mCornerRadiusId = cornerRadiusId;
            mBorderWidthId = borderWidthId;
        }

        /**
         * Create the {@link IconSpecs} for the icon type and based on the icon size (small or large
         * or square) of the icon to be rendered.
         *
         * @param context to get the resources.
         * @param imageType Enum that specifies the type of the icon fetched by the {@code
         *     AutofillImageFetcher}.
         * @param imageSize Enum that specifies the icon's size (small or large or square).
         * @return {@link IconSpecs} instance containing the specs for the icon.
         */
        public static IconSpecs create(
                Context context, @ImageType int imageType, @ImageSize int imageSize) {
            switch (imageType) {
                case ImageType.CREDIT_CARD_ART_IMAGE:
                case ImageType.PIX_ACCOUNT_IMAGE:
                    return createForCreditCardIcon(context, imageSize);
                case ImageType.VALUABLE_IMAGE:
                    return createForValuableIcon(context, imageSize);
            }
            assert false : "Image type not handled: " + imageType;
            return assumeNonNull(null);
        }

        private static IconSpecs createForCreditCardIcon(
                Context context, @ImageSize int imageSize) {
            switch (imageSize) {
                case ImageSize.LARGE:
                    return new IconSpecs(
                            context,
                            ImageType.CREDIT_CARD_ART_IMAGE,
                            R.dimen.large_card_icon_width,
                            R.dimen.large_card_icon_height,
                            R.dimen.large_card_icon_corner_radius,
                            R.dimen.card_icon_border_width);
                case ImageSize.SQUARE:
                    return new IconSpecs(
                            context,
                            ImageType.CREDIT_CARD_ART_IMAGE,
                            R.dimen.square_card_icon_side_length,
                            R.dimen.square_card_icon_side_length,
                            R.dimen.square_card_icon_corner_radius,
                            R.dimen.card_icon_border_width_zero);
                case ImageSize.SMALL:
                    return new IconSpecs(
                            context,
                            ImageType.CREDIT_CARD_ART_IMAGE,
                            R.dimen.small_card_icon_width,
                            R.dimen.small_card_icon_height,
                            R.dimen.small_card_icon_corner_radius,
                            R.dimen.card_icon_border_width);
            }
            assert false : "Image size not handled: " + imageSize;
            return assumeNonNull(null);
        }

        private static IconSpecs createForValuableIcon(Context context, @ImageSize int imageSize) {
            switch (imageSize) {
                case ImageSize.LARGE:
                    return new IconSpecs(
                            context,
                            ImageType.VALUABLE_IMAGE,
                            R.dimen.large_valuable_icon_size,
                            R.dimen.large_valuable_icon_size);
                case ImageSize.SMALL:
                    return new IconSpecs(
                            context,
                            ImageType.VALUABLE_IMAGE,
                            R.dimen.small_valuable_icon_size,
                            R.dimen.small_valuable_icon_size);
            }
            assert false : "Image size not handled: " + imageSize;
            return assumeNonNull(null);
        }

        public GURL getResolvedIconUrl(GURL iconUrl) {
            switch (mImageType) {
                case ImageType.CREDIT_CARD_ART_IMAGE:
                case ImageType.PIX_ACCOUNT_IMAGE:
                    return getFifeIconUrlWithParams(
                            iconUrl, getWidth(), getHeight(), /* circleCrop= */ false, /* requestPng= */ false);
                case ImageType.VALUABLE_IMAGE:
                    return getFifeIconUrlWithParams(
                            iconUrl, getWidth(), getHeight(), /* circleCrop= */ true, /* requestPng= */ true);
            }
            assert false : "Image type not handled: " + mImageType;
            return assumeNonNull(null);
        }

        public @Px int getWidth() {
            return mContext.getResources().getDimensionPixelSize(mWidthId);
        }

        public @Px int getHeight() {
            return mContext.getResources().getDimensionPixelSize(mHeightId);
        }

        public @Px int getCornerRadius() {
            return mCornerRadiusId == 0
                    ? 0
                    : mContext.getResources().getDimensionPixelSize(mCornerRadiusId);
        }

        public @Px int getBorderWidth() {
            return mBorderWidthId == 0
                    ? 0
                    : mContext.getResources().getDimensionPixelSize(mBorderWidthId);
        }
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
    public static void showTooltip(
            Context context,
            PopupWindow popup,
            int text,
            OffsetProvider offsetProvider,
            View anchorView,
            final Runnable dismissAction) {
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
        popup.setBackgroundDrawable(
                ApiCompatibilityUtils.getDrawable(
                        resources, R.drawable.store_locally_tooltip_background));

        // An alternate solution is to extend TextView and override onConfigurationChanged. However,
        // due to lemon compression, onConfigurationChanged never gets called.
        final ComponentCallbacks componentCallbacks =
                new ComponentCallbacks() {
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

        popup.setOnDismissListener(
                () -> {
                    Handler h = new Handler();
                    h.postDelayed(dismissAction, TOOLTIP_DEFERRED_PERIOD_MS);
                    ContextUtils.getApplicationContext()
                            .unregisterComponentCallbacks(componentCallbacks);
                });

        popup.showAsDropDown(
                anchorView,
                offsetProvider.getXOffset(textView),
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
    public static @ErrorType int getExpirationDateErrorType(
            EditText monthInput,
            EditText yearInput,
            boolean didFocusOnMonth,
            boolean didFocusOnYear) {
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
     *     fields.
     * @param context Context required to get resources,
     * @param monthInput EditText for the month field.
     * @param yearInput EditText for the year field.
     * @param cvcInput EditText for the cvc field.
     */
    public static void updateColorForInputs(
            @ErrorType int errorType,
            Context context,
            EditText monthInput,
            EditText yearInput,
            @Nullable EditText cvcInput) {
        ColorFilter filter =
                new PorterDuffColorFilter(
                        context.getColor(R.color.input_underline_error_color),
                        PorterDuff.Mode.SRC_IN);

        // Decide on what field(s) to apply the filter.
        boolean filterMonth =
                errorType == ErrorType.EXPIRATION_MONTH
                        || errorType == ErrorType.EXPIRATION_DATE
                        || errorType == ErrorType.CVC_AND_EXPIRATION;
        boolean filterYear =
                errorType == ErrorType.EXPIRATION_YEAR
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
     *
     * @param input The input to modify.
     * @param filter The color filter to apply to the background.
     */
    public static void updateColorForInput(EditText input, @Nullable ColorFilter filter) {
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
        Drawable mInlineTitleIcon =
                ResourcesCompat.getDrawable(
                        context.getResources(), logoResourceId, context.getTheme());
        assumeNonNull(mInlineTitleIcon);
        // The first character will be replaced by the logo, and the consecutive spaces after
        // are used as padding.
        SpannableString titleWithLogo = new SpannableString("   " + title);
        // How much the original logo should scale up in size to match height of text.
        float scaleFactor = titleTextView.getTextSize() / mInlineTitleIcon.getIntrinsicHeight();
        mInlineTitleIcon.setBounds(
                /* left= */ 0,
                /* top= */ 0,
                /* right */ (int) (scaleFactor * mInlineTitleIcon.getIntrinsicWidth()),
                /* bottom */ (int) (scaleFactor * mInlineTitleIcon.getIntrinsicHeight()));
        titleWithLogo.setSpan(
                new ImageSpan(mInlineTitleIcon, ImageSpan.ALIGN_CENTER),
                /* start= */ 0,
                /* end= */ 1,
                /* flags= */ Spannable.SPAN_INCLUSIVE_EXCLUSIVE);
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
    public static SpannableStringBuilder getSpannableStringForLegalMessageLines(
            Context context,
            List<LegalMessageLine> legalMessageLines,
            boolean underlineLinks,
            Callback<String> onClickCallback) {
        SpannableStringBuilder spannableStringBuilder = new SpannableStringBuilder();
        for (int i = 0; i < legalMessageLines.size(); i++) {
            LegalMessageLine line = legalMessageLines.get(i);
            SpannableString text = new SpannableString(line.text);
            for (final LegalMessageLine.Link link : line.links) {
                if (underlineLinks) {
                    text.setSpan(
                            new ClickableSpan() {
                                @Override
                                public void onClick(View view) {
                                    onClickCallback.onResult(link.url);
                                }
                            },
                            link.start,
                            link.end,
                            Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
                } else {
                    text.setSpan(
                            new ChromeClickableSpan(
                                    context, view -> onClickCallback.onResult(link.url)),
                            link.start,
                            link.end,
                            Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
                }
            }
            spannableStringBuilder.append(text);
            if (i != legalMessageLines.size() - 1) {
                spannableStringBuilder.append("\n");
            }
        }
        return spannableStringBuilder;
    }

    /**
     * Returns a {@link SpannableString} containing a {@link ChromeClickableSpan} for the text
     * contained within the tags <link1></link1>.
     *
     * @param context The context required to fetch the resources.
     * @param stringResourceId The resource id of the string on which the clickable span should be
     *     applied.
     * @param url The url that should be opened when the clickable span is clicked.
     * @param onClickCallback The callback for the link clicks.
     * @return {@link SpannableString} that can be directly set on the TextView.
     */
    public static SpannableString getSpannableStringWithClickableSpansToOpenLinksInCustomTabs(
            Context context, int stringResourceId, String url, Callback<String> onClickCallback) {
        return SpanApplier.applySpans(
                context.getString(stringResourceId),
                new SpanApplier.SpanInfo(
                        "<link1>",
                        "</link1>",
                        new ChromeClickableSpan(context, view -> onClickCallback.onResult(url))));
    }

    /**
     * Adds dimension params to a FIFE image URL.
     *
     * @param customIconUrl A FIFE URL to fetch the image.
     * @param width in pixels.
     * @param height in pixels.
     * @param circleCrop whether to the circle crop parameter to the URL ('-cc').
     * @return {@link GURL} formatted with the icon dimensions to fetch the image.
     */
    @VisibleForTesting
    static GURL getFifeIconUrlWithParams(
            GURL customIconUrl, @Px int width, @Px int height, boolean circleCrop, boolean requestPng) {
        // Params can be added to a FIFE URL by appending them at the end like URL[=params]. "w"
        // option is used to set the width in pixels, and "h" is used to set the height in pixels.
        StringBuilder url = new StringBuilder(customIconUrl.getSpec());
        url.append("=w").append(width).append("-h").append(height);
        if (circleCrop) {
            url.append("-cc");
        }
        if (requestPng) {
            url.append("-rp");
        }

        return new GURL(url.toString());
    }

    /**
     * Always show the Capital One virtual card icon for virtual cards if the card
     * icon URL is available for the card. Never show the Capital One virtual card
     * icon for FPAN.
     * Otherwise, show rich card art.
     *
     * @param customIconUrl {@link GURL} for fetching the custom icon.
     * @param isVirtualCard Whether or not the card is a virtual card.
     * @return True if the custom icon should be shown. False otherwise.
     */
    public static boolean shouldShowCustomIcon(GURL customIconUrl, boolean isVirtualCard) {
        if (customIconUrl == null) {
            return false;
        }

        if (isVirtualCard && customIconUrl.getSpec().equals(CAPITAL_ONE_ICON_URL)) {
            return true;
        }

        if (!customIconUrl.getSpec().equals(CAPITAL_ONE_ICON_URL)) {
            return true;
        }

        return false;
    }

    /**
     * If {@code showCustomIcon} is true, and the {@code cardArtUrl} is valid, it fetches the bitmap
     * of the required size from {@code imageFetcher}. If not, the default icon {@code
     * defaultIconId} is fetched from the resources. If the bitmap is not available in cache, then
     * it is fetched from the server and stored in cache for the next time.
     *
     * @param context Context required to get resources.
     * @param imageFetcher The {@link AutofillImageFetcher} associated with the profile.
     * @param cardArtUrl The URL to fetch the icon.
     * @param defaultIconId Resource Id for the default (network) icon if the card art could not be
     *     retrieved.
     * @param cardIconSize Enum that specifies the icon's size (small or large).
     * @param showCustomIcon If true, custom card icon is fetched, else, default icon is fetched.
     * @return {@link Drawable} that can be set as the card icon. If neither the custom icon nor the
     *     default icon is available, returns null.
     */
    public static @Nullable Drawable getCardIcon(
            Context context,
            AutofillImageFetcher imageFetcher,
            @Nullable GURL cardArtUrl,
            int defaultIconId,
            @ImageSize int cardIconSize,
            boolean showCustomIcon) {
        Drawable defaultIcon =
                defaultIconId == 0 ? null : AppCompatResources.getDrawable(context, defaultIconId);
        if (!showCustomIcon || cardArtUrl == null || !cardArtUrl.isValid()) {
            return defaultIcon;
        }

        if (cardArtUrl.getSpec().equals(CAPITAL_ONE_ICON_URL)) {
            return AppCompatResources.getDrawable(context, R.drawable.capitalone_metadata_card);
        }

        Optional<Bitmap> customIconBitmap =
                imageFetcher.getImageIfAvailable(
                        cardArtUrl,
                        IconSpecs.create(context, ImageType.CREDIT_CARD_ART_IMAGE, cardIconSize));
        if (!customIconBitmap.isPresent()) {
            return defaultIcon;
        }

        return new BitmapDrawable(context.getResources(), customIconBitmap.get());
    }

    /**
     * If {@code imageUrl} is valid, it fetches the bitmap of the required size from {@link
     * AutofillImageFetcher}. Otherwise @{code null} drawable is returned.
     *
     * @param context Context required to get resources.
     * @param imageFetcher The {@link AutofillImageFetcher} associated with the profile.
     * @param imageUrl The URL to fetch the icon.
     * @param imageSize Enum that specifies the icon's size (small or large).
     * @return {@link Drawable} that can be set as the card icon.
     */
    public static @Nullable Drawable getValuableIcon(
            Context context,
            AutofillImageFetcher imageFetcher,
            GURL imageUrl,
            @ImageSize int imageSize) {
        Optional<Bitmap> customIconBitmap =
                imageFetcher.getImageIfAvailable(
                        imageUrl, IconSpecs.create(context, ImageType.VALUABLE_IMAGE, imageSize));

        if (!customIconBitmap.isPresent()) {
            return null;
        }
        return new BitmapDrawable(context.getResources(), customIconBitmap.get());
    }

    /**
     * Resize the bitmap to the required specs, round corners, and add grey border.
     *
     * @param bitmap to be updated.
     * @param iconSpecs {@link IconSpecs} instance containing the specs for the card icon.
     * @param addRoundedCornersAndGreyBorder If true, the bitmap corners are rounded, and a grey
     *     border is added. If false, no enhancements are applied to the bitmap.
     * @return Resized {@link Bitmap} with rounded corners and grey border.
     */
    public static Bitmap resizeAndAddRoundedCornersAndGreyBorder(
            Bitmap bitmap, IconSpecs iconSpecs, boolean addRoundedCornersAndGreyBorder) {
        // The server maintains the card art image's aspect ratio, so the fetched image might not be
        // the exact required size. Scale the icon to the desired dimension.
        if (bitmap.getWidth() != iconSpecs.getWidth()
                || bitmap.getHeight() != iconSpecs.getHeight()) {
            bitmap =
                    Bitmap.createScaledBitmap(
                            bitmap,
                            iconSpecs.getWidth(),
                            iconSpecs.getHeight(),
                            /* filter= */ true);
        }

        if (!addRoundedCornersAndGreyBorder) {
            return bitmap;
        }

        Context context = ContextUtils.getApplicationContext();

        // Square logos have their corners rounded off, and then placed in a rectangular white
        // background of size `ImageSize.LARGE`. The rectangular composite asset further has its
        // corners rounded, and outlined with a grey border similar to other rectangular assets.
        if (iconSpecs.getWidth() == iconSpecs.getHeight()) {
            Bitmap squareBitmap =
                    Bitmap.createBitmap(
                            bitmap.getWidth(), bitmap.getHeight(), Bitmap.Config.ARGB_8888);
            Canvas squareCanvas = new Canvas(squareBitmap);
            Paint squarePaint = new Paint();
            squarePaint.setAntiAlias(true);
            RectF squareRectF = new RectF(0, 0, bitmap.getWidth(), bitmap.getHeight());
            squareCanvas.drawRoundRect(
                    squareRectF,
                    iconSpecs.getCornerRadius(),
                    iconSpecs.getCornerRadius(),
                    squarePaint);
            squarePaint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.SRC_IN));
            squareCanvas.drawBitmap(bitmap, 0, 0, squarePaint);

            IconSpecs backgroundSpecs =
                    IconSpecs.create(context, ImageType.CREDIT_CARD_ART_IMAGE, ImageSize.LARGE);
            Bitmap backgroundBitmap =
                    Bitmap.createBitmap(
                            backgroundSpecs.getWidth(),
                            backgroundSpecs.getHeight(),
                            Bitmap.Config.ARGB_8888);
            Canvas backgroundCanvas = new Canvas(backgroundBitmap);
            Paint backgroundPaint = new Paint();
            backgroundPaint.setColor(Color.WHITE);
            backgroundPaint.setAntiAlias(true);
            backgroundCanvas.drawRect(
                    0, 0, backgroundSpecs.getWidth(), backgroundSpecs.getHeight(), backgroundPaint);
            int left = (backgroundSpecs.getWidth() - bitmap.getWidth()) / 2;
            int top = (backgroundSpecs.getHeight() - bitmap.getHeight()) / 2;
            backgroundCanvas.drawBitmap(squareBitmap, left, top, null);

            // It can now be treated as a rectangular image asset, and enhancements can be applied.
            bitmap = backgroundBitmap;
            iconSpecs = backgroundSpecs;
        }

        // Round the corners.
        float cornerRadius = iconSpecs.getCornerRadius();
        Bitmap bitmapWithEnhancements =
                Bitmap.createBitmap(bitmap.getWidth(), bitmap.getHeight(), Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmapWithEnhancements);
        Paint paint = new Paint();
        paint.setAntiAlias(true);
        Rect rect = new Rect(0, 0, bitmap.getWidth(), bitmap.getHeight());
        RectF rectF = new RectF(rect);
        canvas.drawRoundRect(rectF, cornerRadius, cornerRadius, paint);
        paint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.SRC_IN));
        canvas.drawBitmap(bitmap, rect, rect, paint);

        // Add the grey border.
        int greyColor = ContextCompat.getColor(context, R.color.baseline_neutral_variant_90);
        paint.setColor(greyColor);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(iconSpecs.getBorderWidth());
        canvas.drawRoundRect(rectF, cornerRadius, cornerRadius, paint);

        return bitmapWithEnhancements;
    }

    /**
     * Adds credit card details in the card details section.
     *
     * @param context to get the resources.
     * @param imageFetcher The {@link AutofillImageFetcher} associated with the profile.
     * @param parentView View that contains the card details section.
     * @param cardName Card's nickname/product name/network name.
     * @param cardNumber Card's obfuscated last 4 digits.
     * @param cardLabel Card's label.
     * @param cardArtUrl URL to fetch custom card art.
     * @param defaultIconId Resource Id for the default (network) icon if the card art doesn't exist
     *     or couldn't be retrieved.
     * @param cardIconSize Enum that specifies the icon's size (small or large).
     * @param iconEndMarginId Resource Id for the margin between the icon and the card details
     *     section.
     * @param cardNameAndNumberTextAppearance Text appearance Id for the card name and the card
     *     number.
     * @param cardLabelTextAppearance Text appearance Id for the card label.
     * @param showCustomIcon If true, custom card icon is shown, else, default icon is shown.
     */
    public static void addCardDetails(
            Context context,
            AutofillImageFetcher imageFetcher,
            View parentView,
            String cardName,
            String cardNumber,
            String cardLabel,
            GURL cardArtUrl,
            int defaultIconId,
            @ImageSize int cardIconSize,
            int iconEndMarginId,
            int cardNameAndNumberTextAppearance,
            int cardLabelTextAppearance,
            boolean showCustomIcon) {
        ImageView cardIconView = parentView.findViewById(R.id.card_icon);
        cardIconView.setImageDrawable(
                getCardIcon(
                        context,
                        imageFetcher,
                        cardArtUrl,
                        defaultIconId,
                        cardIconSize,
                        showCustomIcon));

        // Set margin between the card icon and the card details.
        MarginLayoutParams params = (MarginLayoutParams) cardIconView.getLayoutParams();
        params.setMarginEnd(context.getResources().getDimensionPixelSize(iconEndMarginId));

        TextView cardNameView = parentView.findViewById(R.id.card_name);
        cardNameView.setText(cardName);
        cardNameView.setTextAppearance(cardNameAndNumberTextAppearance);

        TextView cardNumberView = parentView.findViewById(R.id.card_number);
        cardNumberView.setText(cardNumber);
        cardNumberView.setTextAppearance(cardNameAndNumberTextAppearance);

        TextView cardLabelView = parentView.findViewById(R.id.card_label);
        cardLabelView.setText(cardLabel);
        cardLabelView.setTextAppearance(cardLabelTextAppearance);
    }

    public static int getInputTypeForField(@FieldType int type) {
        switch (type) {
            case FieldType.NAME_FULL:
                return InputType.TYPE_CLASS_TEXT
                        | InputType.TYPE_TEXT_FLAG_CAP_WORDS
                        | InputType.TYPE_TEXT_VARIATION_PERSON_NAME;
            case FieldType.ADDRESS_HOME_SORTING_CODE:
            case FieldType.ADDRESS_HOME_ZIP:
                return InputType.TYPE_CLASS_TEXT
                        | InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS
                        | InputType.TYPE_TEXT_VARIATION_POSTAL_ADDRESS;
            case FieldType.PHONE_HOME_WHOLE_NUMBER:
                // Show the keyboard with numbers and phone-related symbols.
                return InputType.TYPE_CLASS_PHONE;
            case FieldType.ADDRESS_HOME_STREET_ADDRESS:
                return InputType.TYPE_CLASS_TEXT
                        | InputType.TYPE_TEXT_FLAG_CAP_WORDS
                        | InputType.TYPE_TEXT_FLAG_MULTI_LINE
                        | InputType.TYPE_TEXT_VARIATION_POSTAL_ADDRESS;
            case FieldType.EMAIL_ADDRESS:
                return InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS;
            default:
                return InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_CAP_WORDS;
        }
    }

    /**
     * Sets the touch event filter on the provided `view` so that touch events are ignored if
     * something is drawn on top of the `view`. This is done to mitigate the clickjacking attacks.
     *
     * @param view The view to set the touch event filter on.
     */
    @SuppressLint("ClickableViewAccessibility")
    public static void setFilterTouchForSecurity(View view) {
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.AUTOFILL_ENABLE_SECURITY_TOUCH_EVENT_FILTERING_ANDROID)) {
            return;
        }
        view.setFilterTouchesWhenObscured(true);
        view.setOnTouchListener(
                (View v, MotionEvent ev) ->
                        (ev.getFlags() & MotionEvent.FLAG_WINDOW_IS_PARTIALLY_OBSCURED) != 0);
    }

    /**
     * Returns the link to open the payment method management page for a specific instrument on the
     * Google Wallet website.
     *
     * @param instrumentId Instrument id of the payment method.
     * @return URL to manage the payment method on Google Wallet.
     */
    public static String getManagePaymentMethodUrlForInstrumentId(long instrumentId) {
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.USE_SANDBOX_WALLET_ENVIRONMENT)) {
            return MANAGE_SANDBOX_PAYMENT_METHODS_URL + "&id=" + instrumentId;
        }
        return MANAGE_PAYMENT_METHODS_URL + "&id=" + instrumentId;
    }
}
