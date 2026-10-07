// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.content.Context;
import android.content.ContextWrapper;

import androidx.annotation.UiThread;

import org.chromium.android_webview.AwWebContent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A ContextWrapper that holds a reference to an AwWebContent. Used to pass AwWebContent
 * configuration to WebView during construction.
 */
@NullMarked
public class WebContentContextWrapper extends ContextWrapper {
    private boolean mWasUsed;
    private final AwWebContent mWebContent;

    @UiThread
    public WebContentContextWrapper(Context base, AwWebContent webContent) {
        super(base);
        mWebContent = webContent;
    }

    @UiThread
    public AwWebContent getWebContent() {
        return mWebContent;
    }

    @UiThread
    public void markAsUsed() {
        mWasUsed = true;
    }

    @UiThread
    public boolean wasUsed() {
        return mWasUsed;
    }

    /**
     * Helper to find the WebContentContextWrapper in the context chain, unwrapping any other
     * ContextWrappers.
     */
    @UiThread
    private static @Nullable WebContentContextWrapper get(Context context) {
        while (context instanceof ContextWrapper) {
            if (context instanceof WebContentContextWrapper) {
                return (WebContentContextWrapper) context;
            }
            context = ((ContextWrapper) context).getBaseContext();
        }
        return null;
    }

    /**
     * Gets the AwWebContent from the Context chain and mark the wrapper as used if it exists,
     * otherwise returns null. */
    @UiThread
    public static @Nullable AwWebContent getWebContent(Context context) {
        WebContentContextWrapper wrapper = get(context);
        if (wrapper != null) {
            wrapper.markAsUsed();
            return wrapper.getWebContent();
        }
        return null;
    }
}
