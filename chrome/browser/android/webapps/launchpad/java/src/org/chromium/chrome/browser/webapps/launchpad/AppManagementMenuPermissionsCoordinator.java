// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.launchpad;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.components.embedder_support.util.Origin;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinator for displaying the selected app's site permission in the management menu.
 */
class AppManagementMenuPermissionsCoordinator {
    private final AppManagementMenuPermissionsView mView;
    AppManagementMenuPermissionsMediator mMediator;

    /**
     * Creates a new AppManagementMenuPermissionsCoordinator.
     * @param view The associated AppManagementMenuPermissionsView..
     * @param item The LaunchpadItem that is displaying in the management menu.
     */
    AppManagementMenuPermissionsCoordinator(
            Context context, AppManagementMenuPermissionsView view, LaunchpadItem item) {
        mView = view;

        mMediator = new AppManagementMenuPermissionsMediator(
                context, item.packageName, Origin.create(item.url));
        PropertyModelChangeProcessor.create(
                mMediator.getModel(), mView, AppManagementMenuPermissionsViewBinder::bind);
    }

    @VisibleForTesting
    AppManagementMenuPermissionsMediator getMediatorForTesting() {
        return mMediator;
    }

    void destroy() {
        mMediator = null;
    }
}
