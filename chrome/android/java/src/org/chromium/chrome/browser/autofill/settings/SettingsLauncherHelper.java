// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;

import androidx.fragment.app.Fragment;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.content_public.browser.WebContents;

/** Launches autofill settings subpages. */
public class SettingsLauncherHelper {
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
        Context context = webContents.getTopLevelNativeWindow().getActivity().get();
        if (context != null) {
            SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
            settingsLauncher.launchSettingsActivity(context, fragment);
        }
    }
}
