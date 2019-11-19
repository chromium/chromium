// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.thinwebview;

import android.content.Context;

import org.chromium.chrome.browser.thinwebview.internal.CompositorViewImpl;
import org.chromium.ui.base.WindowAndroid;

/**
 * Factory for creating a {@link CompositorView}.
 */
public class CompositorViewFactory {
    /**
     * Creates a {@link CompositorView} backed by a {@link Surface}. The surface is provided by
     * a either a {@link TextureView} or {@link SurfaceView}.
     * @param context The context to create this view.
     * @param windowAndroid The associated {@code WindowAndroid} on which the view is to be
     *         displayed.
     */
    public static CompositorView create(Context context, WindowAndroid windowAndroid) {
        return new CompositorViewImpl(context, windowAndroid);
    }
}
