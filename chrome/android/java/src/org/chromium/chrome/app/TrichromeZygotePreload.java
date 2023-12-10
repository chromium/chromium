// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.app;

import android.content.pm.ApplicationInfo;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.chrome.browser.ProductConfig;
import org.chromium.content_public.app.ZygotePreload;

/** The {@link ZygotePreload} allowing to use the Chromium Linker when running Trichrome. */
public class TrichromeZygotePreload extends ZygotePreload {
    @Override
    public void doPreload(ApplicationInfo appInfo) {
        // The Chromium Linker is only needed when the App Zygote intends to create the RELRO
        // region.
        boolean useChromiumLinker =
                ProductConfig.USE_CHROMIUM_LINKER
                        && !LibraryLoader.mainProcessIntendsToProvideRelroFd();
        LibraryLoader.getInstance().setLinkerImplementation(useChromiumLinker);
        doPreloadCommon(appInfo);
    }
}
