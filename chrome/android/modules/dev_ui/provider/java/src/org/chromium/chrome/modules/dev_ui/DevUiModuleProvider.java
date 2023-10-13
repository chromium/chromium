// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.dev_ui;

import org.jni_zero.CalledByNative;

import org.chromium.chrome.features.dev_ui.DevUiModule;

/** Helpers for DevUI DFM installation. */
public class DevUiModuleProvider {
    @CalledByNative
    private static boolean isModuleInstalled() {
        return DevUiModule.isInstalled();
    }

    @CalledByNative
    private static void installModule(DevUiInstallListener listener) {
        DevUiModule.install(listener);
    }

    @CalledByNative
    private static void ensureNativeLoaded() {
        DevUiModule.ensureNativeLoaded();
    }
}
