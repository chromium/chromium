// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

/** Populates a {@link ModelList} with an item for each group. */
public class CrossDeviceListMediator {
    private final ModelList mModelList;

    /**
     * @param modelList List of group items to appear on the cross device pane.
     */
    public CrossDeviceListMediator(ModelList modelList) {
        mModelList = modelList;

        buildModelList();
    }

    /** Clean up objects used by this class. */
    public void destroy() {
        mModelList.clear();
    }

    /** Clean up the cross device data displayed by the pane. */
    public void clearModelList() {
        mModelList.clear();
    }

    /** Build the cross device data displayed by the pane. */
    public void buildModelList() {
        mModelList.clear();
    }
}
