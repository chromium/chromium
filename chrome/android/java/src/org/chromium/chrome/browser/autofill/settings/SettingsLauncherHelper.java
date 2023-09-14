// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.BuildInfo;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.device_lock.DeviceLockActivityLauncherImpl;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

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
     * @param windowAndroid The {@link WindowAndroid} required to start the settings page on
     *        automotive devices. Noop without it.
     * @return True iff the context is valid and `launchSettingsActivity` was called.
     */
    public static boolean showAutofillCreditCardSettings(
            @Nullable Context context, @Nullable WindowAndroid windowAndroid) {
        if (context == null) {
            return false;
        }
        if (BuildInfo.getInstance().isAutomotive) {
            DeviceLockActivityLauncherImpl.get().presentDeviceLockChallenge(
                    context, windowAndroid, () -> launchSettingsActivity(context));
        } else {
            launchSettingsActivity(context);
        }
        return true;
    }

    private static void launchSettingsActivity(Context context) {
        RecordUserAction.record("AutofillCreditCardsViewed");
        getLauncher().launchSettingsActivity(context, AutofillPaymentMethodsFragment.class);
    }

    @CalledByNative
    private static void showAutofillProfileSettings(WebContents webContents) {
        showAutofillProfileSettings(webContents.getTopLevelNativeWindow().getActivity().get());
    }

    @CalledByNative
    private static void showAutofillCreditCardSettings(WebContents webContents) {
        showAutofillCreditCardSettings(webContents.getTopLevelNativeWindow().getActivity().get(),
                webContents.getTopLevelNativeWindow());
    }

    private static SettingsLauncher getLauncher() {
        return sLauncherForTesting != null ? sLauncherForTesting : new SettingsLauncherImpl();
    }

    @VisibleForTesting
    static void setLauncher(SettingsLauncher launcher) {
        sLauncherForTesting = launcher;
    }
}
