// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.autofill.editors.EditorProperties.DropdownKeyValue;
import org.chromium.components.autofill.FieldType;

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
    private String mCurrentBestLanguageCode;

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

    /** Description of an address editor input field. */
    public static class AutofillAddressUiComponent {
        /** The type of the field, e.g., FieldType.NAME_FULL. */
        public final @FieldType int id;

        /** The localized display label for the field, e.g., "City." */
        public final String label;

        /** Whether the field is required. */
        public final boolean isRequired;

        /** Whether the field takes up the full line.*/
        public final boolean isFullLine;

        /**
         * Builds a description of an address editor input field.
         *
         * @param id The type of the field, .e.g., FieldType.ADDRESS_HOME_CITY.
         * @param label The localized display label for the field, .e.g., "City."
         * @param isRequired Whether the field is required.
         * @param isFullLine Whether the field takes up the full line.
         */
        public AutofillAddressUiComponent(
                int id, String label, boolean isRequired, boolean isFullLine) {
            this.id = id;
            this.label = label;
            this.isRequired = isRequired;
            this.isFullLine = isFullLine;
        }
    }

    /**
     * Returns the UI components for the CLDR countryCode and languageCode provided. If no language
     * code is provided, the application's default locale is used instead. Also stores the
     * currentBestLanguageCode, retrievable via getCurrentBestLanguageCode, to be used when saving
     * an autofill profile.
     *
     * @param countryCode The CLDR code used to retrieve address components.
     * @param languageCode The language code associated with the saved autofill profile that ui
     *                     components are being retrieved for; can be null if ui components are
     *                     being retrieved for a new profile.
     * @param validationType The target usage validation rules.
     * @return A list of address UI components. The ordering in the list specifies the order these
     *         components should appear in the UI.
     */
    public List<AutofillAddressUiComponent> getAddressUiComponents(
            String countryCode, String languageCode, @AddressValidationType int validationType) {
        List<Integer> componentIds = new ArrayList<>();
        List<String> componentNames = new ArrayList<>();
        List<Integer> componentRequired = new ArrayList<>();
        List<Integer> componentLengths = new ArrayList<>();
        List<AutofillAddressUiComponent> uiComponents = new ArrayList<>();

        mCurrentBestLanguageCode =
                AutofillProfileBridgeJni.get()
                        .getAddressUiComponents(
                                countryCode,
                                languageCode,
                                validationType,
                                componentIds,
                                componentNames,
                                componentRequired,
                                componentLengths);

        for (int i = 0; i < componentIds.size(); i++) {
            uiComponents.add(
                    new AutofillAddressUiComponent(
                            componentIds.get(i),
                            componentNames.get(i),
                            componentRequired.get(i) == 1,
                            componentLengths.get(i) == 1));
        }

        return uiComponents;
    }

    /**
     * @return The language code associated with the most recently retrieved address ui components.
     *         Will return null if getAddressUiComponents() has not been called yet.
     */
    public String getCurrentBestLanguageCode() {
        return mCurrentBestLanguageCode;
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
        String getDefaultCountryCode();

        void getSupportedCountries(List<String> countryCodes, List<String> countryNames);

        void getRequiredFields(String countryCode, List<Integer> requiredFields);

        String getAddressUiComponents(
                String countryCode,
                String languageCode,
                @AddressValidationType int validationType,
                List<Integer> componentIds,
                List<String> componentNames,
                List<Integer> componentRequired,
                List<Integer> componentLengths);
    }
}
