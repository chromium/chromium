// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;

import org.jni_zero.CalledByNative;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/** Launches autofill settings subpages. */
@NullMarked
public class SettingsNavigationHelper {
    /**
      * Tries showing the Autofill and passwords settings page.
      *
      * @param context The {@link Context} required to start the settings page. Noop without it.
      * @return True if the context is valid, feature enabled and `startSettings` was called.
      */
    public static boolean showAutofillAndPasswordsSettings(@Nullable Context context) {
        if (context == null || !ChromeFeatureList.isEnabled(
            ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID)) {
            return false;
        }

        RecordUserAction.record("AutofillYourSavedInfoViewed");
        SettingsNavigationFactory.createSettingsNavigation()
                .startSettings(context, HomeOfTransactionsFragment.class);
        return true;
    }

    /**
     * Tries showing the settings page for Identity Docs.
     *
     * @param context The {@link Context} required to start the settings page. Noop without it.
     * @return True if the context is valid and `startSettings` was called.
     */
    public static boolean showAutofillIdentityDocsSettings(@Nullable Context context) {
        if (context == null) {
            return false;
        }
        SettingsNavigationFactory.createSettingsNavigation()
                .startSettings(context, AutofillIdentityDocsFragment.class);
        return true;
    }

    /**
     * Tries showing the settings page for Travel.
     *
     * @param context The {@link Context} required to start the settings page. Noop without it.
     * @return True iff the context is valid and `startSettings` was called.
     */
    public static boolean showAutofillTravelSettings(@Nullable Context context) {
        if (context == null) {
            return false;
        }
        SettingsNavigationFactory.createSettingsNavigation()
                .startSettings(context, AutofillTravelFragment.class);
        return true;
    }

    /**
     * Tries showing the settings page for Addresses.
     *
     * @param context The {@link Context} required to start the settings page. Noop without it.
     * @return True iff the context is valid and `startSettings` was called.
     */
    public static boolean showAutofillProfileSettings(@Nullable Context context) {
        if (context == null) {
            return false;
        }
        RecordUserAction.record("AutofillAddressesViewed");
        SettingsNavigationFactory.createSettingsNavigation()
                .startSettings(context, AutofillProfilesFragment.class);
        return true;
    }

    /**
     * Tries showing the settings page for Payments.
     *
     * @param context The {@link Context} required to start the settings page. Noop without it.
     * @return True iff the context is valid and `startSettings` was called.
     */
    public static boolean showAutofillCreditCardSettings(@Nullable Context context) {
        if (context == null) {
            return false;
        }
        RecordUserAction.record("AutofillCreditCardsViewed");
        SettingsNavigationFactory.createSettingsNavigation()
                .startSettings(context, AutofillPaymentMethodsFragment.class);
        return true;
    }

    @CalledByNative
    private static void showAutofillProfileSettings(WebContents webContents) {
        WindowAndroid windowAndroid = webContents.getTopLevelNativeWindow();
        if (windowAndroid == null) return;
        showAutofillProfileSettings(windowAndroid.getActivity().get());
    }

    @CalledByNative
    private static void showAutofillCreditCardSettings(WebContents webContents) {
        WindowAndroid windowAndroid = webContents.getTopLevelNativeWindow();
        if (windowAndroid == null) return;
        showAutofillCreditCardSettings(windowAndroid.getActivity().get());
    }
}
