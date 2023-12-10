// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs.ui;

import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

/** Model for a ForeignSession entry in the device detail screen sheet. */
public class ForeignSessionItemProperties {
    /** The device represented by this entry. */
    public static final ReadableObjectPropertyKey<ForeignSession> SESSION_PROFILE =
            new ReadableObjectPropertyKey<>();

    /**
     * An indicator of whether this device is the currently selected one. This key is kept
     * in sync by the RestoreTabsMediator.
     */
    public static final WritableBooleanPropertyKey IS_SELECTED = new WritableBooleanPropertyKey();

    /** The function to run when this session item is selected by the user. */
    public static final ReadableObjectPropertyKey<Runnable> ON_CLICK_LISTENER =
            new ReadableObjectPropertyKey<>();

    /** Creates a model for a ForeignSession device. */
    public static PropertyModel create(
            ForeignSession session, boolean isSelected, Runnable onClickListener) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(SESSION_PROFILE, session)
                .with(IS_SELECTED, isSelected)
                .with(ON_CLICK_LISTENER, onClickListener)
                .build();
    }

    public static final PropertyKey[] ALL_KEYS = {SESSION_PROFILE, IS_SELECTED, ON_CLICK_LISTENER};
}
