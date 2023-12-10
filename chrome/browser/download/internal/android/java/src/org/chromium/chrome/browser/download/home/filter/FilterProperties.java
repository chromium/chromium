// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import android.view.View;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The properties needed to render the download home filter view. */
public interface FilterProperties {
    /** The {@link View} to show in the content area. */
    public static final WritableObjectPropertyKey<View> CONTENT_VIEW =
            new WritableObjectPropertyKey<>();

    /** Which {@code TabType} should be selected. */
    public static final WritableIntPropertyKey SELECTED_TAB = new WritableIntPropertyKey();

    /** The callback listener for {@code TabType} selection changes. */
    public static final WritableObjectPropertyKey<Callback</* @TabType */ Integer>>
            CHANGE_LISTENER = new WritableObjectPropertyKey<>();

    /** Whether or not to show the tabs or just show the content. */
    public static final WritableBooleanPropertyKey SHOW_TABS = new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {CONTENT_VIEW, SELECTED_TAB, CHANGE_LISTENER, SHOW_TABS};
}
