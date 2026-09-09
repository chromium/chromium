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
    private final boolean mPrintSelectionOnly;
    private final String mDefaultTitle;
    private final String mErrorMessage;

    /**
     * Creates a {@link WebContentsPrinter} for the given web contents.
     *
     * @param webContents The web contents to print.
     */
    public WebContentsPrinter(WebContents webContents) {
        this(webContents, false);
    }

    /**
     * Creates a {@link WebContentsPrinter} for the given web contents with selection flag.
     *
     * @param webContents The web contents to print.
     * @param printSelectionOnly Whether to print only the selection within the frame.
     */
    public WebContentsPrinter(WebContents webContents, boolean printSelectionOnly) {
        mWebContents = webContents;
        mPrintSelectionOnly = printSelectionOnly;
        mDefaultTitle = ContextUtils.getApplicationContext().getString(R.string.menu_print);
        mErrorMessage =
                ContextUtils.getApplicationContext().getString(R.string.error_printing_failed);
    }

    @Override
    public boolean initiatePrint(int renderProcessId, int renderFrameId) {
        if (!canPrint()) return false;
        return WebContentsPrinterJni.get()
                .initiatePrint(mWebContents, renderProcessId, renderFrameId, mPrintSelectionOnly);
    }

    @Override
    public boolean print(int renderProcessId, int renderFrameId) {
        if (!canPrint()) return false;
        return WebContentsPrinterJni.get()
                .print(mWebContents, renderProcessId, renderFrameId, mPrintSelectionOnly);
    }

    @Override
    public void finishPrint(int renderProcessId, int renderFrameId) {
        if (mWebContents.isDestroyed()) return;
        WebContentsPrinterJni.get()
                .finishPrint(mWebContents, renderProcessId, renderFrameId, mPrintSelectionOnly);
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
        boolean initiatePrint(
                @Nullable WebContents webContents,
                int renderProcessId,
                int renderFrameId,
                boolean printSelectionOnly);

        boolean print(
                @Nullable WebContents webContents,
                int renderProcessId,
                int renderFrameId,
                boolean printSelectionOnly);

        void finishPrint(
                @Nullable WebContents webContents,
                int renderProcessId,
                int renderFrameId,
                boolean printSelectionOnly);
    }
}
