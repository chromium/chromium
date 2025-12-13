// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.autofill.TestViewStructure;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.ContentFeatures;

/** Tests for the {@link TabImpl} class. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_STARTUP_PROMOS
})
@Batch(Batch.PER_CLASS)
public class TabImplTest {
    private static final String TEST_PATH = "/chrome/test/data/android/about.html";
    private static final long DEFAULT_MAX_TIME_TO_WAIT_IN_MS = 3000;

    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private WebPageStation mInitialPage;

    @Before
    public void setUp() {
        mInitialPage = mActivityTestRule.startOnBlankPage();
    }

    private TabImpl createFrozenTab() {
        String url = mActivityTestRule.getTestServer().getURL(TEST_PATH);
        WebPageStation testPage = mInitialPage.openFakeLinkToWebPage(url);
        Tab tab = testPage.loadedTabElement.value();

        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabState state = TabStateExtractor.from(tab);
                    mActivityTestRule
                            .getActivity()
                            .getCurrentTabModel()
                            .getTabRemover()
                            .closeTabs(
                                    TabClosureParams.closeTab(tab).allowUndo(false).build(),
                                    /* allowDialog= */ false);
                    return (TabImpl)
                            mActivityTestRule
                                    .getActivity()
                                    .getCurrentTabCreator()
                                    .createFrozenTab(state, tab.getId(), /* index= */ 1);
                });
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    public void testTabLoadIfNeededEnsuresBackingForMediaCapture() {
        TabImpl tab = createFrozenTab();

        ThreadUtils.runOnUiThreadBlocking(
                () -> tab.loadIfNeeded(TabLoadIfNeededCaller.MEDIA_CAPTURE_PICKER));

        ThreadUtils.runOnUiThreadBlocking(() -> assertTrue(tab.hasBacking()));
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    public void testTabIsNotInPWA() {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mActivityTestRule.getActivity().getActivityTab(),
                            Matchers.notNullValue());
                },
                DEFAULT_MAX_TIME_TO_WAIT_IN_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        assertFalse(mActivityTestRule.getActivityTab().isTabInPWA());
        assertTrue(mActivityTestRule.getActivityTab().isTabInBrowser());
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    @EnableFeatures({"AnnotatedPageContentsVirtualStructure"})
    public void testOnProvideVirtualStructure() {
        var url = mActivityTestRule.getTestServer().getURL(TEST_PATH);
        mActivityTestRule.loadUrl(url);
        TabImpl tabImpl = (TabImpl) mActivityTestRule.getActivityTab();
        TestViewStructure viewStructure = new TestViewStructure();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tabImpl.getContentView().onProvideVirtualStructure(viewStructure);
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    if (viewStructure.getChildCount() != 1) return false;
                    var rootNode = viewStructure.getChild(0);
                    if (!rootNode.hasExtras()) return false;
                    return rootNode.getExtras()
                            .containsKey("org.chromium.chrome.browser.AnnotatedPageContents");
                },
                DEFAULT_MAX_TIME_TO_WAIT_IN_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        assertEquals(1, viewStructure.getChildCount());
        var rootNode = viewStructure.getChild(0);
        assertTrue(rootNode.hasExtras());
        assertTrue(
                rootNode.getExtras()
                        .containsKey("org.chromium.chrome.browser.AnnotatedPageContents"));
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    @DisableFeatures({
        ChromeFeatureList.ANDROID_PINNED_TABS,
        ChromeFeatureList.ANDROID_PINNED_TABS_TABLET_TAB_STRIP
    })
    public void testSetIsPinned_TrueBecomesFalseWhenFeatureDisabled() {
        TabImpl tab = (TabImpl) mActivityTestRule.getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(() -> tab.setIsPinned(true));
        assertFalse("Tab should not be pinned when the feature is disabled.", tab.getIsPinned());
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    @EnableFeatures({
        ChromeFeatureList.TAB_FREEZING_USES_DISCARD,
        ContentFeatures.WEB_CONTENTS_DISCARD
    })
    public void testFreeze_withDiscard() {
        final TabImpl tab = (TabImpl) mActivityTestRule.getActivityTab();

        // Open a new tab to hide the initial tab. The new tab becomes active.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .getTabModelSelector()
                            .openNewTab(
                                    new LoadUrlParams("about:blank"),
                                    TabLaunchType.FROM_CHROME_UI,
                                    tab,
                                    tab.isIncognito());
                });

        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(tab.isHidden(), Matchers.is(true)));

        ThreadUtils.runOnUiThreadBlocking(tab::freeze);

        assertFalse("Tab should not be frozen", tab.isFrozen());
        assertNotNull("WebContents should not be null after discard", tab.getWebContents());
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    @DisableFeatures({
        ChromeFeatureList.TAB_FREEZING_USES_DISCARD,
        ContentFeatures.WEB_CONTENTS_DISCARD
    })
    public void testFreeze_withoutDiscard() {
        final TabImpl tab = (TabImpl) mActivityTestRule.getActivityTab();

        // Open a new tab to hide the initial tab. The new tab becomes active.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .getTabModelSelector()
                            .openNewTab(
                                    new LoadUrlParams("about:blank"),
                                    TabLaunchType.FROM_CHROME_UI,
                                    tab,
                                    tab.isIncognito());
                });

        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(tab.isHidden(), Matchers.is(true)));

        ThreadUtils.runOnUiThreadBlocking(tab::freeze);

        assertTrue("Tab should be frozen", tab.isFrozen());
        assertNull("WebContents should be null after freezeInternal", tab.getWebContents());
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    @EnableFeatures({
        ChromeFeatureList.TAB_FREEZING_USES_DISCARD,
        ContentFeatures.WEB_CONTENTS_DISCARD
    })
    public void testFreezeAndAppendPendingNavigation_withDiscard() {
        final TabImpl tab = (TabImpl) mActivityTestRule.getActivityTab();

        // Open a new tab to hide the initial tab. The new tab becomes active.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .getTabModelSelector()
                            .openNewTab(
                                    new LoadUrlParams("about:blank"),
                                    TabLaunchType.FROM_CHROME_UI,
                                    tab,
                                    tab.isIncognito());
                });

        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(tab.isHidden(), Matchers.is(true)));

        String url = mActivityTestRule.getTestServer().getURL(TEST_PATH);
        ThreadUtils.runOnUiThreadBlocking(
                () -> tab.freezeAndAppendPendingNavigation(new LoadUrlParams(url), "title"));

        assertFalse("Tab should not be frozen", tab.isFrozen());
        assertNotNull("WebContents should not be null", tab.getWebContents());
        assertNotNull("Pending load params should not be null", tab.getPendingLoadParams());
        assertEquals(
                "Pending load params should have the new URL",
                url,
                tab.getPendingLoadParams().getUrl());
        assertEquals("URL should be updated", url, tab.getUrl().getSpec());
        assertEquals("Title should be updated", "title", tab.getTitle());
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    @DisableFeatures({
        ChromeFeatureList.TAB_FREEZING_USES_DISCARD,
        ContentFeatures.WEB_CONTENTS_DISCARD
    })
    public void testFreezeAndAppendPendingNavigation_withoutDiscard() {
        final TabImpl tab = (TabImpl) mActivityTestRule.getActivityTab();

        // Open a new tab to hide the initial tab. The new tab becomes active.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .getTabModelSelector()
                            .openNewTab(
                                    new LoadUrlParams("about:blank"),
                                    TabLaunchType.FROM_CHROME_UI,
                                    tab,
                                    tab.isIncognito());
                });

        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(tab.isHidden(), Matchers.is(true)));

        String url = mActivityTestRule.getTestServer().getURL(TEST_PATH);
        ThreadUtils.runOnUiThreadBlocking(
                () -> tab.freezeAndAppendPendingNavigation(new LoadUrlParams(url), "title"));

        assertTrue("Tab should be frozen", tab.isFrozen());
        assertNull("WebContents should be null", tab.getWebContents());
        assertNotNull("WebContentsState should not be null", tab.getWebContentsState());
        assertNull("Pending load params should be null", tab.getPendingLoadParams());
        assertEquals(
                "WebContentsState should have the new URL",
                url,
                tab.getWebContentsState().getVirtualUrlFromState());
        assertEquals("URL should be updated", url, tab.getUrl().getSpec());
        assertEquals("Title should be updated", "title", tab.getTitle());
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    @EnableFeatures({
        ChromeFeatureList.TAB_FREEZING_USES_DISCARD,
        ContentFeatures.WEB_CONTENTS_DISCARD
    })
    public void testFreezeAndAppendPendingNavigation_withDiscard_loadUrlDiscardsPendingLoad() {
        final TabImpl tab = (TabImpl) mActivityTestRule.getActivityTab();

        // Open a new tab to hide the initial tab. The new tab becomes active.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .getTabModelSelector()
                            .openNewTab(
                                    new LoadUrlParams("about:blank"),
                                    TabLaunchType.FROM_CHROME_UI,
                                    tab,
                                    tab.isIncognito());
                });

        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(tab.isHidden(), Matchers.is(true)));

        String url1 = mActivityTestRule.getTestServer().getURL(TEST_PATH);
        ThreadUtils.runOnUiThreadBlocking(
                () -> tab.freezeAndAppendPendingNavigation(new LoadUrlParams(url1), "title1"));

        assertNotNull("Pending load params should not be null", tab.getPendingLoadParams());
        assertEquals(
                "Pending load params should have the new URL",
                url1,
                tab.getPendingLoadParams().getUrl());
        assertEquals("URL should be updated", url1, tab.getUrl().getSpec());
        assertEquals("Title should be updated", "title1", tab.getTitle());

        String url2 =
                mActivityTestRule.getTestServer().getURL("/chrome/test/data/android/simple.html");
        ThreadUtils.runOnUiThreadBlocking(() -> tab.loadUrl(new LoadUrlParams(url2)));

        assertNull("Pending load params should be null", tab.getPendingLoadParams());
        assertEquals("URL should be updated", url2, tab.getUrl().getSpec());
        // Title will be updated asynchronously.
    }

    @Test
    @SmallTest
    @Feature({"Tab"})
    @DisableFeatures({
        ChromeFeatureList.TAB_FREEZING_USES_DISCARD,
        ContentFeatures.WEB_CONTENTS_DISCARD
    })
    public void testFreezeAndAppendPendingNavigation_withoutDiscard_loadUrlDiscardsPendingLoad() {
        final TabImpl tab = (TabImpl) mActivityTestRule.getActivityTab();

        // Open a new tab to hide the initial tab. The new tab becomes active.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .getTabModelSelector()
                            .openNewTab(
                                    new LoadUrlParams("about:blank"),
                                    TabLaunchType.FROM_CHROME_UI,
                                    tab,
                                    tab.isIncognito());
                });

        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(tab.isHidden(), Matchers.is(true)));

        String url1 = mActivityTestRule.getTestServer().getURL(TEST_PATH);
        ThreadUtils.runOnUiThreadBlocking(
                () -> tab.freezeAndAppendPendingNavigation(new LoadUrlParams(url1), "title1"));

        assertNotNull("WebContentsState should not be null", tab.getWebContentsState());
        assertNull("Pending load params should be null", tab.getPendingLoadParams());
        assertEquals(
                "WebContentsState should have the new URL",
                url1,
                tab.getWebContentsState().getVirtualUrlFromState());
        assertEquals("URL should be updated", url1, tab.getUrl().getSpec());
        assertEquals("Title should be updated", "title1", tab.getTitle());

        String url2 =
                mActivityTestRule.getTestServer().getURL("/chrome/test/data/android/simple.html");
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tab.loadIfNeeded(TabLoadIfNeededCaller.OTHER);
                    tab.loadUrl(new LoadUrlParams(url2));
                });

        assertNull("Pending load params should be null", tab.getPendingLoadParams());
        assertEquals("URL should be updated", url2, tab.getUrl().getSpec());
        // Title will be updated asynchronously.
    }
}
