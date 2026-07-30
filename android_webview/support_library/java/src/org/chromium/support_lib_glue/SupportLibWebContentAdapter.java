// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import android.content.Context;
import android.webkit.WebView;

import com.android.webview.chromium.WebContent;
import com.android.webview.chromium.WebContentContextWrapper;

import org.chromium.build.annotations.NullMarked;
import org.chromium.support_lib_boundary.WebContentBoundaryInterface;

import java.util.function.Function;

/** Adapter for WebContentBoundaryInterface. */
@NullMarked
class SupportLibWebContentAdapter implements WebContentBoundaryInterface {
    private final WebContent mWebContent;

    public SupportLibWebContentAdapter(WebContent webContent) {
        mWebContent = webContent;
    }

    @Override
    public <T extends WebView> T executeViewFactory(
            Context baseContext, Function<Context, T> factory) {
        WebContentContextWrapper wrapper = new WebContentContextWrapper(baseContext, mWebContent);
        T result = factory.apply(wrapper);
        if (!wrapper.wasUsed()) {
            throw new IllegalStateException("The factory did not construct a new WebView");
        }
        return result;
    }

    @Override
    public void destroy() {
        mWebContent.destroy();
    }
}
