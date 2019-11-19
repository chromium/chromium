// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.content.Context;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;

/** StartSurfaceDelegate. */
public class StartSurfaceDelegate {
    public static Layout createStartSurfaceLayout(Context context, LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost, StartSurface startSurface) {
        return new StartSurfaceLayout(context, updateHost, renderHost, startSurface);
    }

    public static StartSurface createStartSurface(ChromeActivity activity) {
        return new StartSurfaceCoordinator(activity);
    }
}