// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;

/** Handler for GLIC keyed service actions. */
@NullMarked
public final class GlicKeyedServiceHandler {
    private static final String TAG = "GlicServiceHandler";

    private GlicKeyedServiceHandler() {}

    /**
     * Toggles the GLIC UI.
     *
     * @param profile The current profile.
     * @param task The ChromeAndroidTask.
     * @return true if the UI was successfully toggled.
     */
    public static boolean toggleGlic(Profile profile, @Nullable ChromeAndroidTask task) {
        GlicKeyedService service = GlicKeyedServiceFactory.getForProfile(profile);
        if (service == null) {
            return false;
        }

        if (task == null) {
            Log.w(TAG, "Failed to trigger GLIC: ChromeAndroidTask is null.");
            return false;
        }

        long browserWindowPtr = task.getOrCreateNativeBrowserWindowPtr(profile);
        // TODO(crbug.com/479863299): Create and pass in enum for invocationSource.
        service.toggleUI(browserWindowPtr, profile, /* invocationSource= */ 7);
        return true;
    }
}
