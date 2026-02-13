// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * GlicKeyedService is the core class for managing Glic flows. It represents a native
 * GlicKeyedService object in Java.
 */
@NullMarked
public interface GlicKeyedService {
    /**
     * Toggles the Glic user interface.
     *
     * @param browserWindowPtr The native pointer (long) to the BrowserWindowInterface.
     * @param profile The {@link Profile} associated with this service instance.
     * @param invocationSource An integer representing the {@code mojom::InvocationSource} mapping
     *     to how the UI was triggered.
     */
    void toggleUI(long browserWindowPtr, Profile profile, int invocationSource);
}
