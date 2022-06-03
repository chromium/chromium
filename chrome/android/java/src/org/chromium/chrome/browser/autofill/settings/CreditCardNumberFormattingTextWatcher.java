// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;

import org.chromium.chrome.browser.autofill.PersonalDataManager;

/**
 * Watch a TextView and if a credit card number is entered, it will format the number.
 * Disable formatting when user:
 * 1. Inputs dashes or spaces.
 * 2. Removes separators in the middle of the string
 * 3. Enters a number longer than 16 digits.
 *
 * Formatting will be re-enabled once text is cleared.
 */
public class CreditCardNumberFormattingTextWatcher implements TextWatcher {
    /** Character for card number section separator. */
    private static final String SEPARATOR = " ";

    private static final int NUMBER_OF_DIGITS = 16;

    /**
     * Whether to format the credit card number. If true, spaces will be inserted
     * automatically between each group of 4 digits in the credit card number as the user types.
     * This is set to false if the user types a dash or space.
     */
    private boolean mFormattingEnabled = true;

    /**
     * Whether the change was caused by ourselves.
     * This is set true when we are manipulating the text of EditText,
     * and all callback functions should check this boolean to avoid infinite recursion.
     */
    private boolean mSelfChange;

    /** Whether the formatting is disabled because the number is too long. */
    private boolean mNumberTooLong;

    @Override
    public void onTextChanged(CharSequence s, int start, int before, int count) {
        if (mSelfChange || !mFormattingEnabled) return;
        // If user enters non-digit characters, do not format.
        if (count > 0 && hasDashOrSpace(s, start, count)) {
            mFormattingEnabled = false;
        }
    }

    @Override
    public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

    @Override
    public void afterTextChanged(Editable s) {
        if (mSelfChange) return;
        mSelfChange = true;

        if (mFormattingEnabled) {
            removeSeparators(s);
            // If number is too long, do not format it and remove all
            // previous separators.
            if (s.length() > NUMBER_OF_DIGITS) {
                mNumberTooLong = true;
                mFormattingEnabled = false;
            } else {
                insertSeparators(s);
            }
        } else if (mNumberTooLong && s.length() <= NUMBER_OF_DIGITS) {
            // If user deletes extra characters, re-enable formatting
            mNumberTooLong = false;
            mFormattingEnabled = true;
            insertSeparators(s);
        }
        // If user clears the input, re-enable formatting
        if (s.length() == 0) mFormattingEnabled = true;

        mSelfChange = false;
    }

    public static void removeSeparators(Editable s) {
        int index = TextUtils.indexOf(s, SEPARATOR);
        while (index >= 0) {
            s.delete(index, index + 1);
            index = TextUtils.indexOf(s, SEPARATOR, index + 1);
        }
    }

    public static void insertSeparators(Editable s) {
        int[] positions;
        if (PersonalDataManager.getInstance()
                        .getBasicCardIssuerNetwork(s.toString(), false)
                        .equals("amex")) {
            positions = new int[2];
            positions[0] = 4;
            positions[1] = 11;
        } else {
            positions = new int[3];
            positions[0] = 4;
            positions[1] = 9;
            positions[2] = 14;
        }
        for (int i : positions) {
            if (s.length() > i) {
                s.insert(i, SEPARATOR);
            }
        }
    }

    public static boolean hasDashOrSpace(final CharSequence s, final int start, final int count) {
        return TextUtils.indexOf(s, " ", start, start + count) != -1
                || TextUtils.indexOf(s, "-", start, start + count) != -1;
    }
}
