// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.chromium.chrome.browser.tasks.MostVisitedListProperties.IS_VISIBLE;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Model-to-View binder for most visited list. Handles view manipulations. */
final class MostVisitedListViewBinder {
    public static void bind(PropertyModel model, ViewGroup viewGroup, PropertyKey propertyKey) {
        if (IS_VISIBLE == propertyKey) {
            viewGroup.setVisibility(model.get(IS_VISIBLE) ? View.VISIBLE : View.GONE);
        }
    }
}
