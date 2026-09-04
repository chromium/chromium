// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;
import android.os.Bundle;

import org.jni_zero.CalledByNative;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.settings.AutofillAndPasswordsFragment.AutofillSettingsReferrer;
import org.chromium.chrome.browser.autofill.settings.options.AutofillOptionsFragment;
import org.chromium.chrome.browser.autofill.settings.options.AutofillOptionsReferrer;
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
        if (context == null
                || !ChromeFeatureList.isEnabled(
                        ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID)) {
            return false;
        }

        RecordUserAction.record("AutofillYourSavedInfoViewed");
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putInt(
                AutofillAndPasswordsFragment.EXTRA_REFERRER,
                AutofillSettingsReferrer.SETTINGS_MENU);
        SettingsNavigationFactory.createSettingsNavigation(context)
                .startSettings(context, AutofillAndPasswordsFragment.class, fragmentArgs);
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
        SettingsNavigationFactory.createSettingsNavigation(context)
                .startSettings(
                        context,
                        AutofillIdentityDocsFragment.class,
                        /* fragmentArgs= */ null,
                        /* addToBackStack= */ true);
        return true;
    }

    /**
     * Tries showing the settings page for Shopping.
     *
     * @param context The {@link Context} required to start the settings page. Noop without it.
     * @return True if the context is valid and `startSettings` was called.
     */
    public static boolean showAutofillShoppingSettings(@Nullable Context context) {
        if (context == null) {
            return false;
        }
        SettingsNavigationFactory.createSettingsNavigation(context)
                .startSettings(
                        context,
                        AutofillShoppingFragment.class,
                        /* fragmentArgs= */ null,
                        /* addToBackStack= */ true);
        return true;
    }

    /**
     * Tries showing the settings page for Personal Context.
     *
     * @param context The {@link Context} required to start the settings page. Noop without it.
     * @return True if the context is valid and `startSettings` was called.
     */
    public static boolean showAutofillPersonalContextSettings(
            @Nullable Context context, @AutofillOptionsReferrer int referrer) {
        return PersonalContextSettingsLauncher.showPersonalContextSettings(context, referrer);
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
        SettingsNavigationFactory.createSettingsNavigation(context)
                .startSettings(
                        context,
                        AutofillTravelFragment.class,
                        /* fragmentArgs= */ null,
                        /* addToBackStack= */ true);
        return true;
    }

    /**
     * Tries showing the settings page for Autofill options.
     *
     * @param context The {@link Context} required to start the settings page. Noop without it.
     * @return True if the context is valid and `startSettings` was called.
     */
    public static boolean showAutofillSettings(@Nullable Context context) {
        if (context == null) {
            return false;
        }
        SettingsNavigationFactory.createSettingsNavigation(context)
                .startSettings(
                        context,
                        AutofillOptionsFragment.class,
                        AutofillOptionsFragment.createRequiredArgs(
                                AutofillOptionsReferrer.PRIVATE_INFERENCE_NOTICE),
                        /* addToBackStack= */ true);
        return true;
    }

    /**
     * Tries showing the settings page for Addresses.
     *
     * @param context The {@link Context} required to start the settings page. Noop without it.
     * @return True iff the context is valid and `startSettings` was called.
     */
    public static boolean showAutofillProfileSettings(@Nullable Context context) {
        return showAutofillProfileSettings(context, /* addToBackStack= */ false);
    }

    /**
     * Tries showing the settings page for Addresses.
     *
     * @param context The {@link Context} required to start the settings page. Noop without it.
     * @param addToBackStack Whether to call startSettings method with adding to backstack.
     * @return True if the context is valid and `startSettings` was called.
     */
    public static boolean showAutofillProfileSettings(
            @Nullable Context context, boolean addToBackStack) {
        if (context == null) {
            return false;
        }
        RecordUserAction.record("AutofillAddressesViewed");

        SettingsNavigationFactory.createSettingsNavigation(context)
                .startSettings(
                        context,
                        AutofillProfilesFragment.class,
                        /* fragmentArgs= */ null,
                        addToBackStack);

        return true;
    }

    /**
     * Tries showing the settings page for Payments.
     *
     * @param context The {@link Context} required to start the settings page. Noop without it.
     * @return True iff the context is valid and `startSettings` was called.
     */
    public static boolean showAutofillCreditCardSettings(@Nullable Context context) {
        return showAutofillCreditCardSettings(context, /* addToBackStack= */ false);
    }

    /**
     * Tries showing the settings page for Payments.
     *
     * @param context The {@link Context} required to start the settings page. Noop without it.
     * @param addToBackStack Whether to call startSettings method with adding to backstack.
     * @return True if the context is valid and `startSettings` was called.
     */
    public static boolean showAutofillCreditCardSettings(
            @Nullable Context context, boolean addToBackStack) {
        if (context == null) {
            return false;
        }
        RecordUserAction.record("AutofillCreditCardsViewed");

        SettingsNavigationFactory.createSettingsNavigation(context)
                .startSettings(
                        context,
                        AutofillPaymentMethodsFragment.class,
                        /* fragmentArgs= */ null,
                        addToBackStack);
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

    @CalledByNative
    private static void showAutofillIdentityDocsSettings(WebContents webContents) {
        WindowAndroid windowAndroid = webContents.getTopLevelNativeWindow();
        if (windowAndroid == null) return;
        showAutofillIdentityDocsSettings(windowAndroid.getActivity().get());
    }

    @CalledByNative
    private static void showAutofillTravelSettings(WebContents webContents) {
        WindowAndroid windowAndroid = webContents.getTopLevelNativeWindow();
        if (windowAndroid == null) return;
        showAutofillTravelSettings(windowAndroid.getActivity().get());
    }

    @CalledByNative
    private static void showAutofillShoppingSettings(WebContents webContents) {
        WindowAndroid windowAndroid = webContents.getTopLevelNativeWindow();
        if (windowAndroid == null) return;
        showAutofillShoppingSettings(windowAndroid.getActivity().get());
    }

    @CalledByNative
    private static void showAutofillPersonalContextSettings(
            WebContents webContents, @AutofillOptionsReferrer int referrer) {
        WindowAndroid windowAndroid = webContents.getTopLevelNativeWindow();
        if (windowAndroid == null) return;
        showAutofillPersonalContextSettings(windowAndroid.getActivity().get(), referrer);
    }

    @CalledByNative
    private static void showAutofillSettings(WebContents webContents) {
        WindowAndroid windowAndroid = webContents.getTopLevelNativeWindow();
        if (windowAndroid == null) return;
        showAutofillSettings(windowAndroid.getActivity().get());
    }
}
