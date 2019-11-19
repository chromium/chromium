// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

/**
 * StatusViewBinder observes StatusModel changes and triggers StatusView updates.
 */
class StatusViewBinder implements ViewBinder<PropertyModel, StatusView, PropertyKey> {
    StatusViewBinder() {}

    @Override
    public void bind(PropertyModel model, StatusView view, PropertyKey propertyKey) {
        if (StatusProperties.ANIMATIONS_ENABLED.equals(propertyKey)) {
            view.setAnimationsEnabled(model.get(StatusProperties.ANIMATIONS_ENABLED));
        } else if (StatusProperties.STATUS_ICON_RES.equals(propertyKey)) {
            view.setStatusIcon(model.get(StatusProperties.STATUS_ICON_RES));
        } else if (StatusProperties.STATUS_ICON.equals(propertyKey)) {
            view.setStatusIcon(model.get(StatusProperties.STATUS_ICON));
        } else if (StatusProperties.STATUS_ALPHA.equals(propertyKey)) {
            view.setStatusIconAlpha(model.get(StatusProperties.STATUS_ALPHA));
        } else if (StatusProperties.SHOW_STATUS_ICON.equals(propertyKey)) {
            view.setStatusIconShown(model.get(StatusProperties.SHOW_STATUS_ICON));
        } else if (StatusProperties.STATUS_ICON_ACCESSIBILITY_TOAST_RES.equals(propertyKey)) {
            view.setStatusIconAccessibilityToast(
                    model.get(StatusProperties.STATUS_ICON_ACCESSIBILITY_TOAST_RES));
        } else if (StatusProperties.STATUS_ICON_TINT_RES.equals(propertyKey)) {
            view.setStatusIconTint(model.get(StatusProperties.STATUS_ICON_TINT_RES));
        } else if (StatusProperties.STATUS_ICON_DESCRIPTION_RES.equals(propertyKey)) {
            view.setStatusIconDescription(model.get(StatusProperties.STATUS_ICON_DESCRIPTION_RES));
        } else if (StatusProperties.SEPARATOR_COLOR_RES.equals(propertyKey)) {
            view.setSeparatorColor(model.get(StatusProperties.SEPARATOR_COLOR_RES));
        } else if (StatusProperties.STATUS_CLICK_LISTENER.equals(propertyKey)) {
            view.setStatusClickListener(model.get(StatusProperties.STATUS_CLICK_LISTENER));
        } else if (StatusProperties.VERBOSE_STATUS_TEXT_COLOR_RES.equals(propertyKey)) {
            view.setVerboseStatusTextColor(
                    model.get(StatusProperties.VERBOSE_STATUS_TEXT_COLOR_RES));
        } else if (StatusProperties.VERBOSE_STATUS_TEXT_STRING_RES.equals(propertyKey)) {
            view.setVerboseStatusTextContent(
                    model.get(StatusProperties.VERBOSE_STATUS_TEXT_STRING_RES));
        } else if (StatusProperties.VERBOSE_STATUS_TEXT_VISIBLE.equals(propertyKey)) {
            view.setVerboseStatusTextVisible(
                    model.get(StatusProperties.VERBOSE_STATUS_TEXT_VISIBLE));
        } else if (StatusProperties.VERBOSE_STATUS_TEXT_WIDTH.equals(propertyKey)) {
            view.setVerboseStatusTextWidth(model.get(StatusProperties.VERBOSE_STATUS_TEXT_WIDTH));
        } else if (StatusProperties.INCOGNITO_BADGE_VISIBLE.equals(propertyKey)) {
            view.setIncognitoBadgeVisibility(model.get(StatusProperties.INCOGNITO_BADGE_VISIBLE));
        } else {
            assert false : "Unhandled property update";
        }
    }
}
