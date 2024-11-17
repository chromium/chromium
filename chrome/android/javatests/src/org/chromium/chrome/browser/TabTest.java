// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import androidx.annotation.Nullable;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabLoadIfNeededCaller;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateExtractor;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.RecentTabsPageTestUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

/** Tests for Tab class. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class TabTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCtaTabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Tab mTab;
    private int mRootIdForReset;
    private CallbackHelper mOnTitleUpdatedHelper;

    private final TabObserver mTabObserver =
            new EmptyTabObserver() {
                @Override
                public void onTitleUpdated(Tab tab) {
                    mOnTitleUpdatedHelper.notifyCalled();
                }
            };

    private boolean isShowingSadTab() throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(() -> SadTab.isShowing(mTab));
    }

    @Before
    public void setUp() throws Exception {
        mTab = sActivityTestRule.getActivity().getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(() -> mTab.addObserver(mTabObserver));
        mOnTitleUpdatedHelper = new CallbackHelper();
        mRootIdForReset = mTab.getRootId();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Reset Root Id to what it was at the start, as it can be modified in the
                    // tests.
                    mTab.setRootId(mRootIdForReset);
                    mTab.removeObserver(mTabObserver);
                });
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    public void testTabContext() {
        assertFalse(
                "The tab context cannot be an activity",
                mTab.getContentView().getContext() instanceof Activity);
        assertNotSame(
                "The tab context's theme should have been updated",
                mTab.getContentView().getContext().getTheme(),
                sActivityTestRule.getActivity().getApplication().getTheme());
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    public void testTitleDelayUpdate() throws Throwable {
        final String oldTitle = "oldTitle";
        final String newTitle = "newTitle";

        sActivityTestRule.loadUrl(
                "data:text/html;charset=utf-8,<html><head><title>"
                        + oldTitle
                        + "</title></head><body/></html>");
        assertEquals(
                "title does not match initial title",
                oldTitle,
                ChromeTabUtils.getTitleOnUiThread(mTab));
        int currentCallCount = mOnTitleUpdatedHelper.getCallCount();
        sActivityTestRule.runJavaScriptCodeInCurrentTab("document.title='" + newTitle + "';");
        mOnTitleUpdatedHelper.waitForCallback(currentCallCount);
        assertEquals("title does not update", newTitle, ChromeTabUtils.getTitleOnUiThread(mTab));
    }

    /**
     * Verifies a Tab's contents is restored when the Tab is foregrounded after its contents have
     * been destroyed while backgrounded. Note that document mode is explicitly disabled, as the
     * document activity may be fully recreated if its contents is killed while in the background.
     */
    @Test
    @SmallTest
    @Feature({"Tab"})
    public void testTabRestoredIfKilledWhileActivityStopped() throws Exception {
        // Ensure the tab is showing before stopping the activity.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mTab.show(TabSelectionType.FROM_NEW, TabLoadIfNeededCaller.OTHER));

        assertFalse(mTab.needsReload());
        assertFalse(mTab.isHidden());
        assertFalse(isShowingSadTab());

        // Stop the activity and simulate a killed renderer.
        ChromeApplicationTestUtils.fireHomeScreenIntent(
                ApplicationProvider.getApplicationContext());
        ThreadUtils.runOnUiThreadBlocking(
                () -> ChromeTabUtils.simulateRendererKilledForTesting(mTab));

        CriteriaHelper.pollUiThread(mTab::isHidden);
        assertTrue(mTab.needsReload());
        assertFalse(isShowingSadTab());

        ChromeApplicationTestUtils.launchChrome(ApplicationProvider.getApplicationContext());

        // The tab should be restored and visible.
        CriteriaHelper.pollUiThread(() -> !mTab.isHidden());
        assertFalse(mTab.needsReload());
        assertFalse(isShowingSadTab());
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    public void testTabAttachment() {
        assertNotNull(mTab.getWebContents());
        assertFalse(mTab.isDetached());

        detachOnUiThread(mTab);
        assertNotNull(mTab.getWebContents());
        assertTrue(mTab.isDetached());

        attachOnUiThread(mTab);
        assertNotNull(mTab.getWebContents());
        assertFalse(mTab.isDetached());
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    @RequiresRestart(
            "crbug.com/358190587, causes BlankCTATabInitialStateRule state reset to fail flakily.")
    public void testNativePageTabAttachment() {
        sActivityTestRule.loadUrl(UrlConstants.RECENT_TABS_URL);
        RecentTabsPageTestUtils.waitForRecentTabsPageLoaded(mTab);
        assertNotNull(mTab.getWebContents());
        assertFalse(mTab.isDetached());

        detachOnUiThread(mTab);
        assertNotNull(mTab.getWebContents());
        assertTrue(mTab.isDetached());

        attachOnUiThread(mTab);
        assertNotNull(mTab.getWebContents());
        assertFalse(mTab.isDetached());
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    public void testFrozenTabAttachment() {
        String url =
                sActivityTestRule.getTestServer().getURL("/chrome/test/data/android/about.html");
        Tab tab = createSecondFrozenTab(url);
        assertNull(tab.getWebContents());
        assertFalse(tab.isDetached());

        detachOnUiThread(tab);
        assertNull(tab.getWebContents());
        assertTrue(tab.isDetached());

        attachOnUiThread(tab);
        assertNull(tab.getWebContents());
        assertFalse(tab.isDetached());
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    public void testRestoreTabState() {
        TabState tabState =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return TabStateExtractor.from(mTab);
                        });
        tabState.timestampMillis = 437289L;
        tabState.lastNavigationCommittedTimestampMillis = 748932L;
        tabState.rootId = 5;
        tabState.tabGroupId = new Token(1L, 2L);
        tabState.tabHasSensitiveContent = true;

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabTestUtils.restoreFieldsFromState(mTab, tabState);
                });
        assertEquals(tabState.timestampMillis, mTab.getTimestampMillis());
        assertEquals(
                tabState.lastNavigationCommittedTimestampMillis,
                mTab.getLastNavigationCommittedTimestampMillis());
        assertEquals(tabState.rootId, mTab.getRootId());
        assertEquals(tabState.tabGroupId, mTab.getTabGroupId());
        assertEquals(tabState.tabHasSensitiveContent, mTab.getTabHasSensitiveContent());
    }

    @FunctionalInterface
    private interface TestTabCreator {
        /** Create a new tab with the provided URL. */
        Tab createTab(String url);
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    public void testFreezeAndAppendPendingNavigation_AlreadyFrozen() {
        String firstUrl =
                sActivityTestRule.getTestServer().getURL("/chrome/test/data/android/about.html");
        String secondUrl =
                sActivityTestRule.getTestServer().getURL("/chrome/test/data/android/test.html");
        checkFreezingAndAppendingPendingNavigation(
                this::createSecondFrozenTab, firstUrl, secondUrl, "MyFrozenTitle");
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    public void testFreezeAndAppendPendingNavigation_LazyBackground() {
        String firstUrl =
                sActivityTestRule.getTestServer().getURL("/chrome/test/data/android/about.html");
        String secondUrl =
                sActivityTestRule.getTestServer().getURL("/chrome/test/data/android/test.html");
        checkFreezingAndAppendingPendingNavigation(
                this::createLazyTab, firstUrl, secondUrl, "MyLazyTitle");
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    public void testFreezeAndAppendPendingNavigation_LiveBackground() {
        String firstUrl =
                sActivityTestRule.getTestServer().getURL("/chrome/test/data/android/about.html");
        String secondUrl =
                sActivityTestRule.getTestServer().getURL("/chrome/test/data/android/test.html");
        checkFreezingAndAppendingPendingNavigation(
                url -> {
                    Tab tab = sActivityTestRule.loadUrlInNewTab(url, /* incognito= */ false);
                    ThreadUtils.runOnUiThreadBlocking(
                            () -> {
                                TabModel model =
                                        sActivityTestRule.getActivity().getCurrentTabModel();
                                TabModelUtils.setIndex(model, /* index= */ 0);
                            });
                    return tab;
                },
                firstUrl,
                secondUrl,
                "MyTitle");
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    public void testFreezeAndAppendPendingNavigation_LiveBackground_NativePage() {
        String firstUrl = UrlConstants.NTP_URL;
        String secondUrl =
                sActivityTestRule.getTestServer().getURL("/chrome/test/data/android/test.html");
        checkFreezingAndAppendingPendingNavigation(
                url -> {
                    Tab tab = sActivityTestRule.loadUrlInNewTab(url, /* incognito= */ false);
                    ThreadUtils.runOnUiThreadBlocking(
                            () -> {
                                TabModel model =
                                        sActivityTestRule.getActivity().getCurrentTabModel();
                                TabModelUtils.setIndex(model, /* index= */ 0);
                            });
                    assertTrue(tab.isNativePage());
                    return tab;
                },
                firstUrl,
                secondUrl,
                "Not NTP");
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    public void testFreezeAndAppendPendingNavigation_NullTitle() {
        String firstUrl =
                sActivityTestRule.getTestServer().getURL("/chrome/test/data/android/about.html");
        String secondUrl =
                sActivityTestRule.getTestServer().getURL("/chrome/test/data/android/test.html");
        checkFreezingAndAppendingPendingNavigation(
                this::createSecondFrozenTab, firstUrl, secondUrl, null);
    }

    private void checkFreezingAndAppendingPendingNavigation(
            TestTabCreator tabCreator,
            String firstUrl,
            String secondUrl,
            @Nullable String secondTitle) {
        TabObserver observer = Mockito.mock(TabObserver.class);
        Tab bgTab = tabCreator.createTab(firstUrl);
        boolean wasFrozen = bgTab.isFrozen();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    bgTab.addObserver(observer);
                    bgTab.freezeAndAppendPendingNavigation(
                            new LoadUrlParams(secondUrl), secondTitle);
                    assertTrue(bgTab.isFrozen());
                    assertFalse(bgTab.isNativePage());
                });
        verify(observer).onUrlUpdated(eq(bgTab));
        if (wasFrozen) {
            verify(observer, never()).onContentChanged(bgTab);
        } else {
            verify(observer).onContentChanged(bgTab);
        }
        verify(observer).onFaviconUpdated(bgTab, null, null);
        verify(observer).onTitleUpdated(bgTab);
        verify(observer).onNavigationEntriesAppended(bgTab);
        assertEquals(secondTitle, ChromeTabUtils.getTitleOnUiThread(bgTab));
        assertEquals(secondUrl, ChromeTabUtils.getUrlStringOnUiThread(bgTab));

        assertFalse(bgTab.isLoading());
        assertNull(bgTab.getWebContents());
        assertNull(bgTab.getPendingLoadParams());

        Runnable loadPage =
                () -> {
                    ThreadUtils.runOnUiThreadBlocking(
                            () -> {
                                TabModel model =
                                        sActivityTestRule.getActivity().getCurrentTabModel();
                                TabModelUtils.setIndex(model, model.indexOf(bgTab));
                            });
                };
        ChromeTabUtils.waitForTabPageLoaded(bgTab, secondUrl, loadPage);
        assertNotNull(bgTab.getView());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(bgTab.canGoForward());
                    assertTrue(bgTab.canGoBack());
                    bgTab.goBack();
                });
        ChromeTabUtils.waitForTabPageLoaded(bgTab, firstUrl);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(bgTab.canGoBack());
                    assertTrue(bgTab.canGoForward());
                });
    }

    private void detachOnUiThread(Tab tab) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebContents webContents = tab.getWebContents();
                    if (webContents != null) webContents.setTopLevelNativeWindow(null);
                    tab.updateAttachment(/* window= */ null, /* tabDelegateFactory= */ null);
                });
    }

    private void attachOnUiThread(Tab tab) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WindowAndroid window = sActivityTestRule.getActivity().getWindowAndroid();
                    WebContents webContents = tab.getWebContents();
                    if (webContents != null) webContents.setTopLevelNativeWindow(window);
                    tab.updateAttachment(window, /* tabDelegateFactory= */ null);
                });
    }

    private Tab createSecondFrozenTab(String url) {
        Tab tab = sActivityTestRule.loadUrlInNewTab(url, /* incognito= */ false);
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabState state = TabStateExtractor.from(tab);
                    sActivityTestRule
                            .getActivity()
                            .getCurrentTabModel()
                            .closeTabs(TabClosureParams.closeTab(tab).allowUndo(false).build());
                    return sActivityTestRule
                            .getActivity()
                            .getCurrentTabCreator()
                            .createFrozenTab(state, tab.getId(), /* index= */ 1);
                });
    }

    private Tab createLazyTab(String url) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabCreator tabCreator =
                            sActivityTestRule
                                    .getActivity()
                                    .getTabCreatorManagerSupplier()
                                    .get()
                                    .getTabCreator(/* incognito= */ false);
                    LoadUrlParams params = new LoadUrlParams(new GURL(url));
                    return tabCreator.createNewTab(
                            params,
                            "Lazy Title",
                            TabLaunchType.FROM_SYNC_BACKGROUND,
                            /* parent= */ null,
                            /* position= */ TabList.INVALID_TAB_INDEX);
                });
    }
}
