// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;

/** Unit tests for {@link CustomTabCountObserver}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CustomTabCountObserverUnitTest {
    @Rule
    public CustomTabActivityContentTestEnvironment env =
            new CustomTabActivityContentTestEnvironment();

    @After
    public void tearDown() {
        UmaRecorderHolder.resetForTesting();
    }

    @Test
    public void observeBeforeInitialTab() {
        env.tabProvider.setInitialTab(env.prepareTab(), TabCreationMode.DEFAULT);
        new CustomTabCountObserver(env.tabProvider);

        assertTabCountsRecorded(1, "Initial Tab created.");
    }

    @Test
    public void observerMultipleTabs() {
        Tab tab1 = newTabWithId(1);
        Tab tab2 = newTabWithId(2);
        Tab tab3 = newTabWithId(3);

        new CustomTabCountObserver(env.tabProvider);
        env.tabProvider.setInitialTab(tab1, TabCreationMode.DEFAULT);
        assertTabCountsRecorded(1, "Initial Tab created.");

        // Assuming tab 2 created by clicking target=_blank.
        env.tabProvider.swapTab(tab2);
        assertTabCountsRecorded(2, "Switched to new tab.");

        // Assuming tab 2 closed.
        env.tabProvider.swapTab(tab1);
        assertTabCountsRecorded(2, "No new counts when switching into previous tab.");

        // Assuming tab 3 created.
        env.tabProvider.swapTab(tab3);
        assertTabCountsRecorded(3, "3rd tab created.");

        // Assuming all tab closed.
        env.tabProvider.removeTab();
        assertTabCountsRecorded(3, "No new counts when all tabs closing.");
    }

    private void assertTabCountsRecorded(int count, String reason) {
        String histogram = "CustomTabs.TabCounts.UniqueTabsSeen";
        Assert.assertEquals(
                String.format(
                        "<%s> with should recorded <%d> times. Reason: %s",
                        histogram, count, reason),
                count,
                RecordHistogram.getHistogramTotalCountForTesting(histogram));
        Assert.assertEquals(
                String.format(
                        "<%s> with sample <%d> is not recorded. Reason: %s",
                        histogram, count, reason),
                1,
                RecordHistogram.getHistogramValueCountForTesting(histogram, count));
    }

    private Tab newTabWithId(int id) {
        Tab tab = env.prepareTab();
        Mockito.doReturn(id).when(tab).getId();
        return tab;
    }
}
