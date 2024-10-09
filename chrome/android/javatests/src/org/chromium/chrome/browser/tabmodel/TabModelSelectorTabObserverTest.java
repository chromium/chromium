// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.HashSet;
import java.util.Set;

/** Tests for the TabModelSelectorTabObserver. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class TabModelSelectorTabObserverTest {
    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;
    private int mTabId;

    @ClassRule
    public static final TabModelSelectorObserverTestRule sTestRule =
            new TabModelSelectorObserverTestRule();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Mockito.when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);
    }

    @Test
    @SmallTest
    public void testAddingTab() {
        TestTabModelSelectorTabObserver observer = createTabModelSelectorTabObserver();
        Tab tab = createTestTab(false);
        assertTabDoesNotHaveObserver(tab, observer, /* checkUnregistration= */ false);
        addTab(sTestRule.getNormalTabModel(), tab);
        assertTabHasObserver(tab, observer);
        destroyObserver(observer);
    }

    @Test
    @SmallTest
    public void testClosingTab() {
        TestTabModelSelectorTabObserver observer = createTabModelSelectorTabObserver();
        Tab tab = createTestTab(false);
        addTab(sTestRule.getNormalTabModel(), tab);
        assertTabHasObserver(tab, observer);
        closeTab(sTestRule.getNormalTabModel(), tab);
        assertTabDoesNotHaveObserver(tab, observer, true);
        destroyObserver(observer);
    }

    @Test
    @SmallTest
    public void testRemovingTab() {
        TestTabModelSelectorTabObserver observer = createTabModelSelectorTabObserver();
        Tab tab = createTestTab(false);
        addTab(sTestRule.getNormalTabModel(), tab);
        assertTabHasObserver(tab, observer);
        removeTab(sTestRule.getNormalTabModel(), tab);
        assertTabDoesNotHaveObserver(tab, observer, true);
        destroyObserver(observer);
    }

    @Test
    @SmallTest
    public void testPreExistingTabs() {
        Tab normalTab1 = createTestTab(false);
        addTab(sTestRule.getNormalTabModel(), normalTab1);
        Tab normalTab2 = createTestTab(false);
        addTab(sTestRule.getNormalTabModel(), normalTab2);

        Tab incognitoTab1 = createTestTab(true);
        addTab(sTestRule.getIncognitoTabModel(), incognitoTab1);
        Tab incognitoTab2 = createTestTab(true);
        addTab(sTestRule.getIncognitoTabModel(), incognitoTab2);

        TestTabModelSelectorTabObserver observer = createTabModelSelectorTabObserver();
        assertTabHasObserver(normalTab1, observer);
        assertTabHasObserver(normalTab2, observer);
        assertTabHasObserver(incognitoTab1, observer);
        assertTabHasObserver(incognitoTab2, observer);
        destroyObserver(observer);
    }

    @Test
    @SmallTest
    public void testDestroyRemovesObserver() {
        Tab normalTab1 = createTestTab(false);
        addTab(sTestRule.getNormalTabModel(), normalTab1);
        Tab incognitoTab1 = createTestTab(true);
        addTab(sTestRule.getIncognitoTabModel(), incognitoTab1);

        TestTabModelSelectorTabObserver observer = createTabModelSelectorTabObserver();
        assertTabHasObserver(normalTab1, observer);
        assertTabHasObserver(incognitoTab1, observer);

        destroyObserver(observer);
        assertTabDoesNotHaveObserver(normalTab1, observer, true);
        assertTabDoesNotHaveObserver(incognitoTab1, observer, true);
    }

    @Test
    @SmallTest
    public void testObserverAddedBeforeInitialize() {
        TabModelSelectorBase selector =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return new TabModelSelectorBase(null, false) {
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
                        });
        TestTabModelSelectorTabObserver observer = createTabModelSelectorTabObserver();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.initialize(
                            sTestRule.getNormalTabModel(), sTestRule.getIncognitoTabModel());
                });

        Tab normalTab1 = createTestTab(false);
        addTab(sTestRule.getNormalTabModel(), normalTab1);
        assertTabHasObserver(normalTab1, observer);

        Tab incognitoTab1 = createTestTab(true);
        addTab(sTestRule.getIncognitoTabModel(), incognitoTab1);
        assertTabHasObserver(incognitoTab1, observer);
        destroyObserver(observer);
    }

    private TestTabModelSelectorTabObserver createTabModelSelectorTabObserver() {
        final TestTabModelSelectorTabObserver observer =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> new TestTabModelSelectorTabObserver(sTestRule.getSelector()));
        // Initially tabs are added in deferred state, wait for this to complete before proceeding
        // to ensure all tabs are registered. In production the observer should only ever be
        // interacted with on the UI thread so this is a non-issue. However, in this test asserts
        // may run on the instrumentation thread.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            observer.isDeferredInitializationFinishedForTesting(),
                            Matchers.is(true));
                });
        return observer;
    }

    private void destroyObserver(TestTabModelSelectorTabObserver observer) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    observer.destroy();
                });
    }

    private Tab createTestTab(boolean incognito) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return new MockTab(
                            Tab.INVALID_TAB_ID, incognito ? mIncognitoProfile : mProfile);
                });
    }

    private static void addTab(TabModel tabModel, Tab tab) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        tabModel.addTab(
                                tab,
                                0,
                                TabLaunchType.FROM_LINK,
                                TabCreationState.LIVE_IN_FOREGROUND));
    }

    private static void closeTab(TabModel tabModel, Tab tab) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> tabModel.closeTabs(TabClosureParams.closeTab(tab).allowUndo(false).build()));
    }

    private static void removeTab(TabModel tabModel, Tab tab) {
        ThreadUtils.runOnUiThreadBlocking(() -> tabModel.removeTab(tab));
    }

    private static class TestTabModelSelectorTabObserver extends TabModelSelectorTabObserver {
        public final Set<Tab> mRegisteredTabs = new HashSet<>();
        public final Set<Tab> mUnregisteredTabs = new HashSet<>();

        public TestTabModelSelectorTabObserver(TabModelSelectorBase selector) {
            super(selector);
        }

        @Override
        protected void onTabRegistered(Tab tab) {
            mRegisteredTabs.add(tab);
        }

        @Override
        protected void onTabUnregistered(Tab tab) {
            mUnregisteredTabs.add(tab);
        }

        private boolean isRegisteredTab(Tab tab) {
            return mRegisteredTabs.contains(tab);
        }

        private boolean isUnregisteredTab(Tab tab) {
            return mUnregisteredTabs.contains(tab);
        }
    }

    private void assertTabHasObserver(Tab tab, TestTabModelSelectorTabObserver observer) {
        Assert.assertTrue(tabHasObserver(tab, observer));
        Assert.assertTrue(
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return observer.isRegisteredTab(tab);
                        }));
    }

    private void assertTabDoesNotHaveObserver(
            Tab tab, TestTabModelSelectorTabObserver observer, boolean checkUnregistration) {
        Assert.assertFalse(tabHasObserver(tab, observer));
        if (!checkUnregistration) return;
        Assert.assertTrue(
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return observer.isUnregisteredTab(tab);
                        }));
    }

    private static boolean tabHasObserver(Tab tab, TestTabModelSelectorTabObserver observer) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    RewindableIterator<TabObserver> tabObservers =
                            TabTestUtils.getTabObservers(tab);
                    tabObservers.rewind();
                    boolean found = false;
                    while (tabObservers.hasNext()) found |= observer.equals(tabObservers.next());
                    return found;
                });
    }
}
