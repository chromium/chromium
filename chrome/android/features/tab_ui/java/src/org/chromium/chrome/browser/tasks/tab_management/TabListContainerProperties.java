// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class TabListContainerProperties {
    public static final PropertyModel.WritableBooleanPropertyKey IS_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.WritableBooleanPropertyKey IS_INCOGNITO =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel
            .WritableObjectPropertyKey<TabListRecyclerView.VisibilityListener> VISIBILITY_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyModel.ReadableObjectPropertyKey<BrowserControlsStateProvider>
            BROWSER_CONTROLS_STATE_PROVIDER = new PropertyModel.WritableObjectPropertyKey<>();

    /**
     * Integer, but not {@link PropertyModel.WritableIntPropertyKey} so that we can force update on
     * the same value.
     */
    public static final PropertyModel.WritableObjectPropertyKey<Integer> INITIAL_SCROLL_INDEX =
            new PropertyModel.WritableObjectPropertyKey<>(true);

    public static final PropertyModel.WritableBooleanPropertyKey ANIMATE_VISIBILITY_CHANGES =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.WritableIntPropertyKey TOP_MARGIN =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyModel.WritableIntPropertyKey BOTTOM_CONTROLS_HEIGHT =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyModel.WritableIntPropertyKey SHADOW_TOP_OFFSET =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyModel.WritableIntPropertyKey BOTTOM_PADDING =
            new PropertyModel.WritableIntPropertyKey();

    /**
     * Same as {@link TabListCoordinator.TabListMode}.
     */
    public static final PropertyModel.WritableIntPropertyKey MODE =
            new PropertyModel.WritableIntPropertyKey();

    /**
     * A property which is set to focus on the passed tab index for accessibility. Integer, but not
     * {@link PropertyModel.WritableIntPropertyKey} so that we can focus on the same tab index which
     * may have lost focus in between.
     */
    public static final PropertyModel
            .WritableObjectPropertyKey<Integer> FOCUS_TAB_INDEX_FOR_ACCESSIBILITY =
            new PropertyModel.WritableObjectPropertyKey<>(/*skipEquality=*/true);

    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {IS_VISIBLE, IS_INCOGNITO,
            VISIBILITY_LISTENER, BROWSER_CONTROLS_STATE_PROVIDER, INITIAL_SCROLL_INDEX,
            ANIMATE_VISIBILITY_CHANGES, TOP_MARGIN, BOTTOM_CONTROLS_HEIGHT, SHADOW_TOP_OFFSET,
            BOTTOM_PADDING, MODE, FOCUS_TAB_INDEX_FOR_ACCESSIBILITY};
}
