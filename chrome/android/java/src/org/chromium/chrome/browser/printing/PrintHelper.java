// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.printing;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.printing.PrintManagerDelegateImpl;
import org.chromium.printing.PrintingController;
import org.chromium.printing.PrintingControllerImpl;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.widget.Toast;

/** Helper class for triggering print flow for a Tab. */
@NullMarked
public class PrintHelper {
    /**
     * Triggers printing for the specified tab.
     *
     * @param tab The tab to print.
     * @return true if printing was successfully started, false otherwise.
     */
    public static boolean printTab(@Nullable Tab tab) {
        if (tab == null || !tab.isInitialized()) {
            return false;
        }

        if (!UserPrefs.get(tab.getProfile()).getBoolean(Pref.PRINTING_ENABLED)) {
            return false;
        }

        WindowAndroid windowAndroid = tab.getWindowAndroid();
        if (windowAndroid == null) return false;

        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) return false;

        PrintingController printingController = PrintingControllerImpl.getInstance(windowAndroid);
        if (printingController == null || printingController.isBusy()) return false;

        if (tab.isNativePage()) {
            NativePage nativePage = tab.getNativePage();
            if (nativePage != null && !nativePage.isPdf()) {
                Toast.makeText(activity, R.string.toast_disallow_print, Toast.LENGTH_LONG).show();
                return false;
            }
        }

        printingController.startPrint(new TabPrinter(tab), new PrintManagerDelegateImpl(activity));
        return true;
    }
}
