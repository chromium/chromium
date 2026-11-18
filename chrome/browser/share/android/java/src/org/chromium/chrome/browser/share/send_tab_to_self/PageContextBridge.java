// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/**
 * Shared JNI bridge providing common {@code PageContext} form-field restoration functionality for
 * cross-device tab sharing features.
 */
@NullMarked
@JNINamespace("send_tab_to_self")
public class PageContextBridge {
    /** Populates form fields in {@code tab} from a serialized {@code PageContext} protobuf. */
    public static void fillWebContents(Tab tab, byte @Nullable [] pageContext, String url) {
        ThreadUtils.assertOnUiThread();
        if (pageContext == null || pageContext.length == 0) {
            return;
        }
        runWhenWebContentsAvailable(
                tab,
                webContents ->
                        PageContextBridgeJni.get()
                                .fillWebContents(webContents, pageContext, new GURL(url)));
    }

    /** Executes {@code action} once the {@link WebContents} of {@code tab} is available. */
    static void runWhenWebContentsAvailable(Tab tab, Callback<WebContents> action) {
        ThreadUtils.assertOnUiThread();
        if (tab.isDestroyed()) return;
        WebContents webContents = tab.getWebContents();
        if (webContents != null) {
            action.onResult(webContents);
            return;
        }
        tab.addObserver(
                new TabObserver() {
                    @Override
                    public void onContentChanged(Tab t) {
                        WebContents contents = t.getWebContents();
                        if (contents == null) return;
                        t.removeObserver(this);
                        action.onResult(contents);
                    }

                    @Override
                    public void onDestroyed(Tab t) {
                        t.removeObserver(this);
                    }
                });
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        void fillWebContents(
                @JniType("content::WebContents*") WebContents webContents,
                @JniType("std::vector<uint8_t>") byte[] pageContext,
                @JniType("GURL") GURL url);
    }
}
