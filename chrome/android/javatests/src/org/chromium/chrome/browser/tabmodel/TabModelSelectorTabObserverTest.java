// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
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
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @ClassRule
    public static final TabModelSelectorObserverTestRule sTestRule =
            new TabModelSelectorObserverTestRule();

    @Mock private TabDelegateFactory mTabDelegateFactory;
    private int mTabId;
    private Profile mProfile;
    private Profile mIncognitoProfile;

    @Before
    public void setUp() {
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfile = sTestRule.getNormalTabModel().getProfile();
                    mIncognitoProfile = sTestRule.getIncognitoTabModel().getProfile();
                });
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
                        });
        TestTabModelSelectorTabObserver observer = createTabModelSelectorTabObserver();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    selector.initialize(
                            TabModelHolderFactory.createTabModelHolderForTesting(
                                    sTestRule.getNormalTabModel()),
                            TabModelHolderFactory.createIncognitoTabModelHolderForTesting(
                                    sTestRule.getIncognitoTabModel()));
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
                    return TabBuilder.createForLazyLoad(
                                    incognito ? mIncognitoProfile : mProfile,
                                    new LoadUrlParams("about:blank"),
                                    /* title= */ null)
                            .setDelegateFactory(mTabDelegateFactory)
                            .setLaunchType(TabLaunchType.FROM_LINK)
                            .build();
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
                () ->
                        tabModel.getTabRemover()
                                .closeTabs(
                                        TabClosureParams.closeTab(tab).allowUndo(false).build(),
                                        /* allowDialog= */ false));
    }

    private static void removeTab(TabModel tabModel, Tab tab) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> tabModel.getTabRemover().removeTab(tab, /* allowDialog= */ false));
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
        assertTrue(tabHasObserver(tab, observer));
        assertTrue(
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return observer.isRegisteredTab(tab);
                        }));
    }

    private void assertTabDoesNotHaveObserver(
            Tab tab, TestTabModelSelectorTabObserver observer, boolean checkUnregistration) {
        assertFalse(tabHasObserver(tab, observer));
        if (!checkUnregistration) return;
        assertTrue(
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
