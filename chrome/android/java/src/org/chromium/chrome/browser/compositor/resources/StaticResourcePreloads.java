// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.resources;

import android.content.Context;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.chrome.browser.toolbar.ToolbarFeatures;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.Arrays;

/**
 * Tracks all high priority resources that should be loaded at startup to be used by CC layers.
 * TODO(dtrainor): Add the high priority and low priority resources here as they get ported over.
 */
public class StaticResourcePreloads {
    /** A list of resources to load synchronously once the compositor is initialized. */
    private static final int[] sSynchronousResources =
            new int[] {
                TabUiThemeUtil.getTabResource(),
                R.drawable.btn_tab_close_normal,
                R.drawable.spinner,
                R.drawable.spinner_white,
                R.drawable.ic_new_tab_button,
            };

    /** A list of resources to load asynchronously once the compositor is initialized. */
    private static final int[] sAsynchronousResources =
            new int[] {
                R.drawable.btn_tabstrip_switch_normal, R.drawable.location_bar_incognito_badge
            };

    private static final int[] sEmptyList = new int[] {};

    private static final int sUrlBarResourceId = R.drawable.modern_location_bar;

    public static int[] getSynchronousResources(Context context) {
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                ? sSynchronousResources
                : sEmptyList;
    }

    public static int[] getAsynchronousResources(Context context) {
        int[] resources =
                DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                        ? sAsynchronousResources
                        : sEmptyList;
        if (ToolbarFeatures.shouldSuppressCaptures()) {
            resources = Arrays.copyOf(resources, resources.length + 1);
            resources[resources.length - 1] = sUrlBarResourceId;
        }

        return resources;
    }
}
