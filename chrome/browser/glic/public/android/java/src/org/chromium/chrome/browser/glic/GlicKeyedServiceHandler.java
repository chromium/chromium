// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.glic.GlicKeyedService.GlicInvocationSource;
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
     * @param invocationSource How the UI was triggered.
     * @return true if the UI was successfully toggled.
     */
    public static boolean toggleGlic(
            Profile profile,
            @Nullable ChromeAndroidTask task,
            boolean preventClose,
            @GlicInvocationSource int invocationSource) {
        GlicKeyedService service = GlicKeyedServiceFactory.getForProfile(profile);
        if (service == null) {
            return false;
        }

        if (task == null) {
            Log.w(TAG, "Failed to trigger GLIC: ChromeAndroidTask is null.");
            return false;
        }

        long browserWindowPtr = task.getOrCreateNativeBrowserWindowPtr(profile);
        service.toggleUI(browserWindowPtr, preventClose, profile, invocationSource);
        return true;
    }

    /**
     * Invokes the GLIC service with auto-submit prompt.
     *
     * @param profile The current profile.
     * @param tab The {@link Tab} to target.
     * @param text The text prompt to submit.
     * @param invocationSource How the UI was triggered.
     * @return true if the service was successfully invoked.
     */
    public static boolean invokeWithAutoSubmit(
            Profile profile, Tab tab, String text, @GlicInvocationSource int invocationSource) {

        GlicKeyedService service = GlicKeyedServiceFactory.getForProfile(profile);
        if (service == null) {
            return false;
        }

        return service.invokeWithAutoSubmit(tab, text, invocationSource);
    }
}
