// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.app;

import android.content.pm.ApplicationInfo;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.chrome.browser.ProductConfig;
import org.chromium.chrome.browser.version.ChromeVersionInfo;
import org.chromium.content_public.app.ZygotePreload;

/**
 * The {@link ZygotePreload} allowing to use the ModernLinker when running Trichrome.
 */
public class TrichromeZygotePreload extends ZygotePreload {
    @Override
    public void doPreload(ApplicationInfo appInfo) {
        // Temporarily disallow the use of the Chromium Linker in the App Zygote while a performance
        // regression associated with it is being investigated. See http://crbug.com/1154224#c55.
        if (!ChromeVersionInfo.isCanaryBuild() && !ChromeVersionInfo.isDevBuild()
                && !ChromeVersionInfo.isLocalBuild()) {
            LibraryLoader.setDisallowChromiumLinkerInZygote();
        }
        // The ModernLinker is only needed when the App Zygote intends to create the RELRO region.
        boolean useModernLinker = ProductConfig.USE_MODERN_LINKER
                && !LibraryLoader.mainProcessIntendsToProvideRelroFd();
        LibraryLoader.getInstance().setLinkerImplementation(
                /* useChromiumLinker= */ useModernLinker, /* useModernLinker= */ useModernLinker);
        doPreloadCommon(appInfo);
    }
}
