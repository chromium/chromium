// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.NewTabTileViewProperties.IS_INCOGNITO;

import org.chromium.base.MathUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This is mediator for NewTabTile component.
 */
public class NewTabTileMediator {
    private final TabModelSelector mTabModelSelector;
    private final TabModelSelectorObserver mTabModelSelectorObserver;

    NewTabTileMediator(PropertyModel model, TabModelSelector tabModelSelector,
            TabCreatorManager tabCreatorManager) {
        mTabModelSelector = tabModelSelector;

        // Deliberately use un-cached value to match with native.
        float aspectRatio = (float) ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID,
                TabUiFeatureUtilities.THUMBNAIL_ASPECT_RATIO_PARAM, 1.0);
        aspectRatio = MathUtils.clamp(aspectRatio, 0.5f, 2.0f);
        model.set(NewTabTileViewProperties.THUMBNAIL_ASPECT_RATIO, aspectRatio);
        model.set(NewTabTileViewProperties.CARD_HEIGHT_INTERCEPT, 0);
        model.set(NewTabTileViewProperties.ON_CLICK_LISTENER, view -> {
            tabCreatorManager.getTabCreator(tabModelSelector.isIncognitoSelected()).launchNTP();
            RecordUserAction.record("MobileNewTabOpened.NewTabTile");
        });

        mTabModelSelectorObserver = new TabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                model.set(IS_INCOGNITO, newModel.isIncognito());
            }
        };
        tabModelSelector.addObserver(mTabModelSelectorObserver);
    }

    public void destroy() {
        mTabModelSelector.removeObserver(mTabModelSelectorObserver);
    }
}