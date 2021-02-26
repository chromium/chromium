// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.snippets;

import android.view.View;

import org.chromium.components.browser_ui.widget.listmenu.ListMenu;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Represents the data for a header of a group of snippets.
 */
public class SectionHeader extends PropertyModel {
    /** The header text to be shown. */
    public static final WritableObjectPropertyKey<String> HEADER_TEXT_KEY =
            new WritableObjectPropertyKey<>();
    /** The model of the menu items to show in the overflow menu to manage the feed. */
    public static final WritableObjectPropertyKey<ModelList> MENU_MODEL_LIST_KEY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<ListMenu.Delegate> MENU_DELEGATE_KEY =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<View.OnClickListener> ON_CLICK_HANDLER_KEY =
            new WritableObjectPropertyKey<>();

    /**
     * Constructor for non-expandable header.
     * @param headerText The title of the header.
     */
    public SectionHeader(String headerText) {
        super(ON_CLICK_HANDLER_KEY, HEADER_TEXT_KEY, MENU_DELEGATE_KEY, MENU_MODEL_LIST_KEY);
        set(HEADER_TEXT_KEY, headerText);
    }
}
