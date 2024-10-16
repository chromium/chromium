// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.test.core.app.ApplicationProvider;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.CommandLine;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
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
        CommandLine.init(null);
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
                    public boolean isSessionRestoreInProgress() {
                        return false;
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
                    public boolean isSessionRestoreInProgress() {
                        return false;
                    }

                    @Override
                    public TabModel getModel(boolean incognito) {
                        return mSelector.getModel(incognito);
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

        mNormalTabModel =
                new TabModelSelectorTestTabModel(
                        ProfileManager.getLastUsedRegularProfile(),
                        orderController,
                        tabContentManager,
                        nextTabPolicySupplier,
                        asyncTabParamsManager,
                        NO_RESTORE_TYPE,
                        delegate);

        mIncognitoTabModel =
                new TabModelSelectorTestIncognitoTabModel(
                        ProfileManager.getLastUsedRegularProfile()
                                .getPrimaryOTRProfile(/* createIfNeeded= */ true),
                        orderController,
                        tabContentManager,
                        nextTabPolicySupplier,
                        asyncTabParamsManager,
                        delegate);

        mSelector.initialize(mNormalTabModel, mIncognitoTabModel);
    }

    /** Test TabModel that exposes the needed capabilities for testing. */
    public static class TabModelSelectorTestTabModel extends TabModelImpl
            implements IncognitoTabModelInternal {
        private Set<TabModelObserver> mObserverSet = new HashSet<>();

        public TabModelSelectorTestTabModel(
                Profile profile,
                TabModelOrderController orderController,
                TabContentManager tabContentManager,
                NextTabPolicySupplier nextTabPolicySupplier,
                AsyncTabParamsManager asyncTabParamsManager,
                @ActivityType int activityType,
                TabModelDelegate modelDelegate) {
            super(
                    profile,
                    activityType,
                    null,
                    null,
                    orderController,
                    tabContentManager,
                    nextTabPolicySupplier,
                    asyncTabParamsManager,
                    modelDelegate,
                    /* supportUndo= */ false,
                    /* trackInNativeModelList= */ true);
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
        public void addIncognitoObserver(IncognitoTabModelObserver observer) {}

        @Override
        public void removeIncognitoObserver(IncognitoTabModelObserver observer) {}

        public Set<TabModelObserver> getObservers() {
            return mObserverSet;
        }
    }

    /** Test IncognitoTabModel that exposes the needed capabilities for testing. */
    private static class TabModelSelectorTestIncognitoTabModel extends TabModelSelectorTestTabModel
            implements IncognitoTabModel {
        public TabModelSelectorTestIncognitoTabModel(
                Profile profile,
                TabModelOrderController orderController,
                TabContentManager tabContentManager,
                NextTabPolicySupplier nextTabPolicySupplier,
                AsyncTabParamsManager asyncTabParamsManager,
                TabModelDelegate modelDelegate) {
            super(
                    ProfileManager.getLastUsedRegularProfile()
                            .getPrimaryOTRProfile(/* createIfNeeded= */ true),
                    orderController,
                    tabContentManager,
                    nextTabPolicySupplier,
                    asyncTabParamsManager,
                    NO_RESTORE_TYPE,
                    modelDelegate);
        }

        @Override
        public void addIncognitoObserver(IncognitoTabModelObserver observer) {}

        @Override
        public void removeIncognitoObserver(IncognitoTabModelObserver observer) {}
    }
}
