// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.support.test.InstrumentationRegistry;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.CommandLine;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.WindowAndroid;

import java.util.HashSet;
import java.util.Set;

/**
 * Basis for testing tab model selector observers.
 */
public class TabModelSelectorObserverTestRule extends ChromeBrowserTestRule {
    private TabModelSelectorBase mSelector;
    private TabModelSelectorTestTabModel mNormalTabModel;
    private TabModelSelectorTestTabModel mIncognitoTabModel;

    private WindowAndroid mWindowAndroid;

    public TabModelSelectorBase getSelector() {
        return mSelector;
    }

    public TabModelSelectorTestTabModel getNormalTabModel() {
        return mNormalTabModel;
    }

    public TabModelSelectorTestTabModel getIncognitoTabModel() {
        return mIncognitoTabModel;
    }

    public WindowAndroid getWindowAndroid() {
        return mWindowAndroid;
    }

    @Override
    public Statement apply(final Statement base, Description description) {
        CommandLine.init(null);
        return super.apply(new Statement() {
            @Override
            public void evaluate() throws Throwable {
                setUp();
                base.evaluate();
            }
        }, description);
    }

    private void setUp() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { initialize(); });
    }

    private void initialize() {
        mWindowAndroid = new WindowAndroid(InstrumentationRegistry.getInstrumentation()
                                                   .getTargetContext()
                                                   .getApplicationContext());

        mSelector = new TabModelSelectorBase(null, false) {
            @Override
            public Tab openNewTab(LoadUrlParams loadUrlParams, @TabLaunchType int type, Tab parent,
                    boolean incognito) {
                return null;
            }
        };

        TabModelOrderController orderController = new TabModelOrderControllerImpl(mSelector);
        TabContentManager tabContentManager =
                new TabContentManager(InstrumentationRegistry.getTargetContext(), null, false);
        TabPersistencePolicy persistencePolicy = new TabbedModeTabPersistencePolicy(0, false);
        TabPersistentStore tabPersistentStore =
                new TabPersistentStore(persistencePolicy, mSelector, null, null);

        TabModelDelegate delegate = new TabModelDelegate() {
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
            public boolean isCurrentModel(TabModel model) {
                return false;
            }

            @Override
            public boolean isInOverviewMode() {
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
            public boolean closeAllTabsRequest(boolean incognito) {
                return false;
            }
        };
        mNormalTabModel = new TabModelSelectorTestTabModel(
                false, orderController, tabContentManager, tabPersistentStore, delegate);

        mIncognitoTabModel = new TabModelSelectorTestTabModel(
                true, orderController, tabContentManager, tabPersistentStore, delegate);

        mSelector.initialize(mNormalTabModel, mIncognitoTabModel);
    }

    /**
     * Test TabModel that exposes the needed capabilities for testing.
     */
    public static class TabModelSelectorTestTabModel extends TabModelImpl {
        private Set<TabModelObserver> mObserverSet = new HashSet<>();

        public TabModelSelectorTestTabModel(boolean incognito,
                TabModelOrderController orderController, TabContentManager tabContentManager,
                TabPersistentStore tabPersistentStore, TabModelDelegate modelDelegate) {
            super(incognito, false, null, null, null, orderController, tabContentManager,
                    tabPersistentStore, modelDelegate, false);
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

        public Set<TabModelObserver> getObservers() {
            return mObserverSet;
        }
    }
}
