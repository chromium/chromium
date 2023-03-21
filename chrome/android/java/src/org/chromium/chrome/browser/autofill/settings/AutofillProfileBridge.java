// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.app.Activity;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.components.autofill.prefeditor.EditorFieldModel;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.content_public.browser.WebContents;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;
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
     * Address field types.
     * This list must be kept in-sync with the corresponding enum in
     * third_party/libaddressinput/src/cpp/include/libaddressinput/address_field.h
     */
    @IntDef({AddressField.COUNTRY, AddressField.ADMIN_AREA, AddressField.LOCALITY,
            AddressField.DEPENDENT_LOCALITY, AddressField.SORTING_CODE, AddressField.POSTAL_CODE,
            AddressField.STREET_ADDRESS, AddressField.ORGANIZATION, AddressField.RECIPIENT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface AddressField {
        int COUNTRY = 0;
        int ADMIN_AREA = 1;
        int LOCALITY = 2;
        int DEPENDENT_LOCALITY = 3;
        int SORTING_CODE = 4;
        int POSTAL_CODE = 5;
        int STREET_ADDRESS = 6;
        int ORGANIZATION = 7;
        int RECIPIENT = 8;

        int NUM_ENTRIES = 9;
    }

    private String mCurrentBestLanguageCode;

    /**
     * @return The CLDR region code for the default locale.
     */
    public static String getDefaultCountryCode() {
        return AutofillProfileBridgeJni.get().getDefaultCountryCode();
    }

    /** @return The list of supported countries sorted by their localized display names. */
    public static List<EditorFieldModel.DropdownKeyValue> getSupportedCountries() {
        List<String> countryCodes = new ArrayList<>();
        List<String> countryNames = new ArrayList<>();
        List<EditorFieldModel.DropdownKeyValue> countries = new ArrayList<>();

        AutofillProfileBridgeJni.get().getSupportedCountries(countryCodes, countryNames);

        for (int i = 0; i < countryCodes.size(); i++) {
            countries.add(new EditorFieldModel.DropdownKeyValue(
                    countryCodes.get(i), countryNames.get(i)));
        }

        final Collator collator = Collator.getInstance(Locale.getDefault());
        collator.setStrength(Collator.PRIMARY);
        Collections.sort(countries, new Comparator<EditorFieldModel.DropdownKeyValue>() {
            @Override
            public int compare(
                    EditorFieldModel.DropdownKeyValue lhs, EditorFieldModel.DropdownKeyValue rhs) {
                int result = collator.compare(lhs.getValue(), rhs.getValue());
                if (result == 0) result = lhs.getKey().compareTo(rhs.getKey());
                return result;
            }
        });
        return countries;
    }

    /** @return The list of admin areas sorted by their localized display names. */
    public static List<EditorFieldModel.DropdownKeyValue> getAdminAreaDropdownList(
            String[] adminAreaCodes, String[] adminAreaNames) {
        List<EditorFieldModel.DropdownKeyValue> adminAreas = new ArrayList<>();

        for (int i = 0; i < adminAreaCodes.length; ++i) {
            adminAreas.add(
                    new EditorFieldModel.DropdownKeyValue(adminAreaCodes[i], adminAreaNames[i]));
        }

        final Collator collator = Collator.getInstance(Locale.getDefault());
        collator.setStrength(Collator.PRIMARY);
        Collections.sort(adminAreas, new Comparator<EditorFieldModel.DropdownKeyValue>() {
            @Override
            public int compare(
                    EditorFieldModel.DropdownKeyValue lhs, EditorFieldModel.DropdownKeyValue rhs) {
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
     * Description of an address editor input field.
     */
    public static class AddressUiComponent {
        /** The type of the field, e.g., AddressField.LOCALITY. */
        public final int id;

        /** The localized display label for the field, e.g., "City." */
        public final String label;

        /** Whether the field is required. */
        public final boolean isRequired;

        /** Whether the field takes up the full line.*/
        public final boolean isFullLine;

        /**
         * Builds a description of an address editor input field.
         *
         * @param id         The type of the field, .e.g., AddressField.LOCALITY.
         * @param label      The localized display label for the field, .e.g., "City."
         * @param isRequired Whether the field is required.
         * @param isFullLine Whether the field takes up the full line.
         */
        public AddressUiComponent(int id, String label, boolean isRequired, boolean isFullLine) {
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
    public List<AddressUiComponent> getAddressUiComponents(
            String countryCode, String languageCode, @AddressValidationType int validationType) {
        List<Integer> componentIds = new ArrayList<>();
        List<String> componentNames = new ArrayList<>();
        List<Integer> componentRequired = new ArrayList<>();
        List<Integer> componentLengths = new ArrayList<>();
        List<AddressUiComponent> uiComponents = new ArrayList<>();

        mCurrentBestLanguageCode = AutofillProfileBridgeJni.get().getAddressUiComponents(
                countryCode, languageCode, validationType, componentIds, componentNames,
                componentRequired, componentLengths);

        for (int i = 0; i < componentIds.size(); i++) {
            uiComponents.add(new AddressUiComponent(componentIds.get(i), componentNames.get(i),
                    componentRequired.get(i) == 1, componentLengths.get(i) == 1));
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

    @CalledByNative
    private static void showAutofillProfileSettings(WebContents webContents) {
        RecordUserAction.record("AutofillAddressesViewed");
        showSettingSubpage(webContents, AutofillProfilesFragment.class);
    }

    @CalledByNative
    private static void showAutofillCreditCardSettings(WebContents webContents) {
        RecordUserAction.record("AutofillCreditCardsViewed");
        showSettingSubpage(webContents, AutofillPaymentMethodsFragment.class);
    }

    private static void showSettingSubpage(
            WebContents webContents, Class<? extends Fragment> fragment) {
        WeakReference<Activity> currentActivity =
                webContents.getTopLevelNativeWindow().getActivity();
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        settingsLauncher.launchSettingsActivity(currentActivity.get(), fragment);
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        String getDefaultCountryCode();
        void getSupportedCountries(List<String> countryCodes, List<String> countryNames);
        void getRequiredFields(String countryCode, List<Integer> requiredFields);
        String getAddressUiComponents(String countryCode, String languageCode,
                @AddressValidationType int validationType, List<Integer> componentIds,
                List<String> componentNames, List<Integer> componentRequired,
                List<Integer> componentLengths);
    }
}
