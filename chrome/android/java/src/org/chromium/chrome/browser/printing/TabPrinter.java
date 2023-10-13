// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.printing;

import android.text.TextUtils;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;
import org.chromium.printing.Printable;

import java.lang.ref.WeakReference;

/**
 * Wraps printing related functionality of a {@link Tab} object.
 *
 * This class doesn't have any lifetime expectations with regards to Tab, since we keep a weak
 * reference.
 */
@JNINamespace("printing")
public class TabPrinter implements Printable {
    private static final String TAG = "printing";

    private final WeakReference<Tab> mTab;
    private final String mDefaultTitle;
    private final String mErrorMessage;

    @CalledByNative
    private static TabPrinter getPrintable(Tab tab) {
        return new TabPrinter(tab);
    }

    public TabPrinter(Tab tab) {
        mTab = new WeakReference<Tab>(tab);
        mDefaultTitle = ContextUtils.getApplicationContext().getString(R.string.menu_print);
        mErrorMessage =
                ContextUtils.getApplicationContext().getString(R.string.error_printing_failed);
    }

    @Override
    public boolean print(int renderProcessId, int renderFrameId) {
        if (!canPrint()) return false;
        Tab tab = mTab.get();
        assert tab != null && tab.isInitialized();
        return TabPrinterJni.get().print(tab.getWebContents(), renderProcessId, renderFrameId);
    }

    @Override
    public String getTitle() {
        Tab tab = mTab.get();
        if (tab == null || !tab.isInitialized()) return mDefaultTitle;

        String title = tab.getTitle();
        if (!TextUtils.isEmpty(title)) return title;

        String url = tab.getUrl().getSpec();
        if (!TextUtils.isEmpty(url)) return url;

        return mDefaultTitle;
    }

    @Override
    public boolean canPrint() {
        Tab tab = mTab.get();
        if (tab == null || !tab.isInitialized()) {
            // Tab.isInitialized() will be false if tab is in destroy process.
            Log.d(TAG, "Tab is not avaliable for printing.");
            return false;
        }
        return true;
    }

    @Override
    public String getErrorMessage() {
        return mErrorMessage;
    }

    @NativeMethods
    interface Natives {
        boolean print(WebContents webContents, int renderProcessId, int renderFrameId);
    }
}
