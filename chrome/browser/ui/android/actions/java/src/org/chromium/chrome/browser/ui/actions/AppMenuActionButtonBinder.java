// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions;

import android.view.View;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binder for the App Menu Button action. */
@NullMarked
public class AppMenuActionButtonBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        view = ActionButtonBinder.resolveView(view);

        if (propertyKey == AppMenuActionProperties.APP_MENU_SETUP_CALLBACK) {
            Callback<View> callback = model.get(AppMenuActionProperties.APP_MENU_SETUP_CALLBACK);
            if (callback != null) {
                callback.onResult(view);
            }
        } else {
            ActionButtonBinder.bind(model, view, propertyKey);
        }
    }
}
