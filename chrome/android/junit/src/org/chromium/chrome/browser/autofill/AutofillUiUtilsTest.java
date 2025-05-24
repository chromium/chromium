// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Bitmap;
import android.text.SpannableStringBuilder;
import android.widget.EditText;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.autofill.AutofillUiUtils.ErrorType;
import org.chromium.chrome.browser.autofill.AutofillUiUtils.IconSpecs;
import org.chromium.components.autofill.ImageSize;
import org.chromium.components.autofill.ImageType;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.Calendar;

/** Tests the AutofillUiUtils's java code. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AutofillUiUtilsTest {
    private Context mContext;
    private EditText mMonthInput;
    private EditText mYearInput;
    private int mThisMonth;
    private int mTwoDigitThisYear;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        mMonthInput = new EditText(mContext);
        mYearInput = new EditText(mContext);
        mThisMonth = Calendar.getInstance().get(Calendar.MONTH) + 1;
        mTwoDigitThisYear = Calendar.getInstance().get(Calendar.YEAR) % 100;
    }

    @Test
    @SmallTest
    @SuppressLint("SetTextI18n")
    public void testExpirationDateErrorWithInvalidMonthReturnsExpirationMonthErrorType() {
        mMonthInput.setText("20");
        mYearInput.setText(String.valueOf(mTwoDigitThisYear));

        int errorType = getExpirationDateErrorForUserEnteredMonthAndYear();

        Assert.assertEquals(ErrorType.EXPIRATION_MONTH, errorType);
    }

    @Test
    @SmallTest
    public void testExpirationDateErrorWithInvalidYearReturnsExpirationYearErrorType() {
        mMonthInput.setText(String.valueOf(mThisMonth));
        mYearInput.setText(String.valueOf(mTwoDigitThisYear - 1));

        int errorType = getExpirationDateErrorForUserEnteredMonthAndYear();

        Assert.assertEquals(ErrorType.EXPIRATION_YEAR, errorType);
    }

    @Test
    @SmallTest
    public void testExpirationDateErrorWithInvalidFutureYearReturnsExpirationYearErrorType() {
        mMonthInput.setText(String.valueOf(mThisMonth));
        mYearInput.setText(String.valueOf(mTwoDigitThisYear + 21));

        int errorType = getExpirationDateErrorForUserEnteredMonthAndYear();

        Assert.assertEquals(ErrorType.EXPIRATION_YEAR, errorType);
    }

    @Test
    @SmallTest
    public void testExpirationDateErrorWithCurrentYearAndCurrentMonthReturnsNoneErrorType() {
        mMonthInput.setText(String.valueOf(mThisMonth));
        mYearInput.setText(String.valueOf(mTwoDigitThisYear));

        int errorType = getExpirationDateErrorForUserEnteredMonthAndYear();

        Assert.assertEquals(ErrorType.NONE, errorType);
    }

    @Test
    @SmallTest
    @SuppressLint("SetTextI18n")
    public void
            testExpirationDateErrorWithEditingMonthAndNotFocusedYearReturnsNotEnoughInfoErrorType() {
        mMonthInput.setText("1");
        mYearInput.setText(String.valueOf(""));
        mMonthInput.requestFocus(); // currently being edited

        int errorType =
                AutofillUiUtils.getExpirationDateErrorType(
                        mMonthInput,
                        mYearInput,
                        /* didFocusOnMonth= */ true,
                        /* didFocusOnYear= */ false);

        Assert.assertEquals(ErrorType.NOT_ENOUGH_INFO, errorType);
    }

    @Test
    @SmallTest
    @SuppressLint("SetTextI18n")
    public void
            testExpirationDateErrorWithEditingMonthAndFocusedInvalidYearReturnsExpirationYearErrorType() {
        mMonthInput.setText("1");
        mYearInput.setText(String.valueOf(""));
        mMonthInput.requestFocus(); // currently being edited

        int errorType =
                AutofillUiUtils.getExpirationDateErrorType(
                        mMonthInput,
                        mYearInput,
                        /* didFocusOnMonth= */ true,
                        /* didFocusOnYear= */ true);

        Assert.assertEquals(ErrorType.EXPIRATION_YEAR, errorType);
    }

    @Test
    @SmallTest
    @SuppressLint("SetTextI18n")
    public void
            testExpirationDateErrorWithValidMonthAndIncompleteYearReturnsNotEnoughInfoErrorType() {
        mMonthInput.setText(String.valueOf(mThisMonth));
        mYearInput.setText("1");
        mYearInput.requestFocus(); // currently being edited

        int errorType = getExpirationDateErrorForUserEnteredMonthAndYear();

        Assert.assertEquals(ErrorType.NOT_ENOUGH_INFO, errorType);
    }

    @Test
    @SmallTest
    @SuppressLint("SetTextI18n")
    public void testExpirationDateErrorWithValidMonthAndValidYearReturnsNoneErrorType() {
        mMonthInput.setText(String.valueOf(mThisMonth));
        mYearInput.setText(String.valueOf(mTwoDigitThisYear + 1));

        int errorType = getExpirationDateErrorForUserEnteredMonthAndYear();

        Assert.assertEquals(ErrorType.NONE, errorType);
    }

    @Test
    @SmallTest
    @SuppressLint("SetTextI18n")
    public void testExpirationDateErrorWithMonthBeingEditedAndValidYearReturnsNotEnoughInfo() {
        mMonthInput.setText("");
        mMonthInput.requestFocus();
        mYearInput.setText(String.valueOf(mTwoDigitThisYear + 1));

        int errorType = getExpirationDateErrorForUserEnteredMonthAndYear();

        Assert.assertEquals(ErrorType.NOT_ENOUGH_INFO, errorType);
    }

    @Test
    @SmallTest
    @SuppressLint("SetTextI18n")
    public void testExpirationDateErrorWithMonthSetToZeroAndValidYearReturnsNotEnoughInfo() {
        mMonthInput.setText("0");
        mMonthInput.requestFocus();
        mYearInput.setText(String.valueOf(mTwoDigitThisYear + 1));

        int errorType = getExpirationDateErrorForUserEnteredMonthAndYear();

        Assert.assertEquals(ErrorType.NOT_ENOUGH_INFO, errorType);
    }

    @Test
    @SmallTest
    @SuppressLint("SetTextI18n")
    public void testGetMonthWithNonNumericInputReturnsNegativeOne() {
        mMonthInput.setText("MM");

        int month = AutofillUiUtils.getMonth(mMonthInput);

        Assert.assertEquals(-1, month);
    }

    @Test
    @SmallTest
    @SuppressLint("SetTextI18n")
    public void testGetMonthWithNegativeNumberInputReturnsNegativeOne() {
        mMonthInput.setText("-20");

        int month = AutofillUiUtils.getMonth(mMonthInput);

        Assert.assertEquals(-1, month);
    }

    @Test
    @SmallTest
    @SuppressLint("SetTextI18n")
    public void testGetMonthWithZeroAsInputReturnsNegativeOne() {
        mMonthInput.setText("0");

        int month = AutofillUiUtils.getMonth(mMonthInput);

        Assert.assertEquals(-1, month);
    }

    @Test
    @SmallTest
    @SuppressLint("SetTextI18n")
    public void testGetMonthWithThirteenAsInputReturnsNegativeOne() {
        mMonthInput.setText("13");

        int month = AutofillUiUtils.getMonth(mMonthInput);

        Assert.assertEquals(-1, month);
    }

    @Test
    @SmallTest
    @SuppressLint("SetTextI18n")
    public void testGetFourDigitYearWithNonNumericInputReturnsNegativeOne() {
        mYearInput.setText("YY");

        int year = AutofillUiUtils.getMonth(mYearInput);

        Assert.assertEquals(-1, year);
    }

    @Test
    @SmallTest
    @SuppressLint("SetTextI18n")
    public void testGetFourDigitYearWithNegativeNumberInputReturnsNegativeOne() {
        mYearInput.setText("-20");

        int fourDigitYear = AutofillUiUtils.getFourDigitYear(mYearInput);

        Assert.assertEquals(-1, fourDigitYear);
    }

    @Test
    @SmallTest
    public void testGetFourDigitYearForCurrentTwoDigitYearReturnsCurrentFourDigitYear() {
        // Set the edit text value to be the current year in YY format.
        mYearInput.setText(String.valueOf(mTwoDigitThisYear));

        int fourDigitYear = AutofillUiUtils.getFourDigitYear(mYearInput);

        Assert.assertEquals(Calendar.getInstance().get(Calendar.YEAR), fourDigitYear);
    }

    @Test
    @SmallTest
    public void testGetFourDigitYearForPreviousYearReturnsNegativeOne() {
        // Set the edit text value to be the current year in YY format.
        mYearInput.setText(String.valueOf(mTwoDigitThisYear - 1));

        int fourDigitYear = AutofillUiUtils.getFourDigitYear(mYearInput);

        Assert.assertEquals(-1, fourDigitYear);
    }

    @Test
    @SmallTest
    public void testGetFourDigitYearForTenYearsFromNowReturnsValidFourDigitYear() {
        // Set the edit text value to be the current year in YY format.
        mYearInput.setText(String.valueOf(mTwoDigitThisYear + 10));

        int fourDigitYear = AutofillUiUtils.getFourDigitYear(mYearInput);

        Assert.assertEquals(Calendar.getInstance().get(Calendar.YEAR) + 10, fourDigitYear);
    }

    @Test
    @SmallTest
    public void testGetFourDigitYearForElevenYearsFromNowReturnsNegativeOne() {
        // Set the edit text value to be the current year in YY format.
        mYearInput.setText(String.valueOf(mTwoDigitThisYear + 11));

        int fourDigitYear = AutofillUiUtils.getFourDigitYear(mYearInput);

        Assert.assertEquals(-1, fourDigitYear);
    }

    @Test
    @SmallTest
    public void testSpannableStringForLegalMessageLinesAddsNewLineSeparator() {
        SpannableStringBuilder spannableString =
                AutofillUiUtils.getSpannableStringForLegalMessageLines(
                        /* context= */ null,
                        Arrays.asList(new LegalMessageLine("line1"), new LegalMessageLine("line2")),
                        /* underlineLinks= */ false,
                        /* onClickCallback= */ null);

        Assert.assertEquals("line1\nline2", spannableString.toString());
    }

    @Test
    @SmallTest
    public void testResizeAndAddRoundedCornersAndGreyBorder() {
        Bitmap testImage = Bitmap.createBitmap(400, 300, Bitmap.Config.ARGB_8888);
        IconSpecs testSpecs =
                IconSpecs.create(
                        ContextUtils.getApplicationContext(),
                        ImageType.CREDIT_CARD_ART_IMAGE,
                        ImageSize.LARGE);

        Bitmap resizedTestImage =
                AutofillUiUtils.resizeAndAddRoundedCornersAndGreyBorder(
                        testImage, testSpecs, /* addRoundedCornersAndGreyBorder= */ true);

        // Verify that the image gets resized to required dimensions. We can't verify other
        // enhancements like border and corner-radius because they are not properties of the bitmap
        // image.
        Assert.assertEquals(resizedTestImage.getWidth(), testSpecs.getWidth());
        Assert.assertEquals(resizedTestImage.getHeight(), testSpecs.getHeight());
    }

    @Test
    @SmallTest
    public void testCreditCardIconSpec() {
        Context context = ContextUtils.getApplicationContext();
        IconSpecs specs =
                IconSpecs.create(context, ImageType.CREDIT_CARD_ART_IMAGE, ImageSize.LARGE);

        Assert.assertEquals(
                context.getResources().getDimensionPixelSize(R.dimen.large_card_icon_width),
                specs.getWidth());
        Assert.assertEquals(
                context.getResources().getDimensionPixelSize(R.dimen.large_card_icon_height),
                specs.getHeight());
        Assert.assertEquals(
                context.getResources().getDimensionPixelSize(R.dimen.large_card_icon_corner_radius),
                specs.getCornerRadius());
        Assert.assertEquals(
                context.getResources().getDimensionPixelSize(R.dimen.card_icon_border_width),
                specs.getBorderWidth());
    }

    @Test
    @SmallTest
    public void testValuableIconSpec() {
        Context context = ContextUtils.getApplicationContext();
        IconSpecs specs = IconSpecs.create(context, ImageType.VALUABLE_IMAGE, ImageSize.LARGE);

        Assert.assertEquals(
                specs.getWidth(),
                context.getResources().getDimensionPixelSize(R.dimen.large_valuable_icon_size));
        Assert.assertEquals(
                specs.getHeight(),
                context.getResources().getDimensionPixelSize(R.dimen.large_valuable_icon_size));
        Assert.assertEquals(0, specs.getCornerRadius());
        Assert.assertEquals(0, specs.getBorderWidth());
    }

    @Test
    @SmallTest
    public void testVirtualCardShowsCapitalOneVirtualCardIcon() {
        Assert.assertTrue(
                AutofillUiUtils.shouldShowCustomIcon(
                        new GURL(AutofillUiUtils.CAPITAL_ONE_ICON_URL), /* isVirtualCard= */ true));
    }

    @Test
    @SmallTest
    public void testNonVirtualCardDoesNotShowCapitalOneVirtualCardIcon() {
        Assert.assertFalse(
                AutofillUiUtils.shouldShowCustomIcon(
                        new GURL(AutofillUiUtils.CAPITAL_ONE_ICON_URL),
                        /* isVirtualCard= */ false));
    }

    @Test
    @SmallTest
    public void testBothVirtualAndNonVirtualCardsShowRichCardArt() {
        Assert.assertTrue(
                AutofillUiUtils.shouldShowCustomIcon(
                        new GURL("https://www.richcardart.com/richcardart.png"),
                        /* isVirtualCard= */ false));
        Assert.assertTrue(
                AutofillUiUtils.shouldShowCustomIcon(
                        new GURL("https://www.richcardart.com/richcardart.png"),
                        /* isVirtualCard= */ true));
    }

    private @ErrorType int getExpirationDateErrorForUserEnteredMonthAndYear() {
        return AutofillUiUtils.getExpirationDateErrorType(
                mMonthInput, mYearInput, /* didFocusOnMonth= */ true, /* didFocusOnYear= */ true);
    }
}
