// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.test.core.app.ApplicationProvider;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.HashSet;
import java.util.Set;

/** Basis for testing tab model selector observers. */
public class TabModelSelectorObserverTestRule extends ChromeBrowserTestRule {
    // Test activity type that does not restore tab on cold restart.
    // Any type other than ActivityType.TABBED works.
    private static final @ActivityType int NO_RESTORE_TYPE = ActivityType.CUSTOM_TAB;
    private TabModelSelectorBase mSelector;
    private TabModelSelectorTestTabModel mNormalTabModel;
    private TabModelSelectorTestIncognitoTabModel mIncognitoTabModel;

    public TabModelSelectorBase getSelector() {
        return mSelector;
    }

    public TabModelSelectorTestTabModel getNormalTabModel() {
        return mNormalTabModel;
    }

    public TabModelSelectorTestIncognitoTabModel getIncognitoTabModel() {
        return mIncognitoTabModel;
    }

    @Override
    public Statement apply(final Statement base, Description description) {
        return super.apply(
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        setUp();
                        base.evaluate();
                    }
                },
                description);
    }

    private void setUp() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    initialize();
                });
    }

    private void initialize() {
        mSelector =
                new TabModelSelectorBase(null, false) {
                    @Override
                    public void requestToShowTab(Tab tab, int type) {}

                    @Override
                    public boolean isTabModelRestored() {
                        return true;
                    }

                    @Override
                    public Tab openNewTab(
                            LoadUrlParams loadUrlParams,
                            @TabLaunchType int type,
                            Tab parent,
                            boolean incognito) {
                        return null;
                    }
                };

        TabModelOrderController orderController = new TabModelOrderControllerImpl(mSelector);
        TabContentManager tabContentManager =
                new TabContentManager(
                        ApplicationProvider.getApplicationContext(),
                        null,
                        false,
                        mSelector::getTabById,
                        TabWindowManagerSingleton.getInstance());
        tabContentManager.initWithNative();
        NextTabPolicySupplier nextTabPolicySupplier = () -> NextTabPolicy.HIERARCHICAL;
        AsyncTabParamsManager asyncTabParamsManager = AsyncTabParamsManagerSingleton.getInstance();

        TabModelDelegate delegate =
                new TabModelDelegate() {
                    @Override
                    public void selectModel(boolean incognito) {
                        mSelector.selectModel(incognito);
                    }

                    @Override
                    public void requestToShowTab(Tab tab, @TabSelectionType int type) {}

                    @Override
                    public boolean isTabModelRestored() {
                        return true;
                    }

                    @Override
                    public TabModel getModel(boolean incognito) {
                        return mSelector.getModel(incognito);
                    }

                    @Override
                    public TabGroupModelFilter getFilter(boolean incognito) {
                        return mSelector
                                .getTabGroupModelFilterProvider()
                                .getTabGroupModelFilter(incognito);
                    }

                    @Override
                    public TabModel getCurrentModel() {
                        return mSelector.getCurrentModel();
                    }

                    @Override
                    public boolean isReparentingInProgress() {
                        return false;
                    }
                };

        TabRemover normalTabRemover =
                new PassthroughTabRemover(
                        () ->
                                mSelector
                                        .getTabGroupModelFilterProvider()
                                        .getTabGroupModelFilter(/* isIncognito= */ false));
        TabUngrouper normalTabUngrouper =
                new PassthroughTabUngrouper(
                        () ->
                                mSelector
                                        .getTabGroupModelFilterProvider()
                                        .getTabGroupModelFilter(/* isIncognito= */ false));
        mNormalTabModel =
                new TabModelSelectorTestTabModel(
                        ProfileManager.getLastUsedRegularProfile(),
                        orderController,
                        tabContentManager,
                        nextTabPolicySupplier,
                        asyncTabParamsManager,
                        NO_RESTORE_TYPE,
                        delegate,
                        normalTabRemover,
                        normalTabUngrouper);

        TabRemover incognitoTabRemover =
                new PassthroughTabRemover(
                        () ->
                                mSelector
                                        .getTabGroupModelFilterProvider()
                                        .getTabGroupModelFilter(/* isIncognito= */ true));
        TabUngrouper incognitoTabUngrouper =
                new PassthroughTabUngrouper(
                        () ->
                                mSelector
                                        .getTabGroupModelFilterProvider()
                                        .getTabGroupModelFilter(/* isIncognito= */ true));
        mIncognitoTabModel =
                new TabModelSelectorTestIncognitoTabModel(
                        ProfileManager.getLastUsedRegularProfile()
                                .getPrimaryOtrProfile(/* createIfNeeded= */ true),
                        orderController,
                        tabContentManager,
                        nextTabPolicySupplier,
                        asyncTabParamsManager,
                        delegate,
                        incognitoTabRemover,
                        incognitoTabUngrouper);

        mSelector.initialize(
                new TabModelHolder(mNormalTabModel, mNormalTabModel),
                new IncognitoTabModelHolder(mIncognitoTabModel, mIncognitoTabModel));
    }

    /** Test TabModel that exposes the needed capabilities for testing. */
    public static class TabModelSelectorTestTabModel extends TabCollectionTabModelImpl
            implements IncognitoTabModelInternal {
        private final Set<TabModelObserver> mObserverSet = new HashSet<>();

        public TabModelSelectorTestTabModel(
                Profile profile,
                TabModelOrderController orderController,
                TabContentManager tabContentManager,
                NextTabPolicySupplier nextTabPolicySupplier,
                AsyncTabParamsManager asyncTabParamsManager,
                @ActivityType int activityType,
                TabModelDelegate modelDelegate,
                TabRemover tabRemover,
                TabUngrouper tabUngrouper) {
            super(
                    profile,
                    activityType,
                    /* isArchivedTabModel= */ false,
                    null,
                    null,
                    orderController,
                    tabContentManager,
                    nextTabPolicySupplier,
                    modelDelegate,
                    asyncTabParamsManager,
                    tabRemover,
                    tabUngrouper,
                    /* supportUndo= */ false);
        }

        @Override
        public void initializeNative(int activityType, boolean isArchivedTabModel) {
            // Skip setting up the TabModelObserverJniBridge by using isArchivedTabModel = true.
            // Initializing this leads to unexpected observers being added and crashes due to
            // mObserverSet not being initialized. This test should be refactored.
            super.initializeNative(activityType, /* isArchivedTabModel= */ true);
        }

        @Override
        public void addObserver(TabModelObserver observer) {
            super.addObserver(observer);
            mObserverSet.add(observer);
        }

        @Override
        public void removeObserver(TabModelObserver observer) {
            super.removeObserver(observer);
            mObserverSet.remove(observer);
        }

        @Override
        public void addDelegateModelObserver(Callback<TabModelInternal> callback) {}

        @Override
        public void addIncognitoObserver(IncognitoTabModelObserver observer) {}

        @Override
        public void removeIncognitoObserver(IncognitoTabModelObserver observer) {}

        public Set<TabModelObserver> getObservers() {
            return mObserverSet;
        }
    }

    /** Test IncognitoTabModel that exposes the needed capabilities for testing. */
    public static class TabModelSelectorTestIncognitoTabModel extends TabModelSelectorTestTabModel
            implements IncognitoTabModel {
        public TabModelSelectorTestIncognitoTabModel(
                Profile profile,
                TabModelOrderController orderController,
                TabContentManager tabContentManager,
                NextTabPolicySupplier nextTabPolicySupplier,
                AsyncTabParamsManager asyncTabParamsManager,
                TabModelDelegate modelDelegate,
                TabRemover tabRemover,
                TabUngrouper tabUngrouper) {
            super(
                    ProfileManager.getLastUsedRegularProfile()
                            .getPrimaryOtrProfile(/* createIfNeeded= */ true),
                    orderController,
                    tabContentManager,
                    nextTabPolicySupplier,
                    asyncTabParamsManager,
                    NO_RESTORE_TYPE,
                    modelDelegate,
                    tabRemover,
                    tabUngrouper);
        }

        @Override
        public void addIncognitoObserver(IncognitoTabModelObserver observer) {}

        @Override
        public void removeIncognitoObserver(IncognitoTabModelObserver observer) {}
    }
}
