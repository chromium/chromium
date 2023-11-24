// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs.ui;

import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ALL_KEYS;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** A helper class to rebind all keys on a screen change for the restore tabs workflow. */
public class RestoreTabsViewBinderHelper {
    /**
     * A functional interface to perform a callback and run screen specific bind logic.
     * @param <T> the view holder that helps bind the screen.
     */
    public interface BindScreenCallback<T> {
        /**
         * Perform bind logic on all property keys for the respective screen.
         *
         * @param model the property model of the screen being handled.
         * @param view the view holder of the screen being handled.
         * @param propertyKey the property key being changed.
         */
        void bind(PropertyModel model, T view, PropertyKey propertyKey);
    }

    public static <T> void allKeysBinder(
            PropertyModel model, T view, BindScreenCallback<T> callback) {
        for (PropertyKey propertyKey : ALL_KEYS) {
            callback.bind(model, view, propertyKey);
        }
    }
}
