// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;

import java.util.HashSet;
import java.util.Set;

/** Tests for the {@link AllTabObsever}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_STARTUP_PROMOS
})
@Batch(Batch.PER_CLASS)
public class AllTabObserverTest {
    private static final String TEST_PATH = "/chrome/test/data/android/about.html";

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Test
    @SmallTest
    public void testTabAddingAndRemoving() {
        TestAllTabObserver observer = createTestAllTabObserver();
        // Should contain the blank tab.
        CriteriaHelper.pollUiThread(() -> observer.mOpenTabs.size() == 1);
        Tab tab = addTab();
        assertTrue(observer.mOpenTabs.contains(tab));
        closeTab(tab);
        CriteriaHelper.pollUiThread(() -> observer.mOpenTabs.size() == 1);
        assertFalse(observer.mOpenTabs.contains(tab));
        destroyObserver(observer);
    }

    @Test
    @SmallTest
    public void testReportsTabsBeforeCreation() {
        Tab tab = addTab();
        TestAllTabObserver observer = createTestAllTabObserver();
        // Should contain the blank tab and the new tab we added.
        CriteriaHelper.pollUiThread(() -> observer.mOpenTabs.size() == 2);
        assertTrue(observer.mOpenTabs.contains(tab));
        closeTab(tab);
        CriteriaHelper.pollUiThread(() -> observer.mOpenTabs.size() == 1);
        assertFalse(observer.mOpenTabs.contains(tab));
        destroyObserver(observer);
    }

    private TestAllTabObserver createTestAllTabObserver() {
        return ThreadUtils.runOnUiThreadBlocking(() -> new TestAllTabObserver());
    }

    private void destroyObserver(TestAllTabObserver observer) {
        ThreadUtils.runOnUiThreadBlocking(() -> observer.destroy());
    }

    private Tab addTab() {
        return sActivityTestRule.loadUrlInNewTab(
                sActivityTestRule.getTestServer().getURL(TEST_PATH), /* incognito= */ false);
    }

    private void closeTab(Tab tab) {
        final TabModel model =
                sActivityTestRule
                        .getActivity()
                        .getTabModelSelectorSupplier()
                        .get()
                        .getCurrentModel();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var wasFound =
                            TabModelUtils.closeTabById(model, tab.getId(), /* allowUndo= */ false);
                    assertTrue(wasFound);
                });
    }

    private static class TestAllTabObserver implements AllTabObserver.Observer {
        private final AllTabObserver mObserver = new AllTabObserver(this);
        public final Set<Tab> mOpenTabs = new HashSet<>();

        @Override
        public void onTabAdded(Tab tab) {
            assertFalse(mOpenTabs.contains(tab));
            mOpenTabs.add(tab);
        }

        @Override
        public void onTabRemoved(Tab tab) {
            var wasRemoved = mOpenTabs.remove(tab);
            assertTrue(wasRemoved);
        }

        public void destroy() {
            mObserver.destroy();
        }
    }
}
