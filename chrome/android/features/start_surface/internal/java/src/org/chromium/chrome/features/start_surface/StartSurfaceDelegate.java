// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.content.Context;

import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.base.WindowAndroid;

/** StartSurfaceDelegate. */
public class StartSurfaceDelegate {
    public static Layout createStartSurfaceLayout(Context context, LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost, StartSurface startSurface) {
        return new StartSurfaceLayout(context, updateHost, renderHost, startSurface);
    }

    public static StartSurface createStartSurface(ChromeActivity activity,
            ScrimCoordinator scrimCoordinator, BottomSheetController sheetController,
            OneshotSupplierImpl<StartSurface> startSurfaceOneshotSupplier,
            Supplier<Tab> parentTabSupplier, boolean hadWarmStart, WindowAndroid windowAndroid) {
        return new StartSurfaceCoordinator(activity, scrimCoordinator, sheetController,
                startSurfaceOneshotSupplier, parentTabSupplier, hadWarmStart, windowAndroid);
    }
}