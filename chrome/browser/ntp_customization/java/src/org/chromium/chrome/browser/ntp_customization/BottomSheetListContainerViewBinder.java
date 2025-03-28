// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LIST_CONTAINER_VIEW_DELEGATE;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Class responsible for binding a delegate to a {@link BottomSheetListContainerView}. The delegate
 * provides list content and event handlers to the list container view.
 */
public class BottomSheetListContainerViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == LIST_CONTAINER_VIEW_DELEGATE) {
            ListContainerViewDelegate delegate = model.get(LIST_CONTAINER_VIEW_DELEGATE);
            if (delegate == null) {
                ((BottomSheetListContainerView) view).destroy();
            } else {
                ((BottomSheetListContainerView) view).renderAllListItems(delegate);
            }
        }
    }
}
