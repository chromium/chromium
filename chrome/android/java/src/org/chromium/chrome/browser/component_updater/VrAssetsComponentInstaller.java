// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.component_updater;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/** Java-side implementation of the VrAssetsComponentInstaller. */
@JNINamespace("component_updater")
public class VrAssetsComponentInstaller {
    @CalledByNative
    private static boolean shouldRegisterOnStartup() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.VR_SHOULD_REGISTER_ASSETS_COMPONENT_ON_STARTUP, false);
    }
}
