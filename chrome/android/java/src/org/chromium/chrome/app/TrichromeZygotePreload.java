// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.app;

import android.content.pm.ApplicationInfo;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.chrome.browser.ProductConfig;
import org.chromium.content_public.app.ZygotePreload;

/**
 * The {@link ZygotePreload} allowing to use the ModernLinker when running Trichrome.
 */
public class TrichromeZygotePreload extends ZygotePreload {
    @Override
    public void doPreload(ApplicationInfo appInfo) {
        // The ModernLinker is only needed when the App Zygote intends to create the RELRO region.
        boolean useModernLinker = ProductConfig.USE_MODERN_LINKER
                && !LibraryLoader.mainProcessIntendsToProvideRelroFd();
        LibraryLoader.getInstance().setLinkerImplementation(
                /* useChromiumLinker= */ useModernLinker, /* useModernLinker= */ useModernLinker);
        doPreloadCommon(appInfo);
    }
}
