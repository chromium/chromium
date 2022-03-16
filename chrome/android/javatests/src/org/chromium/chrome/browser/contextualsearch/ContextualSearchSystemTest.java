// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.support.test.InstrumentationRegistry;
import android.view.KeyEvent;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.PanelState;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.KeyUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.Arrays;

/**
 * Tests system and application interaction with Contextual Search using instrumentation tests.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
// NOTE: Disable online detection so we we'll default to online on test bots with no network.
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        ContextualSearchFieldTrial.ONLINE_DETECTION_DISABLED,
        "disable-features=" + ChromeFeatureList.CONTEXTUAL_SEARCH_ML_TAP_SUPPRESSION + ","
                + ChromeFeatureList.CONTEXTUAL_SEARCH_THIN_WEB_VIEW_IMPLEMENTATION})
@Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
@Batch(Batch.PER_CLASS)
public class ContextualSearchSystemTest extends ContextualSearchInstrumentationBase {
    /**
     * Parameter provider for enabling/disabling Features under development.
     */
    public static class FeatureParamProvider implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            return Arrays.asList(new ParameterSet().value(EnabledFeature.NONE).name("default"),
                    new ParameterSet()
                            .value(EnabledFeature.TRANSLATIONS)
                            .name("enableTranslations"),
                    new ParameterSet()
                            .value(EnabledFeature.CONTEXTUAL_TRIGGERS)
                            .name("enableContextualTriggers"),
                    new ParameterSet()
                            .value(EnabledFeature.CONTEXTUAL_TRIGGERS_MENU)
                            .name("enableContextualTriggersMenu"),
                    new ParameterSet()
                            .value(EnabledFeature.PRIVACY_NEUTRAL)
                            .name("enablePrivacyNeutralEngagement"),
                    new ParameterSet()
                            .value(EnabledFeature.PRIVACY_NEUTRAL_WITH_RELATED_SEARCHES)
                            .name("enablePrivacyNeutralWithRelatedSearches"));
        }
    }

    private OmniboxTestUtils mOmnibox;

    @Override
    @Before
    public void setUp() throws Exception {
        mTestPage = "/chrome/test/data/android/contextualsearch/simple_test.html";
        super.setUp();
        mOmnibox = new OmniboxTestUtils(sActivityTestRule.getActivity());
    }

    //============================================================================================
    // App Menu suppression support
    //============================================================================================

    /**
     * Simulates pressing the App Menu button.
     */
    private void pressAppMenuKey() {
        pressKey(KeyEvent.KEYCODE_MENU);
    }

    /**
     * Simulates pressing back button.
     */
    private void pressBackButton() {
        pressKey(KeyEvent.KEYCODE_BACK);
    }

    /**
     * Simulates a key press.
     * @param keycode The key's code.
     */
    private void pressKey(int keycode) {
        KeyUtils.singleKeyEventActivity(InstrumentationRegistry.getInstrumentation(),
                sActivityTestRule.getActivity(), keycode);
    }

    private void closeAppMenu() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> sActivityTestRule.getAppMenuCoordinator().getAppMenuHandler().hideAppMenu());
    }

    /**
     * Asserts whether the App Menu is visible.
     */
    private void assertAppMenuVisibility(final boolean isVisible) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(sActivityTestRule.getAppMenuCoordinator()
                                       .getAppMenuHandler()
                                       .isAppMenuShowing(),
                    Matchers.is(isVisible));
        });
    }

    /**
     * Sets the online status and reloads the current Tab with our test URL.
     * @param isOnline Whether to go online.
     */
    private void setOnlineStatusAndReload(boolean isOnline) {
        mFakeServer.setIsOnline(isOnline);
        final String testUrl = mTestServer.getURL(mTestPage);
        final Tab tab = sActivityTestRule.getActivity().getActivityTab();
        TestThreadUtils.runOnUiThreadBlocking(() -> tab.reload());
        // Make sure the page is fully loaded.
        ChromeTabUtils.waitForTabPageLoaded(tab, testUrl);
    }

    //============================================================================================
    // Omnibox
    //============================================================================================

    /**
     * Tests whether the contextual search panel hides when omnibox is clicked.
     */
    //@SmallTest
    //@Feature({"ContextualSearch"})
    @Test
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    @FlakyTest(message = "Flaked in 2017.  https://crbug.com/707529")
    public void testHidesWhenOmniboxFocused() throws Exception {
        clickWordNode("intelligence");

        Assert.assertEquals("Intelligence", mFakeServer.getSearchTermRequested());
        fakeResponse(false, 200, "Intelligence", "display-text", "alternate-term", false);
        assertContainsParameters("Intelligence", "alternate-term");
        waitForPanelToPeek();

        mOmnibox.requestFocus();
        assertPanelClosedOrUndefined();
    }

    //============================================================================================
    // Tab Crash
    //============================================================================================

    /**
     * Tests that the panel closes when its base page crashes.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    // Previously flaky and disabled in 2018.  See https://crbug.com/832539.
    public void testContextualSearchDismissedOnForegroundTabCrash(
            @EnabledFeature int enabledFeature) throws Exception {
        triggerResolve(SEARCH_NODE);
        Assert.assertEquals(SEARCH_NODE_TERM, getSelectedText());
        waitForPanelToPeek();

        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            ChromeTabUtils.simulateRendererKilledForTesting(
                    sActivityTestRule.getActivity().getActivityTab());
        });

        // Give the panelState time to change
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat(mPanel.getPanelState(), Matchers.not(PanelState.PEEKED));
        });

        assertPanelClosedOrUndefined();
    }

    /**
     * Test the the panel does not close when some background tab crashes.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    @FlakyTest(message = "Disabled 4/2021.  https://crbug.com/1192285, /https://crbug.com/1192561")
    public void testContextualSearchNotDismissedOnBackgroundTabCrash(
            @EnabledFeature int enabledFeature) throws Exception {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), sActivityTestRule.getActivity());
        final Tab tab2 =
                TabModelUtils.getCurrentTab(sActivityTestRule.getActivity().getCurrentTabModel());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TabModelUtils.setIndex(sActivityTestRule.getActivity().getCurrentTabModel(), 0, false);
        });

        triggerResolve(SEARCH_NODE);
        Assert.assertEquals(SEARCH_NODE_TERM, getSelectedText());
        waitForPanelToPeek();

        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                () -> { ChromeTabUtils.simulateRendererKilledForTesting(tab2); });

        waitForPanelToPeek();
    }

    //============================================================================================
    // App Menu Suppression
    //============================================================================================

    /**
     * Tests that the App Menu gets suppressed when Search Panel is expanded.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    @DisableIf.Build(supported_abis_includes = "arm64-v8a", message = "crbug.com/596533")
    public void testAppMenuSuppressedWhenExpanded(@EnabledFeature int enabledFeature)
            throws Exception {
        triggerPanelPeek();
        tapPeekingBarToExpandAndAssert();

        pressAppMenuKey();
        assertAppMenuVisibility(false);

        closePanel();

        pressAppMenuKey();
        assertAppMenuVisibility(true);

        closeAppMenu();
    }

    /**
     * Tests that the App Menu gets suppressed when Search Panel is maximized.
     */
    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testAppMenuSuppressedWhenMaximized(@EnabledFeature int enabledFeature)
            throws Exception {
        triggerPanelPeek();
        maximizePanel();
        waitForPanelToMaximize();

        pressAppMenuKey();
        assertAppMenuVisibility(false);

        pressBackButton();
        waitForPanelToClose();

        pressAppMenuKey();
        assertAppMenuVisibility(true);

        closeAppMenu();
    }

    /**
     * Tests that Contextual Search is fully disabled when offline.
     */
    @Test
    @ParameterAnnotations.UseMethodParameter(ContextualSearchManagerTest.FeatureParamProvider.class)
    @FlakyTest(message = "Disabled in 2017.  https://crbug.com/761946")
    // @SmallTest
    // @Feature({"ContextualSearch"})
    // // NOTE: Remove the flag so we will run just this test with onLine detection enabled.
    // @CommandLineFlags.Remove(ContextualSearchFieldTrial.ONLINE_DETECTION_DISABLED)
    public void testNetworkDisconnectedDeactivatesSearch(@EnabledFeature int enabledFeature)
            throws Exception {
        setOnlineStatusAndReload(false);
        // We use the longpress gesture here because unlike Tap it's never suppressed.
        longPressNodeWithoutWaiting(SEARCH_NODE);
        waitForSelectActionBarVisible();
        // Verify the panel didn't open.  It should open by now if CS has not been disabled.
        // TODO(donnd): Consider waiting for some condition to be sure we'll catch all failures,
        // e.g. in case the Bar is about to show but has not yet appeared.  Currently catches ~90%.
        assertPanelClosedOrUndefined();

        // Similar sequence with network connected should peek for Longpress.
        setOnlineStatusAndReload(true);
        longPressNodeWithoutWaiting(SEARCH_NODE);
        waitForSelectActionBarVisible();
        waitForPanelToPeek();
    }
}
