// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs.ui;

import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Model for a Tab entry in the review tabs detail screen sheet. */
public class TabItemProperties {
    /** The tab represented by this entry. */
    public static final ReadableObjectPropertyKey<ForeignSessionTab> FOREIGN_SESSION_TAB =
            new ReadableObjectPropertyKey<>();

    /**
     * An indicator of whether this tab is selected or not. This key is kept in sync by the
     * RestoreTabsMediator.
     */
    public static final WritableBooleanPropertyKey IS_SELECTED = new WritableBooleanPropertyKey();

    /** The function to run when this tab item is selected by the user. */
    public static final WritableObjectPropertyKey<Runnable> ON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    /** Creates a model for a Tab. */
    public static PropertyModel create(ForeignSessionTab tab, boolean isSelected) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(FOREIGN_SESSION_TAB, tab)
                .with(IS_SELECTED, isSelected)
                .build();
    }

    public static final PropertyKey[] ALL_KEYS = {
        FOREIGN_SESSION_TAB, IS_SELECTED, ON_CLICK_LISTENER
    };
}
