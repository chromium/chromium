// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.NEW_TAB_TILE;

import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This is the coordinator for NewTabTile component.
 */
public class NewTabTileCoordinator {
    private final PropertyModel mModel;
    private final NewTabTileMediator mMediator;

    NewTabTileCoordinator(TabModelSelector tabModelSelector, TabCreatorManager tabCreatorManager) {
        mModel = new PropertyModel.Builder(NewTabTileViewProperties.ALL_KEYS)
                         .with(CARD_TYPE, NEW_TAB_TILE)
                         .build();
        mMediator = new NewTabTileMediator(mModel, tabModelSelector, tabCreatorManager);
    }

    public PropertyModel getModel() {
        return mModel;
    }

    public void destroy() {
        mMediator.destroy();
    }
}
