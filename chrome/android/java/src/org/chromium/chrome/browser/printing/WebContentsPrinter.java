// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.printing;

import android.text.TextUtils;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.content_public.browser.WebContents;
import org.chromium.printing.Printable;

import java.io.InputStream;

/** Wraps printing related functionality of a {@link WebContents} object. */
@JNINamespace("printing")
@NullMarked
public class WebContentsPrinter implements Printable {
    private final WebContents mWebContents;
    private final String mDefaultTitle;
    private final String mErrorMessage;

    public WebContentsPrinter(WebContents webContents) {
        mWebContents = webContents;
        mDefaultTitle = ContextUtils.getApplicationContext().getString(R.string.menu_print);
        mErrorMessage =
                ContextUtils.getApplicationContext().getString(R.string.error_printing_failed);
    }

    @Override
    public boolean print(int renderProcessId, int renderFrameId) {
        if (!canPrint()) return false;
        return WebContentsPrinterJni.get().print(mWebContents, renderProcessId, renderFrameId);
    }

    @Override
    public String getTitle() {
        if (mWebContents.isDestroyed()) return mDefaultTitle;

        String title = mWebContents.getTitle();
        if (!TextUtils.isEmpty(title)) return title;

        String url = mWebContents.getVisibleUrl().getSpec();
        if (!TextUtils.isEmpty(url)) return url;

        return mDefaultTitle;
    }

    @Override
    public boolean canPrint() {
        return !mWebContents.isDestroyed();
    }

    @Override
    public String getErrorMessage() {
        return mErrorMessage;
    }

    @Override
    public @Nullable InputStream getPdfInputStream() {
        return null;
    }

    @NativeMethods
    interface Natives {
        boolean print(@Nullable WebContents webContents, int renderProcessId, int renderFrameId);
    }
}
