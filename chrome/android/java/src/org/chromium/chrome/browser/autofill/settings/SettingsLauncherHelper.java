// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.content_public.browser.WebContents;

/** Launches autofill settings subpages. */
public class SettingsLauncherHelper {
    private static SettingsLauncher sLauncherForTesting;

    /**
     * Tries showing the settings page for Addresses.
     *
     * @param context The {@link Context} required to start the settings page. Noop without it.
     * @return True iff the context is valid and `launchSettingsActivity` was called.
     */
    public static boolean showAutofillProfileSettings(@Nullable Context context) {
        if (context == null) {
            return false;
        }
        RecordUserAction.record("AutofillAddressesViewed");
        getLauncher().launchSettingsActivity(context, AutofillProfilesFragment.class);
        return true;
    }

    /**
     * Tries showing the settings page for Payments.
     *
     * @param context The {@link Context} required to start the settings page. Noop without it.
     * @return True iff the context is valid and `launchSettingsActivity` was called.
     */
    public static boolean showAutofillCreditCardSettings(@Nullable Context context) {
        if (context == null) {
            return false;
        }
        RecordUserAction.record("AutofillCreditCardsViewed");
        getLauncher().launchSettingsActivity(context, AutofillPaymentMethodsFragment.class);
        return true;
    }

    @CalledByNative
    private static void showAutofillProfileSettings(WebContents webContents) {
        showAutofillProfileSettings(webContents.getTopLevelNativeWindow().getActivity().get());
    }

    @CalledByNative
    private static void showAutofillCreditCardSettings(WebContents webContents) {
        showAutofillCreditCardSettings(webContents.getTopLevelNativeWindow().getActivity().get());
    }

    private static SettingsLauncher getLauncher() {
        return sLauncherForTesting != null ? sLauncherForTesting : new SettingsLauncherImpl();
    }

    @VisibleForTesting
    static void setLauncher(SettingsLauncher launcher) {
        sLauncherForTesting = launcher;
    }
}
