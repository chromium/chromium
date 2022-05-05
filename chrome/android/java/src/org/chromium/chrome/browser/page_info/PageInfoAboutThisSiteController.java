// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/**
 * Class for controlling the page info 'About This Site' section.
 */
public class PageInfoAboutThisSiteController {
    @NativeMethods
    interface Natives {
        boolean isFeatureEnabled();
        byte[] getSiteInfo(BrowserContextHandle browserContext, GURL url, WebContents webContents);
    }
}
