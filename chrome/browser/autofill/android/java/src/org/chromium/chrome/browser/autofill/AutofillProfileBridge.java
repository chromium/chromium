// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownKeyValue;
import org.chromium.components.autofill.AutofillAddressEditorUiInfo;

import java.text.Collator;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;

/**
 * Static methods to fetch information needed to create the address fields for the autofill profile
 * form.
 */
@JNINamespace("autofill")
public final class AutofillProfileBridge {
    /**
     * @return The CLDR region code for the default locale.
     */
    public static String getDefaultCountryCode() {
        return AutofillProfileBridgeJni.get().getDefaultCountryCode();
    }

    /** @return The list of supported countries sorted by their localized display names. */
    public static List<DropdownKeyValue> getSupportedCountries() {
        List<String> countryCodes = new ArrayList<>();
        List<String> countryNames = new ArrayList<>();
        List<DropdownKeyValue> countries = new ArrayList<>();

        AutofillProfileBridgeJni.get().getSupportedCountries(countryCodes, countryNames);

        for (int i = 0; i < countryCodes.size(); i++) {
            countries.add(new DropdownKeyValue(countryCodes.get(i), countryNames.get(i)));
        }

        final Collator collator = Collator.getInstance(Locale.getDefault());
        collator.setStrength(Collator.PRIMARY);
        Collections.sort(
                countries,
                new Comparator<DropdownKeyValue>() {
                    @Override
                    public int compare(DropdownKeyValue lhs, DropdownKeyValue rhs) {
                        int result = collator.compare(lhs.getValue(), rhs.getValue());
                        if (result == 0) result = lhs.getKey().compareTo(rhs.getKey());
                        return result;
                    }
                });
        return countries;
    }

    /** @return The list of admin areas sorted by their localized display names. */
    public static List<DropdownKeyValue> getAdminAreaDropdownList(
            String[] adminAreaCodes, String[] adminAreaNames) {
        List<DropdownKeyValue> adminAreas = new ArrayList<>();

        for (int i = 0; i < adminAreaCodes.length; ++i) {
            adminAreas.add(new DropdownKeyValue(adminAreaCodes[i], adminAreaNames[i]));
        }

        final Collator collator = Collator.getInstance(Locale.getDefault());
        collator.setStrength(Collator.PRIMARY);
        Collections.sort(
                adminAreas,
                new Comparator<DropdownKeyValue>() {
                    @Override
                    public int compare(DropdownKeyValue lhs, DropdownKeyValue rhs) {
                        // Sorted according to the admin area values, such as Quebec,
                        // rather than the admin area keys, such as QC.
                        return collator.compare(lhs.getValue(), rhs.getValue());
                    }
                });
        return adminAreas;
    }

    /** @return The list of required fields. COUNTRY is always included. RECIPIENT often omitted. */
    public static List<Integer> getRequiredAddressFields(String countryCode) {
        List<Integer> requiredFields = new ArrayList<>();
        AutofillProfileBridgeJni.get().getRequiredFields(countryCode, requiredFields);
        return requiredFields;
    }

    /**
     * Returns the UI components for the CLDR countryCode and languageCode provided. If no language
     * code is provided, the application's default locale is used instead.
     *
     * @param countryCode The CLDR code used to retrieve address components.
     * @param languageCode The language code associated with the saved autofill profile that ui
     *     components are being retrieved for; can be null if ui components are being retrieved for
     *     a new profile.
     * @param validationType The target usage validation rules.
     * @return A list of address UI components bundled with the best language tag. The ordering in
     *     the list specifies the order these components should appear in the UI.
     */
    public AutofillAddressEditorUiInfo getAddressEditorUiInfo(
            String countryCode, String languageCode, @AddressValidationType int validationType) {
        return AutofillProfileBridgeJni.get()
                .getAddressEditorUiInfo(countryCode, languageCode, validationType);
    }

    @CalledByNative
    private static void stringArrayToList(String[] array, List<String> list) {
        for (String s : array) {
            list.add(s);
        }
    }

    @CalledByNative
    private static void intArrayToList(int[] array, List<Integer> list) {
        for (int s : array) {
            list.add(s);
        }
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        @JniType("std::string")
        String getDefaultCountryCode();

        void getSupportedCountries(List<String> countryCodes, List<String> countryNames);

        void getRequiredFields(
                @JniType("std::string") String countryCode, List<Integer> requiredFields);

        @JniType("AutofillAddressEditorUiInfoAndroid")
        AutofillAddressEditorUiInfo getAddressEditorUiInfo(
                @JniType("std::string") String countryCode,
                @JniType("std::string") String languageCode,
                @AddressValidationType int validationType);
    }
}
