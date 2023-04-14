// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Coordinator to manage the Restore Tabs on FRE feature.
 */
public class RestoreTabsCoordinator {
    private RestoreTabsMediator mMediator;
    private PropertyModel mModel = RestoreTabsProperties.createDefaultModel();

    public RestoreTabsCoordinator(Profile profile,
            RestoreTabsControllerFactory.ControllerListener listener,
            TabCreatorManager tabCreatorManager) {
        this(profile, new RestoreTabsMediator(), listener, tabCreatorManager);
    }

    protected RestoreTabsCoordinator(Profile profile, RestoreTabsMediator mediator,
            RestoreTabsControllerFactory.ControllerListener listener,
            TabCreatorManager tabCreatorManager) {
        mMediator = mediator;
        mMediator.initialize(mModel, listener, profile, tabCreatorManager);
    }

    public void destroy() {
        mMediator.destroy();
        mMediator = null;
    }

    public void showOptions() {
        mMediator.showOptions();
    }
}
