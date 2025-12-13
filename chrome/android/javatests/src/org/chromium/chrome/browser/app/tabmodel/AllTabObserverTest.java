// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserverTestRule;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Tests for the {@link AllTabObserver}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_STARTUP_PROMOS
})
@Batch(Batch.PER_CLASS)
public class AllTabObserverTest {
    @ClassRule
    public static final TabModelSelectorObserverTestRule sTestRule =
            new TabModelSelectorObserverTestRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Profile mProfile;
    @Mock private TabWindowManager mTabWindowManager;
    @Mock private TabDelegateFactory mTabDelegateFactory;

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);
                });

        final List<TabModelSelector> selectors = List.of(sTestRule.getSelector());
        when(mTabWindowManager.getAllTabModelSelectors()).thenReturn(selectors);
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AllTabObserver.resetForTesting();
                    TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();
                });
    }

    @Test
    @SmallTest
    public void testTabAddingAndRemoving() {
        final TestAllTabObserver observer = createTestAllTabObserver();

        // Should be empty initially.
        CriteriaHelper.pollUiThread(() -> observer.mOpenTabs.isEmpty());

        // Add a tab to the selector.
        final Tab tab = addTab();
        CriteriaHelper.pollUiThread(() -> observer.mOpenTabs.contains(tab));

        // Remove the tab.
        closeTab(tab);
        CriteriaHelper.pollUiThread(() -> !observer.mOpenTabs.contains(tab));

        destroyObserver(observer);
    }

    @Test
    @SmallTest
    public void testReportsTabsBeforeCreation() {
        // Add a tab before creating observer.
        final Tab tab = addTab();
        final TestAllTabObserver observer = createTestAllTabObserver();

        // Should contain the tab.
        CriteriaHelper.pollUiThread(() -> observer.mOpenTabs.contains(tab));

        // Tab should be gone after removing.
        closeTab(tab);
        CriteriaHelper.pollUiThread(() -> !observer.mOpenTabs.contains(tab));

        destroyObserver(observer);
    }

    @Test
    @SmallTest
    public void testCustomTabReportsTabsBeforeCreation() {
        final Tab tab = ThreadUtils.runOnUiThreadBlocking(() -> new MockTab(0, mProfile));
        ThreadUtils.runOnUiThreadBlocking(() -> AllTabObserver.addCustomTab(tab));

        final TestAllTabObserver observer = createTestAllTabObserver();

        // Should contain the new custom tab we added.
        CriteriaHelper.pollUiThread(() -> observer.mOpenTabs.size() == 1);
        assertTrue(ThreadUtils.runOnUiThreadBlocking(() -> observer.mOpenTabs.contains(tab)));

        // Remove custom tab.
        ThreadUtils.runOnUiThreadBlocking(() -> AllTabObserver.removeCustomTab(tab));

        // Should no longer contain the new tab we added.
        CriteriaHelper.pollUiThread(() -> observer.mOpenTabs.isEmpty());
        assertFalse(ThreadUtils.runOnUiThreadBlocking(() -> observer.mOpenTabs.contains(tab)));

        destroyObserver(observer);
    }

    @Test
    @SmallTest
    public void testCustomTabAddingAndRemoving() {
        final TestAllTabObserver observer1 = createTestAllTabObserver();
        final TestAllTabObserver observer2 = createTestAllTabObserver();

        // Should be empty initially.
        CriteriaHelper.pollUiThread(() -> observer1.mOpenTabs.isEmpty());
        CriteriaHelper.pollUiThread(() -> observer2.mOpenTabs.isEmpty());

        final Tab tab = ThreadUtils.runOnUiThreadBlocking(() -> new MockTab(0, mProfile));
        ThreadUtils.runOnUiThreadBlocking(() -> AllTabObserver.addCustomTab(tab));

        // Should contain the new tab we added.
        CriteriaHelper.pollUiThread(() -> observer1.mOpenTabs.size() == 1);
        CriteriaHelper.pollUiThread(() -> observer2.mOpenTabs.size() == 1);
        assertTrue(ThreadUtils.runOnUiThreadBlocking(() -> observer1.mOpenTabs.contains(tab)));
        assertTrue(ThreadUtils.runOnUiThreadBlocking(() -> observer2.mOpenTabs.contains(tab)));

        // Remove custom tab.
        ThreadUtils.runOnUiThreadBlocking(() -> AllTabObserver.removeCustomTab(tab));

        // Should no longer contain the new tab we added.
        CriteriaHelper.pollUiThread(() -> observer1.mOpenTabs.isEmpty());
        CriteriaHelper.pollUiThread(() -> observer2.mOpenTabs.isEmpty());
        assertFalse(ThreadUtils.runOnUiThreadBlocking(() -> observer1.mOpenTabs.contains(tab)));
        assertFalse(ThreadUtils.runOnUiThreadBlocking(() -> observer2.mOpenTabs.contains(tab)));

        destroyObserver(observer1);
        destroyObserver(observer2);
    }

    private Tab addTab() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final Profile profile = sTestRule.getNormalTabModel().getProfile();
                    final Tab tab =
                            TabBuilder.createForLazyLoad(
                                            profile, new LoadUrlParams("about:blank"), null)
                                    .setDelegateFactory(mTabDelegateFactory)
                                    .setLaunchType(TabLaunchType.FROM_LINK)
                                    .build();
                    sTestRule
                            .getNormalTabModel()
                            .addTab(
                                    tab,
                                    0,
                                    TabLaunchType.FROM_LINK,
                                    TabCreationState.LIVE_IN_FOREGROUND);
                    return tab;
                });
    }

    private void closeTab(Tab tab) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sTestRule
                            .getNormalTabModel()
                            .getTabRemover()
                            .removeTab(tab, /* allowDialog= */ false);
                });
    }

    private TestAllTabObserver createTestAllTabObserver() {
        return ThreadUtils.runOnUiThreadBlocking(() -> new TestAllTabObserver());
    }

    private void destroyObserver(TestAllTabObserver observer) {
        ThreadUtils.runOnUiThreadBlocking(() -> observer.destroy());
    }

    private static class TestAllTabObserver implements AllTabObserver.Observer {
        public final Set<Tab> mOpenTabs = new HashSet<>();
        private final AllTabObserver mObserver = new AllTabObserver(this);

        @Override
        public void onTabAdded(Tab tab) {
            assertFalse(mOpenTabs.contains(tab));
            mOpenTabs.add(tab);
        }

        @Override
        public void onTabRemoved(Tab tab) {
            final var wasRemoved = mOpenTabs.remove(tab);
            assertTrue(wasRemoved);
        }

        public void destroy() {
            mObserver.destroy();
        }
    }
}
