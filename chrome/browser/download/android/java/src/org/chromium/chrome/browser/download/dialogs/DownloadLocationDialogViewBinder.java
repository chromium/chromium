// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The view binder that connects {@link DownloadLocationCustomView} and
 * {@link DownloadLocationDialogCoordinator} which defines the UI properties.
 */
class DownloadLocationDialogViewBinder {
    static void bind(
            PropertyModel model, DownloadLocationCustomView view, PropertyKey propertyKey) {
        if (propertyKey == DownloadLocationDialogProperties.TITLE) {
            view.setTitle(model.get(DownloadLocationDialogProperties.TITLE));
        } else if (propertyKey == DownloadLocationDialogProperties.SUBTITLE) {
            view.setSubtitle(model.get(DownloadLocationDialogProperties.SUBTITLE));
        } else if (propertyKey == DownloadLocationDialogProperties.SHOW_SUBTITLE) {
            view.showSubtitle(model.get(DownloadLocationDialogProperties.SHOW_SUBTITLE));
        } else if (propertyKey == DownloadLocationDialogProperties.SHOW_INCOGNITO_WARNING) {
            view.showIncognitoWarning(
                    model.get(DownloadLocationDialogProperties.SHOW_INCOGNITO_WARNING));
        } else if (propertyKey == DownloadLocationDialogProperties.FILE_NAME) {
            view.setFileName(model.get(DownloadLocationDialogProperties.FILE_NAME));
        } else if (propertyKey == DownloadLocationDialogProperties.FILE_SIZE) {
            view.setFileSize(model.get(DownloadLocationDialogProperties.FILE_SIZE));
        } else if (propertyKey == DownloadLocationDialogProperties.SHOW_LOCATION_AVAILABLE_SPACE) {
            view.showLocationAvailableSpace(
                    model.get(DownloadLocationDialogProperties.SHOW_LOCATION_AVAILABLE_SPACE));
        } else if (propertyKey
                == DownloadLocationDialogProperties.DONT_SHOW_AGAIN_CHECKBOX_CHECKED) {
            view.setDontShowAgainCheckbox(
                    model.get(DownloadLocationDialogProperties.DONT_SHOW_AGAIN_CHECKBOX_CHECKED));
        } else if (propertyKey == DownloadLocationDialogProperties.DONT_SHOW_AGAIN_CHECKBOX_SHOWN) {
            view.showDontShowAgainCheckbox(
                    model.get(DownloadLocationDialogProperties.DONT_SHOW_AGAIN_CHECKBOX_SHOWN));
        }
    }
}
