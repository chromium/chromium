// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.Holder;
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

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(ActiveTabCache::clearGlobalState);
    }

    @Test
    @MediumTest
    public void testSaveAndRestore() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        Tab tab = page.getTab();

        ThreadUtils.runOnUiThreadBlocking(() -> mActiveTabCache.saveActiveTab(tab, 0, null));

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
    public void testSaveAndRestoreIncognito() {
        WebPageStation page = mActivityTestRule.startOnIncognitoBlankPage();
        Tab tab = page.getTab();

        ThreadUtils.runOnUiThreadBlocking(
                () -> mActiveTabCache.saveActiveTab(tab, 0, mCipherFactory));

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
    public void testReplaceActiveTab() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        Tab tab = page.getTab();

        ThreadUtils.runOnUiThreadBlocking(() -> mActiveTabCache.saveActiveTab(tab, 0, null));

        waitForActiveTabFileCreation(/* incognito= */ false);

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
        ThreadUtils.runOnUiThreadBlocking(() -> mActiveTabCache.saveActiveTab(newTab, 1, null));

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
    public void testClearActiveTab() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        Tab tab = page.getTab();

        ThreadUtils.runOnUiThreadBlocking(() -> mActiveTabCache.saveActiveTab(tab, 0, null));

        waitForActiveTabFileCreation(/* incognito= */ false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActiveTabCache.clearActiveTab(false);
                    CachedActiveTab cachedTab = mActiveTabCache.restoreActiveTab(false, null);
                    assertNull(cachedTab);
                });
    }

    @Test
    @MediumTest
    public void testSaveIncognito_NullCipherFactory_ThrowsException() {
        WebPageStation page = mActivityTestRule.startOnIncognitoBlankPage();
        Tab tab = page.getTab();
        assertTrue(tab.isOffTheRecord());

        Holder<Boolean> threwException = new Holder<>(false);
        try {
            // This will result in a RuntimeException or AssertionError depending on whether
            // ThreadUtils.runOnUiThreadBlocking rethrows as a RuntimeException.
            ThreadUtils.runOnUiThreadBlocking(() -> mActiveTabCache.saveActiveTab(tab, 0, null));
        } catch (RuntimeException | AssertionError e) {
            threwException.onResult(true);
        }
        assertTrue("Expected a RuntimeException or AssertionError", threwException.get());
    }

    @Test
    @MediumTest
    public void testRestoreIncognito_NullCipherFactory_ThrowsException() {
        WebPageStation page = mActivityTestRule.startOnIncognitoBlankPage();
        Tab tab = page.getTab();

        ThreadUtils.runOnUiThreadBlocking(
                () -> mActiveTabCache.saveActiveTab(tab, 0, mCipherFactory));

        waitForActiveTabFileCreation(/* incognito= */ true);

        Holder<Boolean> threwException = new Holder<>(false);
        try {
            // This will result in a RuntimeException or AssertionError depending on whether
            // ThreadUtils.runOnUiThreadBlocking rethrows as a RuntimeException.
            ThreadUtils.runOnUiThreadBlocking(() -> mActiveTabCache.restoreActiveTab(true, null));
        } catch (RuntimeException | AssertionError e) {
            threwException.onResult(true);
        }
        assertTrue("Expected a RuntimeException or AssertionError", threwException.get());
    }

    @Test
    @MediumTest
    public void testSaveAndRestoreRegular_WithCipherFactory() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        Tab tab = page.getTab();

        // Pass a cipher factory even though it's regular tab. It will not be used.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActiveTabCache.saveActiveTab(tab, 0, mCipherFactory));

        waitForActiveTabFileCreation(/* incognito= */ false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Restoration should work because it is a regular tab.
                    CachedActiveTab cachedTab = mActiveTabCache.restoreActiveTab(false, null);
                    assertNotNull(cachedTab);
                    assertEquals(tab.getUrl().getSpec(), cachedTab.tabState.url.getSpec());
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Restoration should still work even with a cipher factory.
                    CachedActiveTab cachedTab =
                            mActiveTabCache.restoreActiveTab(false, mCipherFactory);
                    assertNotNull(cachedTab);
                });
    }

    @Test
    @MediumTest
    public void testFlatBufferUsage() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        Tab tab = page.getTab();

        ThreadUtils.runOnUiThreadBlocking(() -> mActiveTabCache.saveActiveTab(tab, 0, null));
        waitForActiveTabFileCreation(/* incognito= */ false);
    }

    @Test
    @MediumTest
    public void testCleanupWindow() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        Tab tab = page.getTab();

        ThreadUtils.runOnUiThreadBlocking(() -> mActiveTabCache.saveActiveTab(tab, 0, null));
        waitForActiveTabFileCreation(/* incognito= */ false);
    }

    private File getActiveTabFile(boolean incognito) {
        String fileName = incognito ? "flatbufferv1_0_incognito" : "flatbufferv1_0_regular";
        return new File(
                ContextUtils.getApplicationContext().getDir("active_tabs", Context.MODE_PRIVATE),
                fileName);
    }

    private void waitForActiveTabFileCreation(boolean incognito) {
        CriteriaHelper.pollInstrumentationThread(
                () -> getActiveTabFile(incognito).exists(), "Active tab file should exist");
    }
}
