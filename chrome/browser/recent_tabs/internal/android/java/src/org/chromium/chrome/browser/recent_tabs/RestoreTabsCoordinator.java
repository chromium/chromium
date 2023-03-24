// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Coordinator to manage the Restore Tabs on FRE feature.
 */
public class RestoreTabsCoordinator {
    private RestoreTabsMediator mMediator = new RestoreTabsMediator();
    private PropertyModel mModel = RestoreTabsProperties.createDefaultModel();

    public void initialize() {
        mMediator.initialize(mModel);
    }

    public void showOptions(Profile profile) {
        ForeignSessionHelper foreignSessionHelper = new ForeignSessionHelper(profile);
        List<ForeignSession> sessions = foreignSessionHelper.getForeignSessions();
        foreignSessionHelper.destroy();

        mMediator.showOptions(sessions);
    }
}
