// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.ClickWithMetaStateCallback;

/** Binder for the Home Button action. */
@NullMarked
public class HomeActionButtonBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == HomeActionProperties.LONG_PRESS_MENU_DELEGATE) {
            ListMenuButton listMenuButton = resolveListMenuButton(view);
            listMenuButton.setDelegate(
                    model.get(HomeActionProperties.LONG_PRESS_MENU_DELEGATE), false);
            listMenuButton.setOnLongClickListener(
                    v -> {
                        if (listMenuButton.getVisibility() != View.VISIBLE) {
                            return false;
                        }
                        listMenuButton.showMenu();
                        return true;
                    });
        } else if (propertyKey == HomeActionProperties.CLICK_WITH_META_CALLBACK) {
            ListMenuButton listMenuButton = resolveListMenuButton(view);
            ClickWithMetaStateCallback callback =
                    model.get(HomeActionProperties.CLICK_WITH_META_CALLBACK);
            listMenuButton.setClickCallback(callback);
        } else {
            ActionButtonBinder.bind(model, view, propertyKey);
        }
    }

    private static ListMenuButton resolveListMenuButton(View view) {
        View targetView = ActionButtonBinder.resolveView(view);
        assert targetView instanceof ListMenuButton : "View must be a ListMenuButton";
        return (ListMenuButton) targetView;
    }
}
