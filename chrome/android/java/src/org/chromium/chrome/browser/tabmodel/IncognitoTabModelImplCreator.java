// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelImpl.IncognitoTabModelDelegate;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;

/** Stores all the variables needed to create an Incognito TabModelImpl when it is needed. */
class IncognitoTabModelImplCreator implements IncognitoTabModelDelegate {
    private final ProfileProvider mProfileProvider;
    private final TabCreator mRegularTabCreator;
    private final TabCreator mIncognitoTabCreator;
    private final TabModelOrderController mOrderController;
    private final TabContentManager mTabContentManager;
    private final NextTabPolicySupplier mNextTabPolicySupplier;
    private final AsyncTabParamsManager mAsyncTabParamsManager;
    private final TabModelDelegate mModelDelegate;

    private final @ActivityType int mActivityType;

    /**
     * Constructor for an IncognitoTabModelImplCreator, used by {@link IncognitoTabModelImpl}.
     *
     * <p>Creating an instance of this class does not create the Incognito TabModelImpl immediately.
     * The {@link IncognitoTabModelImpl} will use this class to create the real TabModelImpl when it
     * will actually be used.
     *
     * @param profileProvider Provides access to the necessary Profiles for this model.
     * @param regularTabCreator Creates regular tabs.
     * @param incognitoTabCreator Creates incognito tabs.
     * @param orderController Determines the order for inserting new Tabs.
     * @param tabContentManager Manages the display content of the tab.
     * @param nextTabPolicySupplier Supplies the policy to pick a next tab if the current is closed
     * @param asyncTabParamsManager An {@link AsyncTabParamsManager} instance.
     * @param activityType Type of the activity for the tab model.
     * @param modelDelegate Delegate to handle external dependencies and interactions.
     */
    IncognitoTabModelImplCreator(
            ProfileProvider profileProvider,
            TabCreator regularTabCreator,
            TabCreator incognitoTabCreator,
            TabModelOrderController orderController,
            TabContentManager tabContentManager,
            NextTabPolicySupplier nextTabPolicySupplier,
            AsyncTabParamsManager asyncTabParamsManager,
            @ActivityType int activityType,
            TabModelDelegate modelDelegate) {
        mProfileProvider = profileProvider;
        mRegularTabCreator = regularTabCreator;
        mIncognitoTabCreator = incognitoTabCreator;
        mOrderController = orderController;
        mTabContentManager = tabContentManager;
        mNextTabPolicySupplier = nextTabPolicySupplier;
        mAsyncTabParamsManager = asyncTabParamsManager;
        mActivityType = activityType;
        mModelDelegate = modelDelegate;
    }

    @Override
    public TabModelInternal createTabModel() {
        return new TabModelImpl(
                mProfileProvider.getOffTheRecordProfile(true),
                mActivityType,
                mRegularTabCreator,
                mIncognitoTabCreator,
                mOrderController,
                mTabContentManager,
                mNextTabPolicySupplier,
                mAsyncTabParamsManager,
                mModelDelegate,
                /* supportUndo= */ false,
                /* isArchivedTabModel= */ false);
    }
}
