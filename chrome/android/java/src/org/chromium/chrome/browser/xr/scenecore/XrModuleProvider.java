// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.components.module_installer.builder.ModuleInterface;
import org.chromium.ui.xr.scenecore.XrSceneCoreSessionInitializer;
import org.chromium.ui.xr.scenecore.XrSceneCoreSessionManager;

@ModuleInterface(
        module = "xr",
        impl = "org.chromium.chrome.browser.xr.scenecore.XrModuleProviderImpl")
@NullMarked
public interface XrModuleProvider {
    XrSceneCoreSessionManager getXrSceneCoreSessionManager(Activity activity);

    XrSceneCoreSessionInitializer getXrSceneCoreSessionInitializer(
            ActivityLifecycleDispatcher dispatcher, XrSceneCoreSessionManager manager);
}
