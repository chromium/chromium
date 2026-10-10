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

    private int mRenderProcessId = -1;
    private int mRenderFrameId = -1;

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
        mRenderProcessId = -1;
        mRenderFrameId = -1;
        if (!canPrint()) return false;
        long targetFrame =
                WebContentsPrinterJni.get()
                        .initiatePrint(
                                mWebContents, renderProcessId, renderFrameId, mPrintSelectionOnly);
        if (targetFrame == -1) return false;
        mRenderProcessId = (int) (targetFrame >> 32);
        mRenderFrameId = (int) targetFrame;
        return true;
    }

    @Override
    public boolean print(int renderProcessId, int renderFrameId) {
        if (!canPrint()) return false;
        boolean hasExplicitTarget = renderProcessId >= 0 && renderFrameId >= 0;
        int processId = hasExplicitTarget ? renderProcessId : mRenderProcessId;
        int frameId = hasExplicitTarget ? renderFrameId : mRenderFrameId;
        return WebContentsPrinterJni.get()
                .print(mWebContents, processId, frameId, mPrintSelectionOnly);
    }

    @Override
    public void finishPrint(int renderProcessId, int renderFrameId) {
        boolean hasExplicitTarget = renderProcessId >= 0 && renderFrameId >= 0;
        int processId = hasExplicitTarget ? renderProcessId : mRenderProcessId;
        int frameId = hasExplicitTarget ? renderFrameId : mRenderFrameId;
        mRenderProcessId = -1;
        mRenderFrameId = -1;
        if (mWebContents.isDestroyed()) return;
        WebContentsPrinterJni.get().finishPrint(mWebContents, processId, frameId);
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
        long initiatePrint(
                @Nullable WebContents webContents,
                int renderProcessId,
                int renderFrameId,
                boolean printSelectionOnly);

        boolean print(
                @Nullable WebContents webContents,
                int renderProcessId,
                int renderFrameId,
                boolean printSelectionOnly);

        void finishPrint(@Nullable WebContents webContents, int renderProcessId, int renderFrameId);
    }
}
