// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import androidx.test.filters.SmallTest;

import org.hamcrest.MatcherAssert;
import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;

/** Tests the Contextual Search Manager using instrumentation tests. */
// NOTE: Disable online detection so we we'll default to online on test bots with no network.
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.CONTEXTUAL_SEARCH_DISABLE_ONLINE_DETECTION)
@Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
@Batch(Batch.PER_CLASS)
public class ContextualSearchCriticalTest extends ContextualSearchInstrumentationBase {
    @Rule public JniMocker mocker = new JniMocker();

    // Needed to avoid issues on Release builds where Natives is made final and can not be Spy'd
    // below.
    @Mock ContextualSearchManagerJni mUnusedContextualSearchManagerJni;

    private ContextualSearchManager.Natives mContextualSearchManagerNatives;

    @Override
    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        mTestPage = "/chrome/test/data/android/contextualsearch/tap_test.html";
        mContextualSearchManagerNatives = Mockito.spy(new ContextualSearchManagerJni());
        mocker.mock(ContextualSearchManagerJni.TEST_HOOKS, mContextualSearchManagerNatives);
        super.setUp();
    }

    // ============================================================================================
    // Test Cases
    // ============================================================================================

    /** Tests that only a single low-priority request is issued for a trigger/open sequence. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    // Previously disabled:  https://crbug.com/1058297
    public void testResolveCausesOneLowPriorityRequest() throws Exception {
        simulateSlowResolveSearch("states");

        // We should not make a second-request until we get a good response from the first-request.
        assertLoadedNoUrl();
        Assert.assertEquals(0, mFakeServer.getLoadedUrlCount());
        simulateSlowResolveFinished();
        assertLoadedLowPriorityUrl();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // When the second request succeeds, we should not issue a new request.
        fakeContentViewDidNavigate(false);
        assertLoadedLowPriorityUrl();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // When the bar opens, we should not make any additional request.
        expandPanelAndAssert();
        assertLoadedLowPriorityUrl();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        assertLoadedLowPriorityUrl();
    }

    /** Tests that a failover for a prefetch request is issued after the panel is opened. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testPrefetchFailoverRequestMadeAfterOpen() throws Exception {
        simulateSlowResolveSearch("states");

        // We should not make a SERP request until we get a good response from the resolve request.
        assertLoadedNoUrl();
        Assert.assertEquals(0, mFakeServer.getLoadedUrlCount());
        fakeResponse(false, 200, "states", "United States Intelligence", "alternate-term", false);
        assertLoadedLowPriorityUrl();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // When the second request fails, we should not automatically issue a new request.
        fakeContentViewDidNavigate(true);
        assertLoadedLowPriorityUrl();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Once the bar opens, we make a new request at normal priority.
        expandPanelAndAssert();
        assertLoadedNormalPriorityUrl();
        Assert.assertEquals(2, mFakeServer.getLoadedUrlCount());
    }

    /** Tests a simple triggering gesture with disable-preload set. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    // Previously flaky and disabled 4/2021.  https://crbug.com/1192285
    public void testResolveDisablePreload() throws Exception {
        simulateSlowResolveSearch("intelligence");

        assertSearchTermRequested();
        boolean doPreventPreload = true;
        fakeResponse(
                false, 200, "Intelligence", "display-text", "alternate-term", doPreventPreload);
        assertLoadedNoUrl();
        waitForPanelToPeek();
        assertLoadedNoUrl();
    }

    /**
     * Tests that an error from the Search Term Resolution request causes a fallback to a search
     * request for the literal selection.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    // Previously disabled: crbug.com/765403
    public void testSearchTermResolutionError() throws Exception {
        simulateSlowResolveSearch("states");
        assertSearchTermRequested();
        fakeResponse(false, 403, "", "", "", false);
        assertLoadedNoUrl();
        expandPanelAndAssert();
        assertLoadedNormalPriorityUrl();
    }

    // ============================================================================================
    // Content Tests
    // ============================================================================================

    /** Tests that resolve followed by expand makes Content visible. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(DeviceFormFactor.PHONE)
    public void testResolveContentVisibility() throws Exception {
        // Simulate a resolving search and make sure Content is not visible.
        simulateResolveSearch();
        assertWebContentsCreatedButNeverMadeVisible();

        // Expanding the Panel should make the Content visible.
        expandPanelAndAssert();
        assertWebContentsVisible();

        // Closing the Panel should destroy the Content.
        tapBasePageToClosePanel();
        assertNoWebContents();
    }

    /**
     * Tests that a non-resolving trigger followed by panel-expand creates Content and makes it
     * visible.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(DeviceFormFactor.PHONE)
    @DisabledTest(message = "Flaky, see crbug.com/40821849")
    public void testNonResolveContentVisibility() throws Exception {
        // Simulate a non-resolve search and make sure no Content is created.
        simulateNonResolveSearch("search");
        assertNoWebContents();
        assertNoSearchesLoaded();

        // Expanding the Panel should make the Content visible.
        expandPanelAndAssert();
        assertWebContentsCreated();
        assertWebContentsVisible();

        // Closing the Panel should destroy the Content.
        tapBasePageToClosePanel();
        assertNoWebContents();
    }

    /**
     * Tests that moving panel up and down after a resolving search will only load the Content once.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(DeviceFormFactor.PHONE)
    public void testResolveMultipleSwipeOnlyLoadsContentOnce() throws Exception {
        // Simulate a resolving search and make sure Content is not visible.
        simulateResolveSearch("search");
        assertWebContentsCreatedButNeverMadeVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Expanding the Panel should make the Content visible.
        expandPanelAndAssert();
        assertWebContentsVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Shrinking the Panel down should not change the visibility or load content again.
        peekPanel();
        assertWebContentsVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Expanding the Panel should not change the visibility or load content again.
        expandPanelAndAssert();
        assertWebContentsVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Closing the Panel should destroy the Content.
        tapBasePageToClosePanel();
        assertNoWebContents();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
    }

    /**
     * Tests that moving the panel up and down after a non-resolving search will only load the
     * Content once.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(DeviceFormFactor.PHONE)
    public void testNonResolveMultipleSwipeOnlyLoadsContentOnce() throws Exception {
        // Simulate a non-resolve search and make sure no Content is created.
        simulateNonResolveSearch("search");
        assertNoWebContents();
        assertNoSearchesLoaded();

        // Expanding the Panel should load the URL and make the Content visible.
        expandPanelAndAssert();
        assertWebContentsCreated();
        assertWebContentsVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Shrinking the Panel down should not change the visibility or load content again.
        peekPanel();
        assertWebContentsVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Expanding the Panel should not change the visibility or load content again.
        expandPanelAndAssert();
        assertWebContentsVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Closing the Panel should destroy the Content.
        tapBasePageToClosePanel();
        assertNoWebContents();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
    }

    /**
     * Tests that chained tap searches create new Content. Chained Tap searches allow immediate
     * triggering of a tap when quite close to a previous tap selection since the user may have just
     * missed the intended target.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @DisabledTest(message = "crbug.com/1404658")
    public void testChainedSearchCreatesNewContent() throws Exception {
        // This test depends on preloading the content - which is loaded and not made visible.
        // We only preload when the user has decided to accept the privacy opt-in.
        mPolicy.overrideDecidedStateForTesting(true);

        // Simulate a resolving search and make sure Content is not visible.
        simulateResolveSearch("search");
        assertWebContentsCreatedButNeverMadeVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        WebContents wc1 = getPanelWebContents();

        waitToPreventDoubleTapRecognition();

        // Simulate a new resolving search and make sure new Content is created.
        simulateResolveSearch("term");
        assertWebContentsCreatedButNeverMadeVisible();
        Assert.assertEquals(2, mFakeServer.getLoadedUrlCount());
        WebContents wc2 = getPanelWebContents();
        Assert.assertNotSame(wc1, wc2);

        waitToPreventDoubleTapRecognition();

        // Simulate a new resolving search and make sure new Content is created.
        simulateResolveSearch("resolution");
        assertWebContentsCreatedButNeverMadeVisible();
        Assert.assertEquals(3, mFakeServer.getLoadedUrlCount());
        WebContents wc3 = getPanelWebContents();
        Assert.assertNotSame(wc2, wc3);

        // Closing the Panel should destroy the Content.
        closePanel();
        assertNoWebContents();
        Assert.assertEquals(3, mFakeServer.getLoadedUrlCount());
    }

    /** Tests that chained searches load correctly. */
    @Test
    @DisabledTest(message = "crbug.com/549805")
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(DeviceFormFactor.PHONE)
    public void testChainedSearchLoadsCorrectSearchTerm() throws Exception {
        // Simulate a resolving search and make sure Content is not visible.
        simulateResolveSearch("search");
        assertWebContentsCreatedButNeverMadeVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        WebContents wc1 = getPanelWebContents();

        // Expanding the Panel should make the Content visible.
        expandPanelAndAssert();
        assertWebContentsVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Swiping the Panel down should not change the visibility or load content again.
        swipePanelDown();
        waitForPanelToPeek();
        assertWebContentsVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        waitToPreventDoubleTapRecognition();

        // Now simulate a non-resolve search, leaving the Panel peeking. This is a retap, and relies
        // on span#search being sufficient near to span#resolution.
        simulateNonResolveSearch("resolution");

        // Expanding the Panel should load and display the new search.
        expandPanelAndAssert();
        assertWebContentsCreated();
        assertWebContentsVisible();
        Assert.assertEquals(2, mFakeServer.getLoadedUrlCount());
        assertLoadedSearchTermMatches("Resolution");
        WebContents wc2 = getPanelWebContents();
        Assert.assertNotSame(wc1, wc2);

        // Closing the Panel should destroy the Content.
        tapBasePageToClosePanel();
        assertNoWebContents();
        Assert.assertEquals(2, mFakeServer.getLoadedUrlCount());
    }

    /** Tests that chained searches make Content visible when opening the Panel. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testChainedSearchContentVisibility() throws Exception {
        // Chained searches are tap-triggered very close to existing tap-triggered searches, which
        // we refer to as tap-near.
        // Simulate a resolving search and make sure Content is not visible.
        simulateResolveSearch();
        assertWebContentsCreatedButNeverMadeVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        WebContents wc1 = getPanelWebContents();

        waitToPreventDoubleTapRecognition();

        // Now simulate a non-resolve search, leaving the Panel peeking. This is a tab-near, and
        // relies on span#search being sufficient near to span#resolution.
        simulateNonResolveSearch("resolution");
        assertNeverCalledWebContentsOnShow();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Expanding the Panel should load and display the new search.
        expandPanelAndAssert();
        assertWebContentsCreated();
        assertWebContentsVisible();
        Assert.assertEquals(2, mFakeServer.getLoadedUrlCount());
        assertLoadedSearchTermMatches("Resolution");
        WebContents wc2 = getPanelWebContents();
        Assert.assertNotSame(wc1, wc2);
    }

    /**
     * Tests that separate searches make Content visible when opening the Panel. If this test
     * passes, but testChainedSearchContentVisibility() fails, then perhaps something's wrong with
     * retap.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testSeparateSearchContentVisibility() throws Exception {
        // Chained searches are tap-triggered very close to existing tap-triggered searches, which
        // we refer to as tap-near.
        // Simulate a resolving search and make sure Content is not visible.
        simulateResolveSearch();
        assertWebContentsCreatedButNeverMadeVisible();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        WebContents wc1 = getPanelWebContents();

        waitToPreventDoubleTapRecognition();

        // Close panel to break chain, and keep searches separate.
        closePanel();

        // Now simulate a non-resolve search, leaving the Panel peeking.
        simulateNonResolveSearch("resolution");
        assertNeverCalledWebContentsOnShow();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // Expanding the Panel should load and display the new search.
        expandPanelAndAssert();
        assertWebContentsCreated();
        assertWebContentsVisible();
        Assert.assertEquals(2, mFakeServer.getLoadedUrlCount());
        assertLoadedSearchTermMatches("Resolution");
        WebContents wc2 = getPanelWebContents();
        Assert.assertNotSame(wc1, wc2);
    }

    // ============================================================================================
    // History Removal Tests.  These are important for privacy, and are not easy to test manually.
    // ============================================================================================

    /** Tests that a tap followed by closing the Panel removes the loaded URL from history. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapCloseRemovedFromHistory() throws Exception {
        // Simulate a resolving search and make sure a URL was loaded.
        simulateResolveSearch("search");
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        String url = mFakeServer.getLoadedUrl();

        // Close the Panel without seeing the Content.
        closePanel();

        // Now check that the URL has been removed from history.
        ArgumentCaptor<String> urlCaptor = ArgumentCaptor.forClass(String.class);
        Mockito.verify(mContextualSearchManagerNatives)
                .removeLastHistoryEntry(
                        Mockito.anyLong(), Mockito.any(), urlCaptor.capture(), Mockito.anyLong());
        Assert.assertEquals(url, urlCaptor.getValue());
    }

    /**
     * Tests that a tap followed by opening the Panel does not remove the loaded URL from history.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(DeviceFormFactor.PHONE)
    public void testTapExpandNotRemovedFromHistory() throws Exception {
        // Simulate a resolving search and make sure a URL was loaded.
        simulateResolveSearch();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        mFakeServer.getLoadedUrl();

        // Expand Panel so that the Content becomes visible.
        expandPanelAndAssert();

        // Close the Panel.
        tapBasePageToClosePanel();

        // Now check that the URL has not been removed from history, since the Content was seen.
        Mockito.verify(mContextualSearchManagerNatives, Mockito.never())
                .removeLastHistoryEntry(
                        Mockito.anyLong(), Mockito.any(), Mockito.any(), Mockito.anyLong());
    }

    /**
     * Tests that chained searches without opening the Panel removes all loaded URLs from history.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    // TODO(crbug.com/369556626): Flaky on automotive.
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testChainedTapsRemovedFromHistory() throws Exception {
        // Make sure we use tap for the simulateResolveSearch since only tap chains.
        // Simulate a resolving search and make sure a URL was loaded.
        simulateResolveSearch("search");
        String url1 = mFakeServer.getLoadedUrl();
        Assert.assertNotNull(url1);

        waitToPreventDoubleTapRecognition();

        // Simulate another resolving search and make sure another URL was loaded.
        simulateResolveSearch("term");
        String url2 = mFakeServer.getLoadedUrl();
        Assert.assertNotSame(url1, url2);

        waitToPreventDoubleTapRecognition();

        // Simulate another resolving search and make sure another URL was loaded.
        simulateResolveSearch("resolution");
        String url3 = mFakeServer.getLoadedUrl();
        Assert.assertNotSame(url2, url3);

        // Close the Panel without seeing any Content.
        closePanel();

        // Now check that all three URLs have been removed from history.
        Assert.assertEquals(3, mFakeServer.getLoadedUrlCount());

        ArgumentCaptor<String> urlCaptor = ArgumentCaptor.forClass(String.class);
        Mockito.verify(mContextualSearchManagerNatives, Mockito.times(3))
                .removeLastHistoryEntry(
                        Mockito.anyLong(), Mockito.any(), urlCaptor.capture(), Mockito.anyLong());
        MatcherAssert.assertThat(
                urlCaptor.getAllValues(), Matchers.containsInAnyOrder(url1, url2, url3));
    }
}
