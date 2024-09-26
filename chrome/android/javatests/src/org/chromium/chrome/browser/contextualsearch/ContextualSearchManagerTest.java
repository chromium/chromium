// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper.waitForSecondChromeTabbedActivity;
import static org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper.waitForTabs;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Build;
import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import androidx.test.filters.LargeTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import com.google.common.collect.ImmutableMap;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.MaxAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.PanelState;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchBarControl;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchImageControl;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchQuickActionControl;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchInternalStateController.InternalState;
import org.chromium.chrome.browser.contextualsearch.ResolvedSearchTerm.CardTag;
import org.chromium.chrome.browser.findinpage.FindToolbar;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.FullscreenTestUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.ViewUtils;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

// TODO(donnd): Create class with limited API to encapsulate the internals of simulations.

/** Tests the Contextual Search Manager using instrumentation tests. */
@RunWith(ChromeJUnit4ClassRunner.class)
// NOTE: Disable online detection so we we'll default to online on test bots with no network.
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.CONTEXTUAL_SEARCH_DISABLE_ONLINE_DETECTION)
@Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
@Batch(Batch.PER_CLASS)
public class ContextualSearchManagerTest extends ContextualSearchInstrumentationBase {
    @Mock private EdgeToEdgeController mMockEdgeToEdgeController;

    // DOM element IDs in our test page based on what functions they trigger.
    // TODO(donnd): add more, and also the associated Search Term, or build a similar mapping.
    /**
     * The DOM node for the word "search" on the test page, which causes a plain search response
     * with the Search Term "Search" from the Fake server.
     */
    private static final String SIMPLE_SEARCH_NODE_ID = "search";

    /** Feature maps that we use for parameterized tests. */

    /** This represents the current fully-launched configuration. */
    private static final ImmutableMap<String, Boolean> ENABLE_NONE = ImmutableMap.of();

    private UserActionTester mActionTester;

    @Override
    @Before
    public void setUp() throws Exception {
        mTestPage = "/chrome/test/data/android/contextualsearch/tap_test.html";
        super.setUp();
    }

    @Override
    @After
    public void tearDown() throws Exception {
        super.tearDown();
        if (mActionTester != null) mActionTester.tearDown();
        mPanel.setEdgeToEdgeControllerSupplierForTesting(() -> null);
    }

    // ============================================================================================
    // UMA assertions
    // ============================================================================================

    private void assertUserActionRecorded(String userActionFullName) throws Exception {
        Assert.assertTrue(mActionTester.getActions().contains(userActionFullName));
    }

    // ============================================================================================
    // Test Cases
    // ============================================================================================

