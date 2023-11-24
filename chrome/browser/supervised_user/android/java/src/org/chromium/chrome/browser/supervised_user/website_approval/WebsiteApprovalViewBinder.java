// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user.website_approval;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides functions that map {@link WebsiteApprovalProperties} changes in a {@link PropertyModel}
 * to the suitable method in {@link WebsiteApprovalSheetContent}.
 */
class WebsiteApprovalViewBinder {
    /**
     * Called whenever a property in the given model changes. It updates the given view
     * accordingly.
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link WebsiteApprovalSheetContent} to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    public static void bind(
            PropertyModel model, WebsiteApprovalSheetContent view, PropertyKey propertyKey) {
        if (propertyKey == WebsiteApprovalProperties.CHILD_NAME) {
            view.setTitle(model.get(WebsiteApprovalProperties.CHILD_NAME));
        } else if (propertyKey == WebsiteApprovalProperties.URL) {
            view.setDomainText(model.get(WebsiteApprovalProperties.URL));
            view.setFullUrlText(model.get(WebsiteApprovalProperties.URL));
        } else if (propertyKey == WebsiteApprovalProperties.ON_CLICK_APPROVE) {
            view.getApproveButton()
                    .setOnClickListener(model.get(WebsiteApprovalProperties.ON_CLICK_APPROVE));
        } else if (propertyKey == WebsiteApprovalProperties.ON_CLICK_DENY) {
            view.getDenyButton()
                    .setOnClickListener(model.get(WebsiteApprovalProperties.ON_CLICK_DENY));
        } else if (propertyKey == WebsiteApprovalProperties.FAVICON) {
            view.setFaviconBitmap(model.get(WebsiteApprovalProperties.FAVICON));
        } else {
            assert false : "Unhandled update to property: " + propertyKey;
        }
    }
}
