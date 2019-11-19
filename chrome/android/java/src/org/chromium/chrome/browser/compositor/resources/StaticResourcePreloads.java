// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.resources;

import android.content.Context;

import org.chromium.chrome.R;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * Tracks all high priority resources that should be loaded at startup to be used by CC layers.
 * TODO(dtrainor): Add the high priority and low priority resources here as they get ported over.
 */
public class StaticResourcePreloads {
    /** A list of resources to load synchronously once the compositor is initialized. */
    private static int[] sSynchronousResources = new int[] {
            R.drawable.bg_tabstrip_tab,
            R.drawable.btn_tab_close_normal,
            R.drawable.btn_tabstrip_new_tab_normal,
            R.drawable.btn_tabstrip_new_incognito_tab_normal,
            R.drawable.btn_tabstrip_new_tab_pressed,
            R.drawable.spinner,
            R.drawable.spinner_white,
    };

    /** A list of resources to load asynchronously once the compositor is initialized. */
    private static int[] sAsynchronousResources = new int[] {
            R.drawable.btn_tabstrip_switch_normal, R.drawable.location_bar_incognito_badge};

    private static int[] sEmptyList = new int[] {};

    public static int[] getSynchronousResources(Context context) {
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(context) ? sSynchronousResources
                                                                          : sEmptyList;
    }

    public static int[] getAsynchronousResources(Context context) {
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(context) ? sAsynchronousResources
                                                                          : sEmptyList;
    }
}
