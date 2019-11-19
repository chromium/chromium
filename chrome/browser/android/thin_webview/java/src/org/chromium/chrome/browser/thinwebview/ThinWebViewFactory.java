// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.thinwebview;

import android.content.Context;

import org.chromium.chrome.browser.thinwebview.internal.ThinWebViewImpl;
import org.chromium.ui.base.WindowAndroid;

/**
 * Factory for creating a {@link ThinWebView}.
 */
public class ThinWebViewFactory {
    /**
     * Creates a {@link ThinWebView} backed by a {@link Surface}. The surface is provided by
     * a either a {@link TextureView} or {@link SurfaceView}.
     * @param context The context to create this view.
     * @param windowAndroid The associated {@code WindowAndroid} on which the view is to be
     *         displayed.
     */
    public static ThinWebView create(Context context, WindowAndroid windowAndroid) {
        return new ThinWebViewImpl(context, windowAndroid);
    }
}
