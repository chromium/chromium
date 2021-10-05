// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.view.View;

import org.chromium.components.browser_ui.widget.listmenu.ListMenu;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyListModel;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Properties for a list of {@link SectionHeaderProperties} models.
 */
public class SectionHeaderListProperties {

    public static final PropertyModel.WritableBooleanPropertyKey IS_SECTION_ENABLED_KEY =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel
            .ReadableObjectPropertyKey<PropertyListModel<PropertyModel, PropertyKey>>
                    SECTION_HEADERS_KEY = new PropertyModel.ReadableObjectPropertyKey<>();
    public static final PropertyModel.WritableIntPropertyKey CURRENT_TAB_INDEX_KEY =
            new PropertyModel.WritableIntPropertyKey();
    public static final PropertyModel.WritableObjectPropertyKey<OnSectionHeaderSelectedListener>
            ON_TAB_SELECTED_CALLBACK_KEY = new PropertyModel.WritableObjectPropertyKey<>();
    /** The model of the menu items to show in the overflow menu to manage the feed. */
    public static final PropertyModel
            .WritableObjectPropertyKey<MVCListAdapter.ModelList> MENU_MODEL_LIST_KEY =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel
            .WritableObjectPropertyKey<ListMenu.Delegate> MENU_DELEGATE_KEY =
            new PropertyModel.WritableObjectPropertyKey<>();
    /**
     * Whether to show tabs or not. Tabs will have all headers shown each in a TabLayout.
     * No tabs means only the 1st header will be shown in a left-aligned TextView.
     */
    public static final PropertyModel.WritableBooleanPropertyKey IS_TAB_MODE_KEY =
            new PropertyModel.WritableBooleanPropertyKey();
    /** Whether to show logo (true) or normal visibility indicator (false). */
    public static final PropertyModel.WritableBooleanPropertyKey IS_LOGO_KEY =
            new PropertyModel.WritableBooleanPropertyKey();
    /** Visibility state for the logo/feed visibility indicator. */
    public static final PropertyModel
            .WritableObjectPropertyKey<ViewVisibility> INDICATOR_VIEW_VISIBILITY_KEY =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<View> EXPANDING_DRAWER_VIEW_KEY =
            new PropertyModel.WritableObjectPropertyKey<>();

    public static PropertyModel create() {
        return new PropertyModel
                .Builder(IS_SECTION_ENABLED_KEY, SECTION_HEADERS_KEY, CURRENT_TAB_INDEX_KEY,
                        ON_TAB_SELECTED_CALLBACK_KEY, MENU_MODEL_LIST_KEY, MENU_DELEGATE_KEY,
                        IS_TAB_MODE_KEY, IS_LOGO_KEY, INDICATOR_VIEW_VISIBILITY_KEY,
                        EXPANDING_DRAWER_VIEW_KEY)
                .with(SECTION_HEADERS_KEY, new PropertyListModel<>())
                .with(INDICATOR_VIEW_VISIBILITY_KEY, ViewVisibility.INVISIBLE)
                .build();
    }
}
