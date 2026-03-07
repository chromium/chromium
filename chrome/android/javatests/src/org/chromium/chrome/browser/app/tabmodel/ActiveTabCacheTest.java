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
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.tab.StorageLoadedData.LoadedTabState;
import org.chromium.chrome.browser.tab.Tab;
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
        initActiveTabCache(/* hasCipherFactory= */ true);

        ThreadUtils.runOnUiThreadBlocking(() -> mActiveTabCache.saveActiveTab(tab));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LoadedTabState tabState =
                            mActiveTabCache.restoreActiveTab(/* isOffTheRecord= */ false);
                    assertNotNull("Tab state should not be null", tabState);
                    assertEquals("Tab ID should match", tab.getId(), tabState.tabId);
                    assertEquals(
                            "URL should match",
                            tab.getUrl().getSpec(),
                            tabState.tabState.url.getSpec());
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

        ThreadUtils.runOnUiThreadBlocking(() -> mActiveTabCache.saveActiveTab(tab));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LoadedTabState tabState =
                            mActiveTabCache.restoreActiveTab(/* isOffTheRecord= */ true);
                    assertNotNull("Tab state should not be null", tabState);
                    assertEquals("Tab ID should match", tab.getId(), tabState.tabId);
                    assertEquals(
                            "URL should match",
                            tab.getUrl().getSpec(),
                            tabState.tabState.url.getSpec());
                });
    }

    @Test
    @MediumTest
    public void testReplaceActiveTab() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        Tab tab = page.getTab();
        initActiveTabCache(/* hasCipherFactory= */ true);

        ThreadUtils.runOnUiThreadBlocking(() -> mActiveTabCache.saveActiveTab(tab));

        waitForActiveTabFileCreation(/* incognito= */ false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LoadedTabState tabState =
                            mActiveTabCache.restoreActiveTab(/* isOffTheRecord= */ false);
                    assertNotNull(tabState);
                    assertEquals(tab.getId(), tabState.tabId);
                    assertEquals(
                            "URL should match", "about:blank", tabState.tabState.url.getSpec());
                });

        String newUrl = "chrome://version/";
        page = page.loadWebPageProgrammatically(newUrl);
        Tab newTab = page.getTab();

        // Save again with new state
        ThreadUtils.runOnUiThreadBlocking(() -> mActiveTabCache.saveActiveTab(newTab));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LoadedTabState tabState =
                            mActiveTabCache.restoreActiveTab(/* isOffTheRecord= */ false);
                    assertNotNull(tabState);
                    assertEquals(newTab.getId(), tabState.tabId);
                    assertEquals(newUrl, tabState.tabState.url.getSpec());
                });
    }

    @Test
    @MediumTest
    public void testClearActiveTab() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        Tab tab = page.getTab();
        initActiveTabCache(/* hasCipherFactory= */ true);

        ThreadUtils.runOnUiThreadBlocking(() -> mActiveTabCache.saveActiveTab(tab));

        waitForActiveTabFileCreation(/* incognito= */ false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActiveTabCache.clearActiveTab(false);
                    LoadedTabState tabState =
                            mActiveTabCache.restoreActiveTab(/* isOffTheRecord= */ false);
                    assertNull(tabState);
                });
    }

    @Test
    @MediumTest
    @DisabledTest(
            message =
                    "Test consistently fails on CI; to be fixed as part of"
                            + " http://crbug.com/485907357")
    public void testSaveIncognito_NullCipherFactory_ThrowsException() {
        WebPageStation page = mActivityTestRule.startOnIncognitoBlankPage();
        Tab tab = page.getTab();
        initActiveTabCache(/* hasCipherFactory= */ false);
        assertTrue(tab.isOffTheRecord());

        Holder<Boolean> threwException = new Holder<>(false);
        try {
            // This will result in a RuntimeException or AssertionError depending on whether
            // ThreadUtils.runOnUiThreadBlocking rethrows as a RuntimeException.
            ThreadUtils.runOnUiThreadBlocking(() -> mActiveTabCache.saveActiveTab(tab));
        } catch (RuntimeException | AssertionError e) {
            threwException.onResult(true);
        }
        assertTrue("Expected a RuntimeException or AssertionError", threwException.get());
    }

    @Test
    @MediumTest
    @DisabledTest(
            message =
                    "Test consistently fails on CI; to be fixed as part of"
                            + " http://crbug.com/485907357")
    public void testRestoreIncognito_NullCipherFactory_ThrowsException() {
        WebPageStation page = mActivityTestRule.startOnIncognitoBlankPage();
        Tab tab = page.getTab();
        initActiveTabCache(/* hasCipherFactory= */ true);

        ThreadUtils.runOnUiThreadBlocking(() -> mActiveTabCache.saveActiveTab(tab));

        waitForActiveTabFileCreation(/* incognito= */ true);

        initActiveTabCache(/* hasCipherFactory= */ false);

        Holder<Boolean> threwException = new Holder<>(false);
        try {
            // This will result in a RuntimeException or AssertionError depending on whether
            // ThreadUtils.runOnUiThreadBlocking rethrows as a RuntimeException.
            ThreadUtils.runOnUiThreadBlocking(() -> mActiveTabCache.restoreActiveTab(true));
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
        initActiveTabCache(/* hasCipherFactory= */ true);

        // Pass a cipher factory even though it's regular tab. It will not be used.
        ThreadUtils.runOnUiThreadBlocking(() -> mActiveTabCache.saveActiveTab(tab));

        waitForActiveTabFileCreation(/* incognito= */ false);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Restoration should work because it is a regular tab.
                    LoadedTabState tabState =
                            mActiveTabCache.restoreActiveTab(/* isOffTheRecord= */ false);
                    assertNotNull(tabState);
                    assertEquals(tab.getId(), tabState.tabId);
                    assertEquals(tab.getUrl().getSpec(), tabState.tabState.url.getSpec());
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Restoration should still work even with a cipher factory.
                    LoadedTabState tabState = mActiveTabCache.restoreActiveTab(false);
                    assertNotNull(tabState);
                    assertEquals(tab.getId(), tabState.tabId);
                });
    }

    @Test
    @MediumTest
    public void testFlatBufferUsage() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        Tab tab = page.getTab();
        initActiveTabCache(/* hasCipherFactory= */ true);

        ThreadUtils.runOnUiThreadBlocking(() -> mActiveTabCache.saveActiveTab(tab));
        waitForActiveTabFileCreation(/* incognito= */ false);
    }

    @Test
    @MediumTest
    public void testCleanupWindow() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        Tab tab = page.getTab();
        initActiveTabCache(/* hasCipherFactory= */ true);

        ThreadUtils.runOnUiThreadBlocking(() -> mActiveTabCache.saveActiveTab(tab));
        waitForActiveTabFileCreation(/* incognito= */ false);
    }

    @Test
    @MediumTest
    public void testTrackingActiveTab() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        Tab tab1 = page.getTab();
        initActiveTabCache(/* hasCipherFactory= */ true);

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
                    LoadedTabState tabState =
                            mActiveTabCache.restoreActiveTab(/* isOffTheRecord= */ false);
                    assertNotNull(tabState);
                    assertEquals(tab1.getId(), tabState.tabId);
                    assertEquals("about:blank", tabState.tabState.url.getSpec());
                });
        clearActiveTabAndWait(/* incognito= */ false);

        page.selectTabFast(tab2, RegularNewTabPageStation::newBuilder);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LoadedTabState tabState =
                            mActiveTabCache.restoreActiveTab(/* isOffTheRecord= */ false);
                    assertNotNull(tabState);
                    assertEquals(tab2.getId(), tabState.tabId);
                    assertEquals(getOriginalNativeNtpUrl(), tabState.tabState.url.getSpec());
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
                    LoadedTabState tabState =
                            mActiveTabCache.restoreActiveTab(/* isOffTheRecord= */ true);
                    assertNotNull(tabState);
                    assertEquals(tab1.getId(), tabState.tabId);
                    assertEquals(aboutBlankUrl, tabState.tabState.url.getSpec());
                });
        clearActiveTabAndWait(/* incognito= */ true);

        ntp = page.selectTabFast(tab2, IncognitoNewTabPageStation::newBuilder);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LoadedTabState tabState =
                            mActiveTabCache.restoreActiveTab(/* isOffTheRecord= */ true);
                    assertNotNull(tabState);
                    assertEquals(tab2.getId(), tabState.tabId);
                    assertEquals(getOriginalNativeNtpUrl(), tabState.tabState.url.getSpec());
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

    private void initActiveTabCache(boolean hasCipherFactory) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActiveTabCache =
                            new ActiveTabCache(
                                    WINDOW_TAG,
                                    mActivityTestRule
                                            .getActivity()
                                            .getTabModelSelectorSupplier()
                                            .get(),
                                    hasCipherFactory ? mCipherFactory : null);
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
