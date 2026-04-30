// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
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
     * @param preventClose Whether to prevent closing the UI if it's already open.
     * @return true if the UI was successfully toggled.
     */
    public static boolean toggleGlic(
            Profile profile, @Nullable ChromeAndroidTask task, boolean preventClose) {
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
        service.toggleUI(browserWindowPtr, preventClose, profile, /* invocationSource= */ 7);
        return true;
    }

    /**
     * Invokes the GLIC service with auto-submit prompt.
     *
     * @param profile The current profile.
     * @param tab The {@link Tab} to target.
     * @param text The text prompt to submit.
     * @param invocationSource An integer representing the {@code mojom::InvocationSource} mapping.
     * @return true if the service was successfully invoked.
     */
    // TODO(crbug.com/479863299): Create and pass in enum for invocationSource.
    public static boolean invokeWithAutoSubmit(
            Profile profile, Tab tab, String text, int invocationSource) {

        GlicKeyedService service = GlicKeyedServiceFactory.getForProfile(profile);
        if (service == null) {
            return false;
        }

        return service.invokeWithAutoSubmit(tab, text, invocationSource);
    }
}
