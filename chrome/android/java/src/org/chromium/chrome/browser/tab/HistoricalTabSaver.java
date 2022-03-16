// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Saves historical tabs.
 */
@JNINamespace("historical_tab_saver")
public class HistoricalTabSaver {
    private static final List<String> UNSUPPORTED_SCHEMES =
            new ArrayList<>(Arrays.asList(UrlConstants.CHROME_SCHEME,
                    UrlConstants.CHROME_NATIVE_SCHEME, ContentUrlConstants.ABOUT_SCHEME));

    /**
     * Creates a historical tab from a tab being closed.
     */
    public static void createHistoricalTab(Tab tab) {
        if (!shouldSave(tab)) return;

        HistoricalTabSaverJni.get().createHistoricalTab(tab);
    }

    /**
     * Checks that the tab has a valid URL for saving. This requires the URL to exist and not be an
     * internal Chrome scheme, about:blank, or a native page.
     */
    private static boolean shouldSave(Tab tab) {
        GURL committedUrlOrFrozenUrl;
        if (tab.getWebContents() != null) {
            committedUrlOrFrozenUrl = tab.getWebContents().getLastCommittedUrl();
        } else {
            committedUrlOrFrozenUrl = tab.getUrl();
        }

        return committedUrlOrFrozenUrl != null && committedUrlOrFrozenUrl.isValid()
                && !committedUrlOrFrozenUrl.isEmpty()
                && !UNSUPPORTED_SCHEMES.contains(committedUrlOrFrozenUrl.getScheme());
    }

    @CalledByNative
    private static WebContents createTemporaryWebContents(Tab tab) {
        assert tab.isFrozen();
        assert tab.getWebContents() == null;
        WebContentsState state = CriticalPersistedTabData.from(tab).getWebContentsState();
        if (state == null) return null;

        return WebContentsStateBridge.restoreContentsFromByteBuffer(
                state, /*isHidden=*/true, /*noRenderer=*/true);
    }

    @CalledByNative
    private static void destroyTemporaryWebContents(WebContents webContents) {
        if (webContents == null) return;

        webContents.destroy();
    }

    @NativeMethods
    interface Natives {
        void createHistoricalTab(Tab tab);
    }
}
