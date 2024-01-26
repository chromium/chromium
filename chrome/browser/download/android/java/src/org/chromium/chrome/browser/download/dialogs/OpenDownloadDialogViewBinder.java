// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The view binder that connects {@link OpenDownloadCustomView} and {@link
 * OpenDownloadDialogCoordinator} which defines the UI properties.
 */
class OpenDownloadDialogViewBinder {
    static void bind(PropertyModel model, OpenDownloadCustomView view, PropertyKey propertyKey) {
        if (propertyKey == OpenDownloadDialogProperties.TITLE) {
            view.setTitle(model.get(OpenDownloadDialogProperties.TITLE));
        } else if (propertyKey == OpenDownloadDialogProperties.SUBTITLE) {
            view.setSubtitle(model.get(OpenDownloadDialogProperties.SUBTITLE));
        } else if (propertyKey == OpenDownloadDialogProperties.AUTO_OPEN_CHECKBOX_CHECKED) {
            view.setAutoOpenCheckbox(
                    model.get(OpenDownloadDialogProperties.AUTO_OPEN_CHECKBOX_CHECKED));
        }
    }
}
