// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import androidx.annotation.Nullable;

import org.chromium.components.content_settings.CookieControlsBridge;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;

/** Java implementation of FakeCookieControlsBridge for testing that does nothing. */
public class FakeCookieControlsBridge implements CookieControlsBridge.Natives {

    @Override
    public long init(
            CookieControlsBridge caller,
            WebContents webContents,
            BrowserContextHandle originalContextHandle) {
        return 0;
    }

    @Override
    public void updateWebContents(
            long nativeCookieControlsBridge,
            WebContents webContents,
            @Nullable BrowserContextHandle originalBrowserContext) {}

    @Override
    public void destroy(long nativeCookieControlsBridge, CookieControlsBridge caller) {}

    @Override
    public void setThirdPartyCookieBlockingEnabledForSite(
            long nativeCookieControlsBridge, boolean blockCookies) {}

    @Override
    public void onUiClosing(long nativeCookieControlsBridge) {}

    @Override
    public void onEntryPointAnimated(long nativeCookieControlsBridge) {}

    @Override
    public boolean isCookieControlsEnabled(BrowserContextHandle browserContextHandle) {
        return false;
    }
}
