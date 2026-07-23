// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.printing;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.ServiceImpl;
import org.chromium.chrome.browser.contextual_tasks.ContextualTasksUtils;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.thinwebview.ThinWebViewPrintingController;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.WebContents;
import org.chromium.printing.PrintManagerDelegateImpl;
import org.chromium.printing.Printable;
import org.chromium.printing.PrintingController;
import org.chromium.printing.PrintingControllerImpl;
import org.chromium.ui.base.WindowAndroid;

/** Implementation of {@link ThinWebViewPrintingController} for Chrome. */
@NullMarked
@ServiceImpl(ThinWebViewPrintingController.class)
public class ChromeThinWebViewPrintingController implements ThinWebViewPrintingController {
    @Override
    public boolean isPrintSupported(WebContents webContents) {
        if (webContents != null
                && ContextualTasksUtils.isContextualTasksUrl(webContents.getVisibleUrl())) {
            return false;
        }
        Profile profile = Profile.fromWebContents(webContents);
        if (profile == null) return false;
        return UserPrefs.get(profile).getBoolean(Pref.PRINTING_ENABLED);
    }

    @Override
    public void startPrint(WebContents webContents) {
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return;

        Activity activity = window.getActivity().get();
        if (activity == null) return;

        Printable printable = new WebContentsPrinter(webContents);
        PrintingController printingController = PrintingControllerImpl.getInstance(window);
        printingController.startPrint(printable, new PrintManagerDelegateImpl(activity));
    }
}
