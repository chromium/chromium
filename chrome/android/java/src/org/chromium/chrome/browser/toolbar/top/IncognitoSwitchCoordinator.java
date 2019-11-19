// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.chromium.chrome.browser.toolbar.top.IncognitoSwitchProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.toolbar.top.IncognitoSwitchProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.toolbar.top.IncognitoSwitchProperties.ON_CHECKED_CHANGE_LISTENER;

import android.view.ViewGroup;
import android.widget.CompoundButton;
import android.widget.Switch;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The controller for the incognito switch. This class handles all interactions that the incognito
 * switch has with the outside world. It also acts as a mediator handling business logic and
 * updating the property model.
 */
class IncognitoSwitchCoordinator {
    private PropertyModel mPropertyModel;
    private TabModelSelector mTabModelSelector;
    private TabModelSelectorObserver mTabModelSelectorObserver;

    public IncognitoSwitchCoordinator(ViewGroup root, TabModelSelector tabModelSelector) {
        assert tabModelSelector != null;
        mTabModelSelector = tabModelSelector;

        Switch incognitoSwitchView = (Switch) root.findViewById(R.id.incognito_switch);
        assert incognitoSwitchView != null;

        initializePropertyModel();

        mTabModelSelectorObserver = new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                mPropertyModel.set(IS_INCOGNITO, mTabModelSelector.isIncognitoSelected());
            }
        };

        mTabModelSelector.addObserver(mTabModelSelectorObserver);

        PropertyModelChangeProcessor.create(
                mPropertyModel, incognitoSwitchView, IncognitoSwitchViewBinder::bind);
    }

    /**
     * Removes observers as necessary.
     */
    public void destroy() {
        mTabModelSelector.removeObserver(mTabModelSelectorObserver);
    }

    private void initializePropertyModel() {
        mPropertyModel = new PropertyModel(IncognitoSwitchProperties.ALL_KEYS);
        mPropertyModel.set(
                ON_CHECKED_CHANGE_LISTENER, new CompoundButton.OnCheckedChangeListener() {
                    @Override
                    public void onCheckedChanged(
                            CompoundButton buttonView, boolean incognitoSelected) {
                        setSelectedMode(incognitoSelected);
                    }
                });
        // Set the initial state.
        mPropertyModel.set(IS_INCOGNITO, mTabModelSelector.isIncognitoSelected());
        mPropertyModel.set(IS_VISIBLE, true);
    }

    private void setSelectedMode(boolean incognitoSelected) {
        if (incognitoSelected == mPropertyModel.get(IS_INCOGNITO)) return;
        mPropertyModel.set(IS_INCOGNITO, incognitoSelected);

        mTabModelSelector.commitAllTabClosures();
        mTabModelSelector.selectModel(incognitoSelected);
    }
}
