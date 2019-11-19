// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.addtohomescreen;

import android.graphics.Bitmap;
import android.util.Pair;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Binds an add-to-homescreen {@link PropertyModel} with a {@link AddToHomescreenDialogView}.
 */
public class AddToHomescreenViewBinder {
    public static void bind(PropertyModel model,
            AddToHomescreenDialogView addToHomescreenDialogView, PropertyKey propertyKey) {
        if (propertyKey.equals(AddToHomescreenProperties.TITLE)) {
            addToHomescreenDialogView.setTitle(model.get(AddToHomescreenProperties.TITLE));
        } else if (propertyKey.equals(AddToHomescreenProperties.URL)) {
            addToHomescreenDialogView.setUrl(model.get(AddToHomescreenProperties.URL));
        } else if (propertyKey.equals(AddToHomescreenProperties.ICON)) {
            Pair<Bitmap, Boolean> iconPair = model.get(AddToHomescreenProperties.ICON);
            addToHomescreenDialogView.setIcon(iconPair.first, iconPair.second);
        } else if (propertyKey.equals(AddToHomescreenProperties.TYPE)) {
            addToHomescreenDialogView.setType(model.get(AddToHomescreenProperties.TYPE));
        } else if (propertyKey.equals(AddToHomescreenProperties.CAN_SUBMIT)) {
            addToHomescreenDialogView.setCanSubmit(model.get(AddToHomescreenProperties.CAN_SUBMIT));
        } else if (propertyKey.equals(AddToHomescreenProperties.NATIVE_INSTALL_BUTTON_TEXT)) {
            addToHomescreenDialogView.setNativeInstallButtonText(
                    model.get(AddToHomescreenProperties.NATIVE_INSTALL_BUTTON_TEXT));
        } else if (propertyKey.equals(AddToHomescreenProperties.NATIVE_APP_RATING)) {
            addToHomescreenDialogView.setNativeAppRating(
                    model.get(AddToHomescreenProperties.NATIVE_APP_RATING));
        }
    }
}
