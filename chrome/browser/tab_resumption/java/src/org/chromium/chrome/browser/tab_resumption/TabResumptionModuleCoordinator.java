// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.view.ViewGroup;
import android.view.ViewStub;

import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The Coordinator for the tab resumption module, which can be embedded by surfaces like NTP or
 * Start surface.
 */
public class TabResumptionModuleCoordinator {
    private final TabResumptionModuleView mModuleView;
    private final TabResumptionModuleMediator mMediator;
    private final PropertyModel mModel;

    public TabResumptionModuleCoordinator(ViewGroup parent, int moduleContainerStubId) {
        mModel = new PropertyModel(TabResumptionModuleProperties.ALL_KEYS);

        ViewStub viewStub = parent.findViewById(moduleContainerStubId);
        mModuleView = (TabResumptionModuleView) viewStub.inflate();

        PropertyModelChangeProcessor.create(
                mModel, mModuleView, new TabResumptionModuleViewBinder());

        mMediator = new TabResumptionModuleMediator(mModel);
    }

    public void destroy() {
        mMediator.destroy();
        mModuleView.destroy();
    }

    public void reload() {
        mMediator.reload();
    }
}
