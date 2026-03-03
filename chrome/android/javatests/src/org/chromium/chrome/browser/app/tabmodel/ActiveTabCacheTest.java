// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import android.content.Context;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.app.tabmodel.ActiveTabCache.CachedActiveTab;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;

import java.io.File;
import java.util.concurrent.ExecutionException;

/** Integration tests for {@link ActiveTabCache}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class ActiveTabCacheTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private final CipherFactory mCipherFactory = new CipherFactory();
    private ActiveTabCache mActiveTabCache;

    @Before
    public void setUp() {
        mActiveTabCache = new ActiveTabCache("0");
    }

    private File getActiveTabFile(boolean incognito) {
        String fileName = incognito ? "0_incognito" : "0_regular";
        return new File(
                ContextUtils.getApplicationContext().getDir("active_tabs", Context.MODE_PRIVATE),
                fileName);
    }

    @Test
    @MediumTest
    public void testSaveAndRestore() throws ExecutionException {
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        Tab tab = page.getTab();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActiveTabCache.saveActiveTab(tab, 0, null);
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    CachedActiveTab cachedTab = mActiveTabCache.restoreActiveTab(false, null);
                    assertNotNull("Cached tab should not be null", cachedTab);
                    assertEquals("Tab index should match", 0, cachedTab.tabIndex);
                    assertNotNull("Tab state should not be null", cachedTab.tabState);
                    assertEquals(
                            "URL should match",
                            tab.getUrl().getSpec(),
                            cachedTab.tabState.url.getSpec());
                });
    }

    @Test
    @MediumTest
    public void testSaveAndRestoreIncognito() throws ExecutionException {
        WebPageStation page = mActivityTestRule.startOnIncognitoBlankPage();
        Tab tab = page.getTab();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActiveTabCache.saveActiveTab(tab, 0, mCipherFactory);
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    CachedActiveTab cachedTab =
                            mActiveTabCache.restoreActiveTab(true, mCipherFactory);
                    assertNotNull("Cached incognito tab should not be null", cachedTab);
                    assertEquals("Tab index should match", 0, cachedTab.tabIndex);
                    assertNotNull("Tab state should not be null", cachedTab.tabState);
                    assertEquals(
                            "URL should match",
                            tab.getUrl().getSpec(),
                            cachedTab.tabState.url.getSpec());
                });
    }

    @Test
    @MediumTest
    public void testReplaceActiveTab() throws ExecutionException {
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        Tab tab = page.getTab();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActiveTabCache.saveActiveTab(tab, 0, null);
                });

        CriteriaHelper.pollInstrumentationThread(
                () -> getActiveTabFile(false).exists(), "Active tab file should exist");

        // Verify first save
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    CachedActiveTab cachedTab = mActiveTabCache.restoreActiveTab(false, null);
                    assertNotNull("Cached tab should not be null", cachedTab);
                    assertEquals("Tab index should match", 0, cachedTab.tabIndex);
                    assertEquals(
                            "URL should match", "about:blank", cachedTab.tabState.url.getSpec());
                });

        // Navigate to a new URL
        String newUrl = "chrome://version/";
        page = page.loadWebPageProgrammatically(newUrl);
        Tab newTab = page.getTab();

        // Save again with new state and different index
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActiveTabCache.saveActiveTab(newTab, 1, null);
                });

        // Verify second save replaced the first
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    CachedActiveTab cachedTab = mActiveTabCache.restoreActiveTab(false, null);
                    assertNotNull("Cached tab should not be null", cachedTab);
                    assertEquals("Tab index should be updated", 1, cachedTab.tabIndex);
                    assertEquals("URL should be updated", newUrl, cachedTab.tabState.url.getSpec());
                });
    }

    @Test
    @MediumTest
    public void testClearActiveTab() throws ExecutionException {
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        Tab tab = page.getTab();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActiveTabCache.saveActiveTab(tab, 0, null);
                });

        CriteriaHelper.pollInstrumentationThread(
                () -> getActiveTabFile(false).exists(), "Active tab file should exist");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActiveTabCache.clearActiveTab(false);
                    CachedActiveTab cachedTab = mActiveTabCache.restoreActiveTab(false, null);
                    assertNull("Cached tab should be null after clear", cachedTab);
                });
    }
}