    /** Tests swiping the overlay open, after an initial trigger that activates the peeking card. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(DeviceFormFactor.PHONE)
    @DisabledTest(message = "crbug.com/1373276")
    public void testSwipeExpand() throws Exception {
        // TODO(donnd): enable for all features.
        assertNoSearchesLoaded();
        triggerResolve("intelligence");
        assertNoSearchesLoaded();

        // Fake a search term resolution response.
        fakeResponse(
                false, 200, "Intelligence", "United States Intelligence", "alternate-term", false);
        assertContainsParameters("Intelligence", "alternate-term");
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        assertLoadedLowPriorityUrl();

        waitForPanelToPeek();
        flingPanelUp();
        waitForPanelToExpand();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        assertLoadedLowPriorityUrl();
    }

    /**
     * Tests swiping the overlay open, after an initial non-resolve search that activates the
     * peeking card, followed by closing the panel. This test also verifies that we don't create any
     * {@link WebContents} or load any URL until the panel is opened.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(DeviceFormFactor.PHONE)
    public void testNonResolveSwipeExpand() throws Exception {
        simulateNonResolveSearch("search");
        assertNoWebContents();
        assertLoadedNoUrl();

        expandPanelAndAssert();
        assertWebContentsCreated();
        assertLoadedNormalPriorityUrl();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());

        // tap the base page to close.
        closePanel();
        Assert.assertEquals(1, mFakeServer.getLoadedUrlCount());
        assertNoWebContents();
    }

    /** Tests that long-press selects text, and a subsequent tap will unselect text. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testLongPressGestureSelects() throws Exception {
        longPressNode("intelligence");
        Assert.assertEquals("Intelligence", getSelectedText());
        waitForPanelToPeek();
        assertLoadedNoUrl(); // No load (preload) after long-press until opening panel.
        clickNode("question-mark");
        waitForPanelToCloseAndSelectionEmpty();
        Assert.assertTrue(TextUtils.isEmpty(getSelectedText()));
        assertLoadedNoUrl();
    }

    /** Tests that a Resolve gesture selects the expected text. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    // Previously flaky, disabled 4/2021.  https://crbug.com/1192285, https://crbug.com/1192561
    public void testResolveGestureSelects() throws Exception {
        simulateResolveSearch("intelligence");
        Assert.assertEquals("Intelligence", getSelectedText());
        assertLoadedLowPriorityUrl();
        clickNode("question-mark");
        waitForPanelToClose();
        Assert.assertTrue(getSelectedText() == null || getSelectedText().isEmpty());
    }

    // ============================================================================================
    // Various Tests
    // ============================================================================================

    /*
     * Test that tapping on the open-new-tab icon before having a resolved search term does not
     * promote to a tab, and that after the resolution it does promote to a tab.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testPromotesToTab() throws Exception {
        // -------- SET UP ---------
        // Track Tab creation with this helper.
        final CallbackHelper tabCreatedHelper = new CallbackHelper();
        int tabCreatedHelperCallCount = tabCreatedHelper.getCallCount();
        TabModelSelectorObserver observer =
                new TabModelSelectorObserver() {
                    @Override
                    public void onNewTabCreated(Tab tab, @TabCreationState int creationState) {
                        tabCreatedHelper.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> sActivityTestRule.getActivity().getTabModelSelector().addObserver(observer));
        // Track User Actions
        mActionTester = new UserActionTester();

        // -------- TEST ---------
        // Start a slow-resolve search and maximize the Panel.
        simulateSlowResolveSearch("search");
        maximizePanel();
        waitForPanelToMaximize();

        // A click should not promote since we are still waiting to Resolve.
        forceOpenTabIconClick();

        // Assert that the Panel is still maximized.
        waitForPanelToMaximize();

        // Let the Search Term Resolution finish.
        simulateSlowResolveFinished();

        // Now a click to promote to a separate tab.
        forceOpenTabIconClick();

        // The Panel should now be closed.
        waitForPanelToClose();

        // Make sure a tab was created.
        tabCreatedHelper.waitForCallback(tabCreatedHelperCallCount);

        // Make sure we captured the promotion in UMA.
        assertUserActionRecorded("ContextualSearch.TabPromotion");

        // -------- CLEAN UP ---------
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivityTestRule.getActivity().getTabModelSelector().removeObserver(observer);
                });
    }

    // ============================================================================================
    // Undecided/Decided users.
    // ============================================================================================

    /** Tests that we do not resolve or preload when the privacy Opt-in has not been accepted. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testUnacceptedPrivacy() throws Exception {
        mPolicy.overrideDecidedStateForTesting(false);

        simulateResolvableSearchAndAssertResolveAndPreload("states", false);
    }

    /** Tests that we do resolve and preload when the privacy Opt-in has been accepted. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testAcceptedPrivacy() throws Exception {
        mPolicy.overrideDecidedStateForTesting(true);

        simulateResolvableSearchAndAssertResolveAndPreload("states", true);
    }

    /**
     * Tests ContextualSearchManager#shouldInterceptNavigation for a case that an initial navigation
     * has a user gesture but the redirected external navigation doesn't.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testRedirectedExternalNavigationWithUserGesture() throws Exception {
        ExternalNavigationHandler.sAllowIntentsToSelfForTesting = true;
        simulateResolveSearch("intelligence");
        GURL initialUrl = new GURL("http://test.com");
        final NavigationHandle navigationHandle =
                NavigationHandle.createForTesting(
                        initialUrl,
                        /* isRendererInitiated= */ true,
                        PageTransition.LINK,
                        /* hasUserGesture= */ true);

        GURL redirectUrl = new GURL(EXTERNAL_APP_URL);

        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        new Runnable() {
                            @Override
                            public void run() {
                                Assert.assertFalse(
                                        mPanel.getOverlayPanelContent()
                                                .getInterceptNavigationDelegateForTesting()
                                                .shouldIgnoreNavigation(
                                                        navigationHandle,
                                                        initialUrl,
                                                        false,
                                                        false));
                                Assert.assertEquals(0, mActivityMonitor.getHits());

                                navigationHandle.didRedirect(redirectUrl, true);
                                Assert.assertTrue(
                                        mPanel.getOverlayPanelContent()
                                                .getInterceptNavigationDelegateForTesting()
                                                .shouldIgnoreNavigation(
                                                        navigationHandle,
                                                        redirectUrl,
                                                        false,
                                                        false));
                                Assert.assertEquals(1, mActivityMonitor.getHits());
                            }
                        });
    }

    /**
     * Tests ContextualSearchManager#shouldInterceptNavigation for a case that an external
     * navigation has a user gesture.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testExternalNavigationWithUserGesture() throws Exception {
        ExternalNavigationHandler.sAllowIntentsToSelfForTesting = true;
        testExternalNavigationImpl(true);
    }

    /**
     * Tests ContextualSearchManager#shouldInterceptNavigation for a case that an external
     * navigation doesn't have a user gesture.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testExternalNavigationWithoutUserGesture() throws Exception {
        ExternalNavigationHandler.sAllowIntentsToSelfForTesting = true;
        testExternalNavigationImpl(false);
    }

    private void testExternalNavigationImpl(boolean hasGesture) throws Exception {
        simulateResolveSearch("intelligence");
        GURL url = new GURL(EXTERNAL_APP_URL);
        final NavigationHandle navigationHandle =
                NavigationHandle.createForTesting(
                        url, /* isRendererInitiated= */ true, PageTransition.LINK, hasGesture);

        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        new Runnable() {
                            @Override
                            public void run() {
                                Assert.assertTrue(
                                        mPanel.getOverlayPanelContent()
                                                .getInterceptNavigationDelegateForTesting()
                                                .shouldIgnoreNavigation(
                                                        navigationHandle, url, false, false));
                            }
                        });
        Assert.assertEquals(hasGesture ? 1 : 0, mActivityMonitor.getHits());
    }

    // ============================================================================================
    // Translate Tests
    // ============================================================================================

    /** Tests that a simple Tap without language determination does not trigger translation. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapWithoutLanguage() throws Exception {
        // Resolving an English word should NOT trigger translation.
        simulateResolveSearch("search");

        // Make sure we did not try to trigger translate.
        Assert.assertFalse(mManager.getRequest().isTranslationForced());
    }

    /** Tests the Translate Caption on a resolve gesture forces a translation. */
    @Test
    @LargeTest
    @Feature({"ContextualSearch"})
    public void testTranslateCaption() throws Exception {
        // Resolving a German word should trigger translation.
        simulateResolveSearch("german");

        // Make sure we tried to trigger translate.
        Assert.assertTrue(
                "Translation was not forced with the current request URL: "
                        + mManager.getRequest().getSearchUrl(),
                mManager.getRequest().isTranslationForced());
    }

    // ============================================================================================
    // END Translate Tests
    // ============================================================================================

    /**
     * Tests that Contextual Search works in fullscreen. Specifically, tests that tapping a word
     * peeks the panel, expanding the bar results in the bar ending at the correct spot in the page
     * and tapping the base page closes the panel.
     */
    @Test
    @SmallTest
    // Previously flaky and disabled 4/2021. See https://crbug.com/1197102
    @Feature({"ContextualSearch"})
    @Restriction(DeviceFormFactor.PHONE)
    public void testTapContentAndExpandPanelInFullscreen() throws Exception {
        // Toggle tab to fulllscreen.
        FullscreenTestUtils.togglePersistentFullscreenAndAssert(
                sActivityTestRule.getActivity().getActivityTab(),
                true,
                sActivityTestRule.getActivity());

        // Simulate a resolving search and assert that the panel peeks.
        simulateResolveSearch("search");

        // Expand the panel and assert that it ends up in the right place.
        expandPanelAndAssert();
        final ContextualSearchPanel panel =
                (ContextualSearchPanel) mManager.getContextualSearchPanel();
        Assert.assertEquals(
                panel.getHeight(), panel.getPanelHeightFromState(PanelState.EXPANDED), 0);

        // Tap the base page and assert that the panel is closed.
        tapBasePageToClosePanel();
    }

    /** Tests that the Contextual Search panel is dismissed when entering or exiting fullscreen. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    // Previously flaky on phones: https://crbug.com/765796
    public void testPanelDismissedOnToggleFullscreen() throws Exception {
        // Simulate a resolving search and assert that the panel peeks.
        simulateResolveSearch("search");

        // Toggle tab to fullscreen.
        Tab tab = sActivityTestRule.getActivity().getActivityTab();
        FullscreenTestUtils.togglePersistentFullscreenAndAssert(
                tab, true, sActivityTestRule.getActivity());

        // Assert that the panel is closed.
        waitForPanelToClose();

        // Simulate a resolving search and assert that the panel peeks.
        simulateResolveSearch("search");

        // Toggle tab to non-fullscreen.
        FullscreenTestUtils.togglePersistentFullscreenAndAssert(
                tab, false, sActivityTestRule.getActivity());

        // Assert that the panel is closed.
        waitForPanelToClose();
    }

    /**
     * Tests that ContextualSearchImageControl correctly sets either the icon sprite or thumbnail as
     * visible.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testImageControl() throws Exception {
        simulateResolveSearch("search");

        final ContextualSearchImageControl imageControl = mPanel.getImageControl();

        Assert.assertFalse(imageControl.getThumbnailVisible());
        Assert.assertTrue(TextUtils.isEmpty(imageControl.getThumbnailUrl()));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    imageControl.setThumbnailUrl("http://someimageurl.com/image.png");
                    imageControl.onThumbnailFetched(true);
                });

        Assert.assertTrue(imageControl.getThumbnailVisible());
        Assert.assertEquals(imageControl.getThumbnailUrl(), "http://someimageurl.com/image.png");

        ThreadUtils.runOnUiThreadBlocking(() -> imageControl.hideCustomImage(false));

        Assert.assertFalse(imageControl.getThumbnailVisible());
        Assert.assertTrue(TextUtils.isEmpty(imageControl.getThumbnailUrl()));
    }

    // TODO(twellington): Add an end-to-end integration test for fetching a thumbnail based on a
    //                    a URL that is included with the resolution response.

    /**
     * Tests that the quick action caption is set correctly when one is available. Also tests that
     * the caption gets changed when the panel is expanded and reset when the panel is closed.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testQuickActionCaptionAndImage() throws Exception {
        CompositorAnimationHandler.setTestingMode(true);

        // Simulate a resolving search to show the Bar, then set the quick action data.
        simulateResolveSearch("search");
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mPanel.onSearchTermResolved(
                                "search",
                                null,
                                "geo:47.6,-122.3",
                                QuickActionCategory.ADDRESS,
                                CardTag.CT_LOCATION,
                                /* relatedSearchesInBar= */ null));

        ContextualSearchBarControl barControl = mPanel.getSearchBarControl();
        ContextualSearchQuickActionControl quickActionControl = barControl.getQuickActionControl();
        ContextualSearchImageControl imageControl = mPanel.getImageControl();

        // Check that the peeking bar is showing the quick action data.
        Assert.assertTrue(quickActionControl.hasQuickAction());
        Assert.assertTrue(barControl.getCaptionVisible());
        // There may be different Map apps on different devices, so we just check that we got an
        // open intent of some kind.
        final String expectedCaptionStart = "Open in ";
        Assert.assertEquals(
                expectedCaptionStart,
                barControl.getCaptionText().subSequence(0, expectedCaptionStart.length()));
        Assert.assertEquals(1.f, imageControl.getCustomImageVisibilityPercentage(), 0);

        // Expand the bar.
        ThreadUtils.runOnUiThreadBlocking(() -> mPanel.simulateTapOnEndButton());
        waitForPanelToExpand();

        // Check that the expanded bar is showing the correct image.
        Assert.assertEquals(0.f, imageControl.getCustomImageVisibilityPercentage(), 0);

        // Go back to peeking.
        peekPanel();

        // Assert that the quick action data is showing.
        Assert.assertTrue(barControl.getCaptionVisible());
        Assert.assertEquals(
                expectedCaptionStart,
                barControl.getCaptionText().subSequence(0, expectedCaptionStart.length()));
        // TODO(donnd): figure out why we get ~0.65 on Oreo rather than 1. https://crbug.com/818515.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            Assert.assertEquals(1.f, imageControl.getCustomImageVisibilityPercentage(), 0);
        } else {
            Assert.assertTrue(0.5f < imageControl.getCustomImageVisibilityPercentage());
        }

        CompositorAnimationHandler.setTestingMode(false);
    }

    /** Tests that an intent is sent when the bar is tapped and a quick action is available. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    // Previously disabled: https://crbug.com/1315417
    public void testQuickActionIntent() throws Exception {
        // Add a new filter to the activity monitor that matches the intent that should be fired.
        IntentFilter quickActionFilter = new IntentFilter(Intent.ACTION_VIEW);
        quickActionFilter.addDataScheme("geo");

        // Note that we don't reuse mActivityMonitor here or we would leak the one already added
        // (unless we removed it here first). When ActivityMonitors leak, Instrumentation silently
        // ignores matching ones added after and tests will fail.
        ActivityMonitor activityMonitor =
                InstrumentationRegistry.getInstrumentation()
                        .addMonitor(
                                quickActionFilter,
                                new Instrumentation.ActivityResult(Activity.RESULT_OK, null),
                                true);

        // Simulate a resolving search to show the Bar, then set the quick action data.
        simulateResolveSearch("search");
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mPanel.onSearchTermResolved(
                                "search",
                                null,
                                "geo:47.6,-122.3",
                                QuickActionCategory.ADDRESS,
                                CardTag.CT_LOCATION,
                                /* relatedSearchesInBar= */ null));

        sActivityTestRule.getActivity().onUserInteraction();
        // Expand the panel to trigger the quick action intent to be fired.
        expandPanelAndAssert();

        CriteriaHelper.pollUiThread(
                () -> {
                    int intentHits = activityMonitor.getHits();
                    Criteria.checkThat(intentHits, Matchers.is(1));
                });

        // Assert that an intent was fired.
        Assert.assertEquals(1, activityMonitor.getHits());
        InstrumentationRegistry.getInstrumentation().removeMonitor(activityMonitor);
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    // TODO(donnd): reenable - recent fixes as of 3/31/2023
    @DisableIf.Build(sdk_is_greater_than = Build.VERSION_CODES.O, message = "crbug.com/1075895")
    // Previously disabled: https://crbug.com/1127796
    public void testQuickActionUrl() throws Exception {
        final String testUrl = mTestServer.getURL("/chrome/test/data/android/google.html");

        // Simulate a resolving search to show the Bar, then set the quick action data.
        simulateResolveSearch("search");
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mPanel.onSearchTermResolved(
                                "search",
                                null,
                                testUrl,
                                QuickActionCategory.WEBSITE,
                                CardTag.CT_URL,
                                /* relatedSearchesInBar= */ null));

        sActivityTestRule.getActivity().onUserInteraction();
        // Expand the bar which should trigger the quick action.
        expandPanel();

        // Wait for that URL to be loaded.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            ChromeTabUtils.getUrlStringOnUiThread(
                                    sActivityTestRule.getActivity().getActivityTab()),
                            Matchers.is(testUrl));
                });
    }

    private void runDictionaryCardTest(@CardTag int cardTag) throws Exception {
        // Simulate a resolving search to show the Bar, then set the quick action data.
        simulateResolveSearch("search");
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mPanel.onSearchTermResolved(
                                "obscure · əbˈskyo͝or",
                                null,
                                null,
                                QuickActionCategory.NONE,
                                cardTag,
                                /* relatedSearchesInBar= */ null));

        expandPanelAndAssert();
    }

    /**
     * Tests that the flow for showing dictionary definitions works, and that tapping in the bar
     * just opens the panel instead of taking some action.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    // Previously disabled: http://crbug.com/1296677
    public void testDictionaryDefinitions() throws Exception {
        runDictionaryCardTest(CardTag.CT_DEFINITION);
    }

    /**
     * Tests that the flow for showing dictionary definitions works, and that tapping in the bar
     * just opens the panel instead of taking some action.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testContextualDictionaryDefinitions() throws Exception {
        runDictionaryCardTest(CardTag.CT_CONTEXTUAL_DEFINITION);
    }

    /** Tests accessibility mode: Tap and Long-press don't activate CS. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testAccesibilityMode() throws Exception {
        mManager.onAccessibilityModeChanged(true);

        // Simulate a tap that resolves to show the Bar.
        clickNode("intelligence");
        assertNoWebContents();
        assertNoSearchesLoaded();

        // Simulate a Long-press.
        longPressNodeWithoutWaiting("states");
        assertNoWebContents();
        assertNoSearchesLoaded();
        mManager.onAccessibilityModeChanged(false);
    }

    /** Tests when FirstRun is not completed: Tap and Long-press don't activate CS. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testFirstRunNotCompleted() throws Exception {
        // Store the original value in a temp, and mark the first run as not completed
        // for this test case.
        // Getting value from shared preference rather than FirstRunStatus#getFirstRunFlowComplete
        // to get rid of the impact from commandline switch. See https://crbug.com/1158467
        boolean originalIsFirstRunComplete =
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE, false);
        FirstRunStatus.setFirstRunFlowComplete(false);

        // Simulate a tap that resolves to show the Bar.
        clickNode("intelligence");
        assertNoWebContents();
        assertNoSearchesLoaded();

        // Simulate a Long-press.
        longPressNodeWithoutWaiting("states");
        assertNoWebContents();
        assertNoSearchesLoaded();

        // Restore the original shared preference value before this test case ends.
        FirstRunStatus.setFirstRunFlowComplete(originalIsFirstRunComplete);
    }

    // ============================================================================================
    // Internal State Controller tests, which ensure that the internal logic flows as expected for
    // each type of triggering gesture.
    // ============================================================================================

    /**
     * Tests that the Manager cycles through all the expected Internal States on Tap that Resolves.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    // Previously flaky and disabled 4/2021.  https://crbug.com/1058297
    public void testAllInternalStatesVisitedResolvingTap() throws Exception {
        // Set up a tracking version of the Internal State Controller.
        ContextualSearchInternalStateControllerWrapper internalStateControllerWrapper =
                ContextualSearchInternalStateControllerWrapper
                        .makeNewInternalStateControllerWrapper(mManager);
        mManager.setContextualSearchInternalStateController(internalStateControllerWrapper);

        // Simulate a gesture that resolves to show the Bar.
        simulateResolveSearch("search");

        Assert.assertEquals(
                "Some states were started but never finished",
                internalStateControllerWrapper.getStartedStates(),
                internalStateControllerWrapper.getFinishedStates());
        Assert.assertEquals(
                "The resolving Tap gesture did not sequence through the expected states.",
                ContextualSearchInternalStateControllerWrapper.EXPECTED_TAP_RESOLVE_SEQUENCE,
                internalStateControllerWrapper.getFinishedStates());
        Assert.assertEquals(
                "The Tap gesture did not trigger a resolved search, or the resolve sequence did "
                        + "not complete.",
                InternalState.SEARCH_COMPLETED,
                internalStateControllerWrapper.getState());
    }

    /**
     * Tests that the Manager cycles through all the expected Internal States on Long-press that
     * Resolves.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testAllInternalStatesVisitedResolvingLongpress__rsearches() throws Exception {
        // Set up a tracking version of the Internal State Controller.
        ContextualSearchInternalStateControllerWrapper internalStateControllerWrapper =
                ContextualSearchInternalStateControllerWrapper
                        .makeNewInternalStateControllerWrapper(mManager);
        mManager.setContextualSearchInternalStateController(internalStateControllerWrapper);

        // Simulate a resolving search to show the Bar.
        longPressNode(SIMPLE_SEARCH_NODE_ID);
        fakeAResponse();

        Assert.assertEquals(
                "Some states were started but never finished",
                internalStateControllerWrapper.getStartedStates(),
                internalStateControllerWrapper.getFinishedStates());
        Assert.assertEquals(
                "The resolving Long-press gesture did not sequence through the expected states.",
                ContextualSearchInternalStateControllerWrapper.EXPECTED_LONGPRESS_RESOLVE_SEQUENCE,
                internalStateControllerWrapper.getFinishedStates());
        Assert.assertEquals(
                "The Long-press gesture did not trigger a resolved search, or the resolve sequence "
                        + "did not complete.",
                InternalState.SEARCH_COMPLETED,
                internalStateControllerWrapper.getState());
    }

    /** Tests that the Manager cycles through all the expected Internal States on a Long-press. */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    // Previously flaky and disabled 4/2021.  https://crbug.com/1192285
    @DisabledTest(
            message = "TODO(donnd): reenable after unifying resolving and non-resolving longpress.")
    public void testAllInternalStatesVisitedNonResolveLongpress() throws Exception {
        // Set up a tracking version of the Internal State Controller.
        ContextualSearchInternalStateControllerWrapper internalStateControllerWrapper =
                ContextualSearchInternalStateControllerWrapper
                        .makeNewInternalStateControllerWrapper(mManager);
        mManager.setContextualSearchInternalStateController(internalStateControllerWrapper);

        // Simulate a Long-press to show the Bar.
        simulateNonResolveSearch("search");

        Assert.assertEquals(
                "Some states were started but never finished",
                internalStateControllerWrapper.getStartedStates(),
                internalStateControllerWrapper.getFinishedStates());
        Assert.assertEquals(
                "The non-resolving Long-press gesture didn't sequence through all of the expected "
                        + " states.",
                ContextualSearchInternalStateControllerWrapper.EXPECTED_LONGPRESS_SEQUENCE,
                internalStateControllerWrapper.getFinishedStates());
    }

    // ============================================================================================
    // Various tests
    // ============================================================================================

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    // Previously flaky and disabled 4/2021.  https://crbug.com/1180304
    public void testTriggeringContextualSearchHidesFindInPageOverlay() throws Exception {
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                sActivityTestRule.getActivity(),
                R.id.find_in_page_id);

        CriteriaHelper.pollUiThread(
                () -> {
                    FindToolbar findToolbar =
                            (FindToolbar)
                                    sActivityTestRule.getActivity().findViewById(R.id.find_toolbar);
                    Criteria.checkThat(findToolbar, Matchers.notNullValue());
                    Criteria.checkThat(findToolbar.isShown(), Matchers.is(true));
                    Criteria.checkThat(findToolbar.isAnimating(), Matchers.is(false));
                });

        // Don't type anything to Find because that may cause scrolling which makes clicking in the
        // page flaky.

        View findToolbar = sActivityTestRule.getActivity().findViewById(R.id.find_toolbar);
        Assert.assertTrue(findToolbar.isShown());

        simulateResolveSearch("search");

        waitForPanelToPeek();
        Assert.assertFalse(
                "Find Toolbar should no longer be shown once Contextual Search Panel appeared",
                findToolbar.isShown());
    }

    /**
     * Tests Tab reparenting. When a tab moves from one activity to another the
     * ContextualSearchTabHelper should detect the change and handle gestures for it too. This
     * happens with multiwindow modes.
     */
    @Test
    @LargeTest
    @Feature({"ContextualSearch"})
    @CommandLineFlags.Add(ChromeSwitches.DISABLE_TAB_MERGING_FOR_TESTING)
    @MaxAndroidSdkLevel(value = Build.VERSION_CODES.R, reason = "crbug.com/1301017")
    public void testTabReparenting() throws Exception {
        // Move our "tap_test" tab to another activity.
        final ChromeActivity ca = sActivityTestRule.getActivity();

        // Create a new tab so |ca| isn't destroyed.
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(), ca);
        ChromeTabUtils.switchTabInCurrentTabModel(ca, 0);

        int testTabId = ca.getActivityTab().getId();
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                ca,
                R.id.move_to_other_window_menu_id);

        // Wait for the second activity to start up and be ready for interaction.
        final ChromeTabbedActivity activity2 = waitForSecondChromeTabbedActivity();
        waitForTabs("CTA2", activity2, 1, testTabId);

        // Trigger on a word and wait for the selection to be established.
        triggerNode(activity2.getActivityTab(), "search");
        CriteriaHelper.pollUiThread(
                () -> {
                    String selection =
                            activity2
                                    .getContextualSearchManagerForTesting()
                                    .getSelectionController()
                                    .getSelectedText();
                    Criteria.checkThat(selection, Matchers.is("Search"));
                });
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        activity2
                                .getCurrentTabModel()
                                .closeTabs(TabClosureParams.closeAllTabs().build()));
        ApplicationTestUtils.finishActivity(activity2);
    }

    // --------------------------------------------------------------------------------------------
    // Longpress-resolve Feature tests: force long-press experiment and make sure that triggers.
    // --------------------------------------------------------------------------------------------
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testTapIsIgnoredWithLongpressResolveEnabled() throws Exception {
        clickNode("states");
        Assert.assertNull(getSelectedText());
        assertPanelClosedOrUndefined();
        assertLoadedNoUrl();
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testLongpressResolveEnabled() throws Exception {
        longPressNode("states");
        assertLoadedNoUrl();
        assertSearchTermRequested();

        fakeResponse(false, 200, "states", "United States Intelligence", "alternate-term", false);
        waitForPanelToPeek();
        assertLoadedLowPriorityUrl();
        assertContainsParameters("states", "alternate-term");
    }

    /** Monitor user action UMA recording operations. */
    private static class UserActionMonitor extends UserActionTester {
        // TODO(donnd): merge into UserActionTester. See https://crbug.com/1103757.
        private Set<String> mUserActionPrefixes;
        private Map<String, Integer> mUserActionCounts;

        /**
         * @param userActionPrefixes A set of plain prefix strings for user actions to monitor.
         */
        UserActionMonitor(Set<String> userActionPrefixes) {
            mUserActionPrefixes = userActionPrefixes;
            mUserActionCounts = new HashMap<String, Integer>();
            for (String action : mUserActionPrefixes) {
                mUserActionCounts.put(action, 0);
            }
        }

        @Override
        public void onResult(String action) {
            for (String entry : mUserActionPrefixes) {
                if (action.startsWith(entry)) {
                    mUserActionCounts.put(entry, mUserActionCounts.get(entry) + 1);
                }
            }
        }

        /**
         * Gets the count of user actions recorded for the given prefix.
         *
         * @param actionPrefix The plain string prefix to lookup (must match a constructed entry)
         * @return The count of user actions recorded for that prefix.
         */
        int get(String actionPrefix) {
            return mUserActionCounts.get(actionPrefix);
        }
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @DisabledTest(message = "https://crbug.com/1048827, https://crbug.com/1181088")
    public void testLongpressExtendingSelectionExactResolve() throws Exception {
        // Set up UserAction monitoring.
        Set<String> userActions = new HashSet();
        userActions.add("ContextualSearch.SelectionEstablished");
        userActions.add("ContextualSearch.ManualRefine");
        UserActionMonitor userActionMonitor = new UserActionMonitor(userActions);

        // First test regular long-press.  It should not require an exact resolve.
        longPressNode("search");
        fakeAResponse();
        assertSearchTermRequested();
        assertExactResolve(false);

        // Long press a node without release so we can simulate the user extending the selection.
        long downTime = longPressNodeWithoutUp("search");

        // Extend the selection to the nearby word.
        longPressExtendSelection("term", "resolution", downTime);
        waitForSelectActionBarVisible();
        fakeAResponse();
        assertSearchTermRequested();
        assertExactResolve(true);

        // Check UMA metrics recorded.
        Assert.assertEquals(2, userActionMonitor.get("ContextualSearch.ManualRefine"));
        Assert.assertEquals(2, userActionMonitor.get("ContextualSearch.SelectionEstablished"));
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testPeekStateHeight() throws Exception {
        final float defaultHeight = 70;
        longPressNode("states");
        assertLoadedNoUrl();
        assertSearchTermRequested();

        Assert.assertEquals(
                "Default height for the bar should be 70 DP.",
                defaultHeight,
                mPanel.getBarHeight(),
                0.001f);

        // Increase the selected TextView height to be taller than the default height.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPanel.getSearchBarControl().setCaption("Increase Height");
                    TextView textView = mPanel.getSearchBarControl().getCaptionTextView();
                    float dpToPx =
                            InstrumentationRegistry.getInstrumentation()
                                    .getContext()
                                    .getResources()
                                    .getDisplayMetrics()
                                    .density;
                    textView.getLayoutParams().height = (int) ((defaultHeight * 2) * dpToPx);
                    ViewUtils.requestLayout(textView, "Update the selected TextView height");
                });

        fakeResponse(false, 200, "states", "United States Intelligence", "alternate-term", false);
        waitForPanelToPeek();

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mPanel.getBarHeight(), Matchers.greaterThan(defaultHeight));
                });
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @EnableFeatures({"DrawEdgeToEdge, DrawCutoutEdgeToEdge"})
    public void testPeekStateHeightGrowsForEdgeToEdge() throws Exception {
        // Run through with the fake controller using the default logic.
        mPanel.setEdgeToEdgeControllerSupplierForTesting(() -> mMockEdgeToEdgeController);
        when(mMockEdgeToEdgeController.getBottomInset()).thenReturn(0);
        final float defaultHeight = 70;

        simulateResolveSearch("search");
        CriteriaHelper.pollUiThread(
                () -> {
                    try {
                        Criteria.checkThat(
                                mPanel.getPanelHeightFromState(PanelState.PEEKED),
                                Matchers.equalTo(defaultHeight));
                    } catch (CriteriaNotSatisfiedException ex) {
                        throw new AssertionError(
                                "Error - Peek Height or Bar Height is not the normal expected value"
                                        + " for these tests.",
                                ex);
                    }
                });
        closePanel();
        verify(mMockEdgeToEdgeController, atLeastOnce()).getBottomInset();

        // Set ToEdge, which returns a non-zero inset. The panel should be positioned higher.
        final int arbitraryGestureNavHeight = 100;
        reset(mMockEdgeToEdgeController);
        when(mMockEdgeToEdgeController.getBottomInset()).thenReturn(arbitraryGestureNavHeight);
        simulateResolveSearch("search");
        CriteriaHelper.pollUiThread(
                () -> {
                    try {
                        Criteria.checkThat(
                                mPanel.getPanelHeightFromState(PanelState.PEEKED),
                                Matchers.equalTo(defaultHeight + arbitraryGestureNavHeight));
                    } catch (CriteriaNotSatisfiedException ex) {
                        throw new AssertionError(
                                "When EdgeToEdge is active the Peek position should be inset for"
                                        + " the Bottom Gesture Nav  Bar.",
                                ex);
                    }
                });
        closePanel();
        verify(mMockEdgeToEdgeController, atLeastOnce()).getBottomInset();
    }
}
