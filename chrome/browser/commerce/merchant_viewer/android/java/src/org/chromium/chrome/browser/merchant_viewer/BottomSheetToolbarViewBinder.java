// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for BottomSheetToolbarView. */
public class BottomSheetToolbarViewBinder {
    public static void bind(
            PropertyModel model, BottomSheetToolbarView view, PropertyKey propertyKey) {
        if (BottomSheetToolbarProperties.URL == propertyKey) {
            view.setUrl(model.get(BottomSheetToolbarProperties.URL));
        } else if (BottomSheetToolbarProperties.TITLE == propertyKey) {
            view.setTitle(model.get(BottomSheetToolbarProperties.TITLE));
        } else if (BottomSheetToolbarProperties.LOAD_PROGRESS == propertyKey) {
            view.setProgress(model.get(BottomSheetToolbarProperties.LOAD_PROGRESS));
        } else if (BottomSheetToolbarProperties.PROGRESS_VISIBLE == propertyKey) {
            view.setProgressVisible(model.get(BottomSheetToolbarProperties.PROGRESS_VISIBLE));
        } else if (BottomSheetToolbarProperties.SECURITY_ICON == propertyKey) {
            view.setSecurityIcon(model.get(BottomSheetToolbarProperties.SECURITY_ICON));
        } else if (BottomSheetToolbarProperties.SECURITY_ICON_CONTENT_DESCRIPTION == propertyKey) {
            view.setSecurityIconDescription(
                    model.get(BottomSheetToolbarProperties.SECURITY_ICON_CONTENT_DESCRIPTION));
        } else if (BottomSheetToolbarProperties.SECURITY_ICON_ON_CLICK_CALLBACK == propertyKey) {
            view.setSecurityIconClickCallback(
                    model.get(BottomSheetToolbarProperties.SECURITY_ICON_ON_CLICK_CALLBACK));
        } else if (BottomSheetToolbarProperties.CLOSE_BUTTON_ON_CLICK_CALLBACK == propertyKey) {
            view.setCloseButtonClickCallback(
                    model.get(BottomSheetToolbarProperties.CLOSE_BUTTON_ON_CLICK_CALLBACK));
        } else if (BottomSheetToolbarProperties.FAVICON_ICON == propertyKey) {
            view.setFaviconIcon(model.get(BottomSheetToolbarProperties.FAVICON_ICON));
        } else if (BottomSheetToolbarProperties.FAVICON_ICON_DRAWABLE == propertyKey) {
            view.setFaviconIconDrawable(
                    model.get(BottomSheetToolbarProperties.FAVICON_ICON_DRAWABLE));
        } else if (BottomSheetToolbarProperties.FAVICON_ICON_VISIBLE == propertyKey) {
            view.setFaviconIconVisible(
                    model.get(BottomSheetToolbarProperties.FAVICON_ICON_VISIBLE));
        } else if (BottomSheetToolbarProperties.OPEN_IN_NEW_TAB_VISIBLE == propertyKey) {
            view.setOpenInNewTabButtonVisible(
                    model.get(BottomSheetToolbarProperties.OPEN_IN_NEW_TAB_VISIBLE));
        }
    }
}
