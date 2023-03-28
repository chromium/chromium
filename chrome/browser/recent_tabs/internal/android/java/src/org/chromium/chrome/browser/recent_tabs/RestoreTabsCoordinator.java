// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Coordinator to manage the Restore Tabs on FRE feature.
 */
public class RestoreTabsCoordinator {
    private ForeignSessionHelper mForeignSessionHelper;
    private RestoreTabsMediator mMediator;
    private PropertyModel mModel = RestoreTabsProperties.createDefaultModel();

    public RestoreTabsCoordinator(
            Profile profile, RestoreTabsControllerFactory.ControllerListener listener) {
        this(new ForeignSessionHelper(profile), new RestoreTabsMediator(), listener);
    }

    protected RestoreTabsCoordinator(ForeignSessionHelper helper, RestoreTabsMediator mediator,
            RestoreTabsControllerFactory.ControllerListener listener) {
        mForeignSessionHelper = helper;
        mMediator = mediator;
        mMediator.initialize(mModel, listener);
    }

    public void destroy() {
        mForeignSessionHelper.destroy();
        mForeignSessionHelper = null;
        mMediator.destroy();
        mMediator = null;
    }

    public void showOptions() {
        mMediator.showOptions(mForeignSessionHelper.getForeignSessions());
    }
}
