// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class TabListContainerProperties {
    public static final PropertyModel.WritableBooleanPropertyKey BLOCK_TOUCH_INPUT =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.WritableBooleanPropertyKey IS_INCOGNITO =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.ReadableObjectPropertyKey<BrowserControlsStateProvider>
            BROWSER_CONTROLS_STATE_PROVIDER = new PropertyModel.WritableObjectPropertyKey<>();

    /**
     * Integer, but not {@link PropertyModel.WritableIntPropertyKey} so that we can force update on
     * the same value.
     */
    public static final PropertyModel.WritableObjectPropertyKey<Integer> INITIAL_SCROLL_INDEX =
            new PropertyModel.WritableObjectPropertyKey<>(true);

    /** Same as {@link TabListCoordinator.TabListMode}. */
    public static final PropertyModel.WritableIntPropertyKey MODE =
            new PropertyModel.WritableIntPropertyKey();

    /**
     * A property which is set to focus on the passed tab index for accessibility. Integer, but not
     * {@link PropertyModel.WritableIntPropertyKey} so that we can focus on the same tab index which
     * may have lost focus in between.
     */
    public static final PropertyModel.WritableObjectPropertyKey<Integer>
            FOCUS_TAB_INDEX_FOR_ACCESSIBILITY =
                    new PropertyModel.WritableObjectPropertyKey<>(/* skipEquality= */ true);

    /** Keys for {@link TabSwitcherPaneCoordinator}. */
    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                BLOCK_TOUCH_INPUT,
                IS_INCOGNITO,
                BROWSER_CONTROLS_STATE_PROVIDER,
                INITIAL_SCROLL_INDEX,
                MODE,
                FOCUS_TAB_INDEX_FOR_ACCESSIBILITY
            };
}
