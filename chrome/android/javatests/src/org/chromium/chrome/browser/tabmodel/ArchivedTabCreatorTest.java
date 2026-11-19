// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabRestoreEntryId;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tab.state.ArchivePersistedTabData;
import org.chromium.chrome.browser.tab.tab_restore.HistoricalTabSaverImpl;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot;
import org.chromium.chrome.browser.tabmodel.undo.UndoClosureSnapshot.ClosedTab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.url.GURL;

import java.util.Collections;
import java.util.List;
import java.util.concurrent.atomic.AtomicReference;

/** Tests for ChromeTabCreator. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class ArchivedTabCreatorTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private static final String TEST_PATH = "/chrome/test/data/android/about.html";

    private EmbeddedTestServer mTestServer;
    private Profile mProfile;
    private ArchivedTabModelOrchestrator mOrchestrator;
    private TabCreator mTabCreator;
    private WebPageStation mInitialPage;

    @Before
    public void setUp() throws Exception {
        mTestServer = mActivityTestRule.getTestServer();
        mInitialPage = mActivityTestRule.startOnBlankPage();
        runOnUiThreadBlocking(
                () -> {
                    mProfile =
                            mInitialPage
                                    .getActivity()
                                    .getProfileProviderSupplier()
                                    .get()
                                    .getOriginalProfile();
                    mOrchestrator = ArchivedTabModelOrchestrator.getForProfile(mProfile);
                    mTabCreator = mOrchestrator.getArchivedTabCreatorForTesting();
                });
        // Wait for the native tab state to be initialized so that we are sure that native is ready.
        CriteriaHelper.pollUiThread(
                () -> mOrchestrator.getTabModelSelector().isTabStateInitialized());
    }

    @After
    public void tearDown() {
        if (mOrchestrator == null) return;
        runOnUiThreadBlocking(
                () -> {
                    TabModel archivedModel = mOrchestrator.getTabModel();
                    if (archivedModel != null) {
                        archivedModel
                                .getTabRemover()
                                .closeTabs(
                                        TabClosureParams.closeAllTabs()
                                                .allowUndo(false)
                                                .saveToTabRestoreService(false)
                                                .build(),
                                        /* allowDialog= */ false);
                    }
                });
    }

    @Test
    @MediumTest
    public void testCreateFrozenTab() throws Exception {
        WebPageStation testPage = mInitialPage.openFakeLinkToWebPage(mTestServer.getURL(TEST_PATH));
        Tab tab = testPage.loadedTabElement.value();
        Tab frozenTab =
                runOnUiThreadBlocking(
                        () ->
                                mTabCreator.createFrozenTab(
                                        extractStateAndCloseTab(tab),
                                        tab.getId(),
                                        /* index= */ 0,
                                        TabLaunchType.FROM_RESTORE));
        assertNotNull(frozenTab);
        assertNull(frozenTab.getWebContents());
    }

    @Test
    @MediumTest
    public void testCreateFrozenTab_forUndoClosure() throws Exception {
        WebPageStation testPage = mInitialPage.openFakeLinkToWebPage(mTestServer.getURL(TEST_PATH));
        Tab tab = testPage.loadedTabElement.value();
        Tab frozenTab =
                runOnUiThreadBlocking(
                        () ->
                                mTabCreator.createFrozenTab(
                                        extractStateAndCloseTab(tab),
                                        Tab.INVALID_TAB_ID,
                                        /* index= */ 0,
                                        TabLaunchType.FROM_UNDO_CLOSURE));
        assertNotNull(frozenTab);
        assertNull(frozenTab.getWebContents());
        assertEquals(TabLaunchType.FROM_UNDO_CLOSURE, frozenTab.getLaunchType());

        ArchivePersistedTabData data = getArchivePersistedTabData(frozenTab);
        assertNotNull(data);
        assertNotEquals(Tab.INVALID_TIMESTAMP, data.getArchivedTimeMs());
        assertTrue(data.getArchivedTimeMs() > 0);
    }

    @Test
    @MediumTest
    public void testRestoreClosedTabsForUndo_archivedModel() throws Exception {
        WebPageStation testPage = mInitialPage.openFakeLinkToWebPage(mTestServer.getURL(TEST_PATH));
        Tab tab = testPage.loadedTabElement.value();
        TabModel archivedModel = runOnUiThreadBlocking(() -> mOrchestrator.getTabModel());
        Tab archivedTab =
                runOnUiThreadBlocking(
                        () ->
                                mTabCreator.createFrozenTab(
                                        extractStateAndCloseTab(tab),
                                        Tab.INVALID_TAB_ID,
                                        /* index= */ 0,
                                        TabLaunchType.FROM_RESTORE));
        assertNotNull(archivedTab);

        int oldTabId = archivedTab.getId();
        GURL expectedUrl = runOnUiThreadBlocking(archivedTab::getUrl);
        String expectedTitle = runOnUiThreadBlocking(archivedTab::getTitle);
        int modelIndex = runOnUiThreadBlocking(() -> archivedModel.indexOf(archivedTab));

        @TabRestoreEntryId
        int entryId =
                runOnUiThreadBlocking(
                        () ->
                                new HistoricalTabSaverImpl(archivedModel)
                                        .createHistoricalTab(archivedTab));
        assertNotEquals(TabRestoreEntryId.INVALID_TAB_RESTORE_ENTRY_ID, entryId);

        runOnUiThreadBlocking(
                () ->
                        archivedModel
                                .getTabRemover()
                                .closeTabs(
                                        TabClosureParams.closeTab(archivedTab)
                                                .allowUndo(false)
                                                .build(),
                                        /* allowDialog= */ false));
        assertNull(runOnUiThreadBlocking(() -> archivedModel.getTabById(oldTabId)));

        UndoClosureSnapshot snapshot =
                UndoClosureSnapshot.forArchived(
                        List.of(
                                new ClosedTab(
                                        oldTabId,
                                        expectedTitle,
                                        /* groupId= */ null,
                                        /* indexInGroup= */ TabList.INVALID_TAB_INDEX,
                                        modelIndex,
                                        /* pinned= */ false,
                                        /* wasSelected= */ false)),
                        Collections.emptyList(),
                        /* savedGroupTitle= */ null,
                        /* savedGroupTabCount= */ 0,
                        entryId);
        int[] restoredIds =
                runOnUiThreadBlocking(
                        () ->
                                archivedModel.restoreClosedTabsForUndo(
                                        snapshot,
                                        /* reselect= */ false,
                                        /* tabGroupUiActionHandler= */ null));

        assertEquals(1, restoredIds.length);
        assertNotEquals(Tab.INVALID_TAB_ID, restoredIds[0]);
        Tab restoredTab = runOnUiThreadBlocking(() -> archivedModel.getTabById(restoredIds[0]));
        assertNotNull(restoredTab);
        assertNull(restoredTab.getWebContents());
        assertEquals(TabLaunchType.FROM_UNDO_CLOSURE, restoredTab.getLaunchType());
        assertEquals(expectedUrl, runOnUiThreadBlocking(restoredTab::getUrl));

        ArchivePersistedTabData data = getArchivePersistedTabData(restoredTab);
        assertNotNull(data);
        assertNotEquals(Tab.INVALID_TIMESTAMP, data.getArchivedTimeMs());
        assertTrue(data.getArchivedTimeMs() > 0);
    }

    @Test
    @MediumTest
    public void testRestoreFallback() {
        runOnUiThreadBlocking(
                () -> {
                    int count = mOrchestrator.getTabModel().getCount();
                    assertNotNull(
                            mTabCreator.createNewTab(
                                    new LoadUrlParams(mTestServer.getURL(TEST_PATH)),
                                    TabLaunchType.FROM_RESTORE,
                                    null));
                    assertEquals(count + 1, mOrchestrator.getTabModel().getCount());
                });
    }

    @Test
    @MediumTest
    public void testRestoreFallback_AssertionErrorWhenTabLaunchTypeIncorrect() throws Exception {
        // Test is a no-op when asserts are disabled.
        if (!BuildConfig.ENABLE_ASSERTS) return;

        runOnUiThreadBlocking(
                () -> {
                    try {
                        mTabCreator.createNewTab(
                                new LoadUrlParams(mTestServer.getURL(TEST_PATH)),
                                TabLaunchType.FROM_CHROME_UI,
                                null);
                    } catch (AssertionError e) {
                        return;
                    }
                    fail(
                            "Creating a non-frozen tab should fail with an assert when the"
                                    + " launch type is incorrect.");
                });
    }

    private TabState extractStateAndCloseTab(Tab tab) {
        TabState state = TabStateExtractor.from(tab);
        mActivityTestRule
                .getActivity()
                .getCurrentTabModel()
                .getTabRemover()
                .closeTabs(
                        TabClosureParams.closeTab(tab).allowUndo(false).build(),
                        /* allowDialog= */ false);
        return state;
    }

    private ArchivePersistedTabData getArchivePersistedTabData(Tab tab) throws Exception {
        CallbackHelper callbackHelper = new CallbackHelper();
        AtomicReference<ArchivePersistedTabData> dataRef = new AtomicReference<>();
        runOnUiThreadBlocking(
                () ->
                        ArchivePersistedTabData.from(
                                tab,
                                data -> {
                                    dataRef.set(data);
                                    callbackHelper.notifyCalled();
                                }));
        callbackHelper.waitForOnly();
        return dataRef.get();
    }
}
