// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeNtpUrl;

import android.content.Context;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.Holder;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;

import java.io.File;

/** Integration tests for {@link ActiveTabCache}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class ActiveTabCacheTest {
    private static final String WINDOW_TAG = "WINDOW";

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private final CipherFactory mCipherFactory = new CipherFactory();
    private ActiveTabCache mActiveTabCache;

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(ActiveTabCache::clearGlobalState);
    }

    @Test
    @MediumTest
    public void testSaveAndRestore() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        Tab tab = page.getTab();
        initActiveTabCache();

        ThreadUtils.runOnUiThreadBlocking(() -> mActiveTabCache.saveActiveTab(tab, null));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabState tabState =
                            mActiveTabCache.restoreActiveTab(
                                    /* isOffTheRecord= */ false, /* cipherFactory= */ null);
                    assertNotNull("Tab state should not be null", tabState);
                    assertEquals(
                            "URL should match", tab.getUrl().getSpec(), tabState.url.getSpec());
                });
    }

    @Test
    @MediumTest
    public void testSaveAndRestoreIncognito() {
        WebPageStation page = mActivityTestRule.startOnIncognitoBlankPage();
        Tab tab = page.getTab();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActiveTabCache =
                            new ActiveTabCache(
                                    WINDOW_TAG,
                                    page.getActivity().getTabModelSelectorSupplier().get(),
                                    mCipherFactory);
                });

        ThreadUtils.runOnUiThreadBlocking(() -> mActiveTabCache.saveActiveTab(tab, mCipherFactory));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabState tabState =
                            mActiveTabCache.restoreActiveTab(
                                    /* isOffTheRecord= */ true, mCipherFactory);
                    assertNotNull("Tab state should not be null", tabState);
                    assertEquals(
                            "URL should match", tab.getUrl().getSpec(), tabState.url.getSpec());
                });
    }

    @Test
    @MediumTest
    public void testReplaceActiveTab() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        Tab tab = page.getTab();
        initActiveTabCache();

        ThreadUtils.runOnUiThreadBlocking(
                () -> mActiveTabCache.saveActiveTab(tab, /* cipherFactory= */ null));

        waitForActiveTabFileCreation(/* incognito= */ false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabState tabState =
                            mActiveTabCache.restoreActiveTab(
                                    /* isOffTheRecord= */ false, /* cipherFactory= */ null);
                    assertNotNull(tabState);
                    assertEquals("URL should match", "about:blank", tabState.url.getSpec());
                });

        String newUrl = "chrome://version/";
        page = page.loadWebPageProgrammatically(newUrl);
        Tab newTab = page.getTab();

        // Save again with new state
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActiveTabCache.saveActiveTab(newTab, /* cipherFactory= */ null));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabState tabState =
                            mActiveTabCache.restoreActiveTab(
                                    /* isOffTheRecord= */ false, /* cipherFactory= */ null);
                    assertNotNull(tabState);
                    assertEquals(newUrl, tabState.url.getSpec());
                });
    }

    @Test
    @MediumTest
    public void testClearActiveTab() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        Tab tab = page.getTab();
        initActiveTabCache();

        ThreadUtils.runOnUiThreadBlocking(() -> mActiveTabCache.saveActiveTab(tab, null));

        waitForActiveTabFileCreation(/* incognito= */ false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActiveTabCache.clearActiveTab(false);
                    TabState tabState =
                            mActiveTabCache.restoreActiveTab(
                                    /* isOffTheRecord= */ false, /* cipherFactory= */ null);
                    assertNull(tabState);
                });
    }

    @Test
    @MediumTest
    public void testSaveIncognito_NullCipherFactory_ThrowsException() {
        WebPageStation page = mActivityTestRule.startOnIncognitoBlankPage();
        Tab tab = page.getTab();
        initActiveTabCache();
        assertTrue(tab.isOffTheRecord());

        Holder<Boolean> threwException = new Holder<>(false);
        try {
            // This will result in a RuntimeException or AssertionError depending on whether
            // ThreadUtils.runOnUiThreadBlocking rethrows as a RuntimeException.
            ThreadUtils.runOnUiThreadBlocking(() -> mActiveTabCache.saveActiveTab(tab, null));
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
        initActiveTabCache();

        ThreadUtils.runOnUiThreadBlocking(() -> mActiveTabCache.saveActiveTab(tab, mCipherFactory));

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
        initActiveTabCache();

        // Pass a cipher factory even though it's regular tab. It will not be used.
        ThreadUtils.runOnUiThreadBlocking(() -> mActiveTabCache.saveActiveTab(tab, mCipherFactory));

        waitForActiveTabFileCreation(/* incognito= */ false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Restoration should work because it is a regular tab.
                    TabState tabState =
                            mActiveTabCache.restoreActiveTab(
                                    /* isOffTheRecord= */ false, /* cipherFactory= */ null);
                    assertNotNull(tabState);
                    assertEquals(tab.getUrl().getSpec(), tabState.url.getSpec());
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Restoration should still work even with a cipher factory.
                    TabState tabState = mActiveTabCache.restoreActiveTab(false, mCipherFactory);
                    assertNotNull(tabState);
                });
    }

    @Test
    @MediumTest
    public void testFlatBufferUsage() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        Tab tab = page.getTab();
        initActiveTabCache();

        ThreadUtils.runOnUiThreadBlocking(
                () -> mActiveTabCache.saveActiveTab(tab, /* cipherFactory= */ null));
        waitForActiveTabFileCreation(/* incognito= */ false);
    }

    @Test
    @MediumTest
    public void testCleanupWindow() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        Tab tab = page.getTab();
        initActiveTabCache();

        ThreadUtils.runOnUiThreadBlocking(
                () -> mActiveTabCache.saveActiveTab(tab, /* cipherFactory= */ null));
        waitForActiveTabFileCreation(/* incognito= */ false);
    }

    @Test
    @MediumTest
    public void testTrackingActiveTab() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        Tab tab1 = page.getTab();
        initActiveTabCache();

        ThreadUtils.runOnUiThreadBlocking(
                () -> mActiveTabCache.startTracking(/* incognito= */ false));
        waitForActiveTabFileCreation(/* incognito= */ false);

        clearActiveTabAndWait(/* incognito= */ false);

        RegularNewTabPageStation ntp = page.openNewTabFast();
        Tab tab2 = ntp.getTab();
        clearActiveTabAndWait(/* incognito= */ false);

        page = ntp.selectTabFast(tab1, WebPageStation::newBuilder);
        waitForActiveTabFileCreation(/* incognito= */ false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabState tabState =
                            mActiveTabCache.restoreActiveTab(
                                    /* isOffTheRecord= */ false, /* cipherFactory= */ null);
                    assertNotNull(tabState);
                    assertEquals("about:blank", tabState.url.getSpec());
                });
        clearActiveTabAndWait(/* incognito= */ false);

        page.selectTabFast(tab2, RegularNewTabPageStation::newBuilder);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabState tabState =
                            mActiveTabCache.restoreActiveTab(
                                    /* isOffTheRecord= */ false, /* cipherFactory= */ null);
                    assertNotNull(tabState);
                    assertEquals(getOriginalNativeNtpUrl(), tabState.url.getSpec());
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> mActiveTabCache.stopTracking(/* incognito= */ false));
    }

    @Test
    @MediumTest
    public void testTrackingActiveTabIncognito() {
        WebPageStation page = mActivityTestRule.startOnIncognitoBlankPage();
        Tab tab1 = page.getTab();
        String aboutBlankUrl = "about:blank";

        WebPageStation finalPage = page;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActiveTabCache =
                            new ActiveTabCache(
                                    WINDOW_TAG,
                                    finalPage.getActivity().getTabModelSelectorSupplier().get(),
                                    mCipherFactory);
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> mActiveTabCache.startTracking(/* incognito= */ true));
        waitForActiveTabFileCreation(/* incognito= */ true);
        clearActiveTabAndWait(/* incognito= */ true);

        IncognitoNewTabPageStation ntp = page.openNewIncognitoTabFast();
        Tab tab2 = ntp.getTab();
        clearActiveTabAndWait(/* incognito= */ true);

        page = ntp.selectTabFast(tab1, WebPageStation::newBuilder);
        waitForActiveTabFileCreation(/* incognito= */ true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabState tabState =
                            mActiveTabCache.restoreActiveTab(
                                    /* isOffTheRecord= */ true, mCipherFactory);
                    assertNotNull(tabState);
                    assertEquals(aboutBlankUrl, tabState.url.getSpec());
                });
        clearActiveTabAndWait(/* incognito= */ true);

        ntp = page.selectTabFast(tab2, IncognitoNewTabPageStation::newBuilder);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabState tabState =
                            mActiveTabCache.restoreActiveTab(
                                    /* isOffTheRecord= */ true, mCipherFactory);
                    assertNotNull(tabState);
                    assertEquals(getOriginalNativeNtpUrl(), tabState.url.getSpec());
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> mActiveTabCache.stopTracking(/* incognito= */ true));

        ntp.openIncognitoTabSwitcher()
                .openAppMenu()
                .clickSelectTabs()
                .addTabToSelection(0, tab1.getId())
                .addTabToSelection(1, tab2.getId())
                .openAppMenuWithEditor()
                .closeTabs();
    }

    private void initActiveTabCache() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActiveTabCache =
                            new ActiveTabCache(
                                    WINDOW_TAG,
                                    mActivityTestRule
                                            .getActivity()
                                            .getTabModelSelectorSupplier()
                                            .get(),
                                    mCipherFactory);
                });
    }

    private File getActiveTabFile(boolean incognito) {
        String fileName = "flatbufferv1_" + WINDOW_TAG + (incognito ? "_incognito" : "_regular");
        return new File(
                ContextUtils.getApplicationContext().getDir("active_tabs", Context.MODE_PRIVATE),
                fileName);
    }

    private void waitForActiveTabFileCreation(boolean incognito) {
        CriteriaHelper.pollInstrumentationThread(
                () -> getActiveTabFile(incognito).exists(), "Active tab file should exist");
    }

    private void waitForActiveTabFileDeletion(boolean incognito) {
        CriteriaHelper.pollInstrumentationThread(
                () -> !getActiveTabFile(incognito).exists(), "Active tab file should not exist");
    }

    private void clearActiveTabAndWait(boolean incognito) {
        ThreadUtils.runOnUiThreadBlocking(() -> mActiveTabCache.clearActiveTab(incognito));
        waitForActiveTabFileDeletion(incognito);
    }
}
