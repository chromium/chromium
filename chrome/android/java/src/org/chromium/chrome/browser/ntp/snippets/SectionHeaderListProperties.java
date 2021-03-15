// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

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

    public static PropertyModel create() {
        return new PropertyModel
                .Builder(IS_SECTION_ENABLED_KEY, SECTION_HEADERS_KEY, CURRENT_TAB_INDEX_KEY,
                        ON_TAB_SELECTED_CALLBACK_KEY, MENU_MODEL_LIST_KEY, MENU_DELEGATE_KEY)
                .with(SECTION_HEADERS_KEY, new PropertyListModel<>())
                .build();
    }
}
