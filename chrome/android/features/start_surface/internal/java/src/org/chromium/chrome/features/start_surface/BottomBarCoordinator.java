// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import org.chromium.chrome.start_surface.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The coordinator to control the bottom bar. */
class BottomBarCoordinator {
    private final PropertyModelChangeProcessor mBottomBarChangeProcessor;

    BottomBarCoordinator(
            Context context, ViewGroup parentView, PropertyModel containerPropertyModel) {
        BottomBarView bottomBarView =
                (BottomBarView) LayoutInflater.from(context)
                        .inflate(R.layout.ss_bottom_bar_layout, parentView, true)
                        .findViewById(R.id.ss_bottom_bar);
        mBottomBarChangeProcessor = PropertyModelChangeProcessor.create(
                containerPropertyModel, bottomBarView, BottomBarViewBinder::bind);
    }
}