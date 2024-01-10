// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import org.chromium.ui.modelutil.PropertyModel;

/** The Mediator for the tab resumption module. */
public class TabResumptionModuleMediator {
    private final PropertyModel mModel;

    TabResumptionModuleMediator(PropertyModel model) {
        mModel = model;
        start();
    }

    void destroy() {}

    /** Reloads the module. */
    public void reload() {
        start();
    }

    /** Main logic to fetch and render suggestions. */
    private void start() {
        // TODO(crbug.com/1515325): Implement.
        mModel.set(TabResumptionModuleProperties.IS_VISIBLE, true);
    }
}
