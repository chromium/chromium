// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.text.Editable;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.ui.text.EmptyTextWatcher;

/**
 * Android wrapper of i18n::phonenumbers::PhoneNumberUtil which provides convenient methods to
 * format and validate phone number.
 */
@JNINamespace("autofill")
public class PhoneNumberUtil {
    // Avoid instantiation by accident.
    private PhoneNumberUtil() {}

    /** TextWatcher to watch phone number changes so as to format it based on country code. */
    public static class CountryAwareFormatTextWatcher implements EmptyTextWatcher {
        /** Indicates the change was caused by ourselves. */
        private boolean mSelfChange;

        @Nullable private String mCountryCode;

        /**
         * Updates the country code used to format phone numbers.
         *
         * @param countryCode The given country code.
         */
        public void setCountryCode(@Nullable String countryCode) {
            mCountryCode = countryCode;
        }

        @Override
        public void afterTextChanged(Editable s) {
            if (mSelfChange) return;

            String formattedNumber = formatForDisplay(s.toString(), mCountryCode);
            mSelfChange = true;
            s.replace(0, s.length(), formattedNumber, 0, formattedNumber.length());
            mSelfChange = false;
        }
    }

    /**
     * Formats the given phone number in INTERNATIONAL format
     * [i18n::phonenumbers::PhoneNumberUtil::PhoneNumberFormat::INTERNATIONAL] based
     * on region code, returning the original number if no formatting can be made.
     * For example, the number of the Google Zürich office will be formatted as
     * "+41 44 668 1800" in INTERNATIONAL format.
     *
     * Note that the region code is from the given phone number if it starts with
     * '+', otherwise the region code is deduced from the given country code or
     * from application locale if the given country code is null.
     *
     * @param phoneNumber The given phone number.
     * @param countryCode The given country code.
     * @return Formatted phone number.
     */
    public static String formatForDisplay(String phoneNumber, @Nullable String countryCode) {
        return PhoneNumberUtilJni.get().formatForDisplay(phoneNumber, countryCode);
    }

    /**
     * Formats the given phone number in E.164 format as specified in the Payment Request spec
     * (https://w3c.github.io/browser-payment-api/#paymentrequest-updated-algorithm)
     * [i18n::phonenumbers::PhoneNumberUtil::PhoneNumberFormat::E164], returning the original number
     * if no formatting can be made. For example, the number of the Google Zürich office will be
     * formatted as "+41446681800" in E.164 format.
     *
     * @param phoneNumber The given phone number.
     * @return Formatted phone number.
     */
    public static String formatForResponse(String phoneNumber) {
        return PhoneNumberUtilJni.get().formatForResponse(phoneNumber);
    }

    /**
     * Checks whether the given phone number is a possible number according to
     * region code.
     * The region code is from the given phone number if it starts with '+',
     * otherwise the region code is deduced from the given country code or from
     * application locale if the given country code is null.
     *
     * @param phoneNumber The given phone number.
     * @param countryCode The given country code.
     *
     * @return True if the given number is a possible number, otherwise return false.
     */
    public static boolean isPossibleNumber(String phoneNumber, @Nullable String countryCode) {
        return PhoneNumberUtilJni.get().isPossibleNumber(phoneNumber, countryCode);
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        String formatForDisplay(String phoneNumber, String countryCode);

        String formatForResponse(String phoneNumber);

        boolean isPossibleNumber(String phoneNumber, String countryCode);
    }
}
