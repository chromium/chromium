// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.ToolbarSwipeLayout;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EdgeSwipeHandler;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.ui.resources.ResourceManager;

class BottomControlsProperties {
    /** The height of the bottom control container (view which includes the top shadow) in px. */
    static final WritableIntPropertyKey BOTTOM_CONTROLS_CONTAINER_HEIGHT_PX =
            new WritableIntPropertyKey();

    /** The height of the bottom toolbar view in px. */
    static final WritableIntPropertyKey BOTTOM_CONTROLS_HEIGHT_PX = new WritableIntPropertyKey();

    /** The Y offset of the view in px. */
    static final WritableIntPropertyKey Y_OFFSET = new WritableIntPropertyKey();

    /** Whether the Android view version of the bottom controls component is visible. */
    static final WritableBooleanPropertyKey ANDROID_VIEW_VISIBLE = new WritableBooleanPropertyKey();

    /** Whether the composited version of the bottom controls component is visible. */
    static final WritableBooleanPropertyKey COMPOSITED_VIEW_VISIBLE =
            new WritableBooleanPropertyKey();

    /** A {@link LayoutManager} to attach overlays to. */
    static final WritableObjectPropertyKey<LayoutManager> LAYOUT_MANAGER =
            new WritableObjectPropertyKey<>();

    /** The browser's {@link ToolbarSwipeLayout}. */
    static final WritableObjectPropertyKey<ToolbarSwipeLayout> TOOLBAR_SWIPE_LAYOUT =
            new WritableObjectPropertyKey<>();

    /** A {@link ResourceManager} for loading textures into the compositor. */
    static final WritableObjectPropertyKey<ResourceManager> RESOURCE_MANAGER =
            new WritableObjectPropertyKey<>();

    /** A handler for swipe events on the toolbar. */
    static final WritableObjectPropertyKey<EdgeSwipeHandler> TOOLBAR_SWIPE_HANDLER =
            new WritableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = new PropertyKey[] {BOTTOM_CONTROLS_CONTAINER_HEIGHT_PX,
            BOTTOM_CONTROLS_HEIGHT_PX, Y_OFFSET, ANDROID_VIEW_VISIBLE, COMPOSITED_VIEW_VISIBLE,
            LAYOUT_MANAGER, TOOLBAR_SWIPE_LAYOUT, RESOURCE_MANAGER, TOOLBAR_SWIPE_HANDLER};
}
