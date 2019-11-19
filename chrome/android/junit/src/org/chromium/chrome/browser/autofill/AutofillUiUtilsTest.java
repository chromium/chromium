// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.annotation.SuppressLint;
import android.content.Context;
import android.support.test.filters.SmallTest;
import android.widget.EditText;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.autofill.AutofillUiUtils.ErrorType;

import java.util.Calendar;

/**
 * Tests the AutofillUiUtils's java code.
 */
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

        int errorType = AutofillUiUtils.getExpirationDateErrorType(mMonthInput,
                mYearInput, /*didFocusOnMonth=*/
                true, /*didFocusOnYear=*/false);

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

        int errorType = AutofillUiUtils.getExpirationDateErrorType(mMonthInput,
                mYearInput, /*didFocusOnMonth=*/
                true, /*didFocusOnYear=*/true);

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

    @ErrorType
    private int getExpirationDateErrorForUserEnteredMonthAndYear() {
        return AutofillUiUtils.getExpirationDateErrorType(mMonthInput,
                mYearInput, /*didFocusOnMonth=*/
                true, /*didFocusOnYear=*/true);
    }
}
