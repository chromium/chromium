// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.View;

import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class TabGridIphItemProperties {
    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnClickListener> IPH_ENTRANCE_CLOSE_BUTTON_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnClickListener> IPH_ENTRANCE_SHOW_BUTTON_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel
            .WritableObjectPropertyKey<View.OnClickListener> IPH_DIALOG_CLOSE_BUTTON_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel
            .WritableObjectPropertyKey<ScrimView.ScrimObserver> IPH_SCRIM_VIEW_OBSERVER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableBooleanPropertyKey IS_IPH_DIALOG_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_IPH_ENTRANCE_VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyModel.WritableBooleanPropertyKey IS_INCOGNITO =
            new PropertyModel.WritableBooleanPropertyKey();
    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {
            IPH_ENTRANCE_CLOSE_BUTTON_LISTENER, IPH_ENTRANCE_SHOW_BUTTON_LISTENER,
            IPH_DIALOG_CLOSE_BUTTON_LISTENER, IPH_SCRIM_VIEW_OBSERVER, IS_IPH_DIALOG_VISIBLE,
            IS_IPH_ENTRANCE_VISIBLE, IS_INCOGNITO};
}
