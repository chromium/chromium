// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.devtools;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.content.Intent;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.profiles.OtrProfileId;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostRegistry;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.DeviceFormFactor;

/** This class tests the functionality of the {@link DevToolsActivity}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures(ContentFeatureList.ANDROID_DEV_TOOLS_FRONTEND)
@Batch(Batch.PER_CLASS)
public class DevToolsActivityTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public FreshCtaTransitTestRule mTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testOpenDevTools() {
        Tab inspectedTab = mTestRule.startOnBlankPage().getTab();

        DevToolsActivity activity = openDevToolsAndWait(inspectedTab);
        assertDevToolsTabAttached(activity, /* incognito= */ false);

        closeInspectedTabAndWaitForDevToolsToClose(inspectedTab, activity);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testOpenDevToolsForIncognitoTab() {
        Tab inspectedTab = mTestRule.startOnIncognitoBlankPage().getTab();
        assertTrue(runOnUiThreadBlocking(() -> inspectedTab.getProfile().isPrimaryOtrProfile()));

        // DevTools for an Incognito tab must be hosted in the Incognito tab model. Otherwise the
        // DevToolsActivity briefly shows up and then closes itself because it has no tabs.
        DevToolsActivity activity = openDevToolsAndWait(inspectedTab);
        assertDevToolsTabAttached(activity, /* incognito= */ true);

        closeInspectedTabAndWaitForDevToolsToClose(inspectedTab, activity);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    @EnableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    @RequiresRestart("Spawns a new Incognito window")
    public void testOpenDevToolsForTabInIncognitoWindow() {
        IncognitoUtils.setShouldOpenIncognitoAsWindowForTesting(true);
        Tab inspectedTab =
                mTestRule.startOnBlankPage().openNewIncognitoWindowFast().loadAboutBlank().getTab();
        assertTrue(runOnUiThreadBlocking(() -> inspectedTab.getProfile().isPrimaryOtrProfile()));

        DevToolsActivity activity = openDevToolsAndWait(inspectedTab);
        assertDevToolsTabAttached(activity, /* incognito= */ true);

        closeInspectedTabAndWaitForDevToolsToClose(inspectedTab, activity);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testDevToolsNotAllowedForNonPrimaryOtrProfile() {
        Tab regularTab = mTestRule.startOnBlankPage().getTab();
        runOnUiThreadBlocking(
                () -> {
                    Profile regularProfile = ProfileManager.getLastUsedRegularProfile();
                    // E.g. Incognito or Ephemeral Custom Tabs use non-primary OTR profiles.
                    Profile otrProfile =
                            regularProfile.getOffTheRecordProfile(
                                    OtrProfileId.createUnique("DevToolsActivityTest"),
                                    /* createIfNeeded= */ true);
                    assertNotNull(otrProfile);
                    assertFalse(otrProfile.isPrimaryOtrProfile());
                    WebContents webContents =
                            WebContentsFactory.createWebContents(
                                    otrProfile,
                                    /* initiallyHidden= */ true,
                                    /* initializeRenderer= */ false);
                    try {
                        assertFalse(
                                DevToolsWindowAndroid.isDevToolsAllowedFor(
                                        mTestRule.getActivity(), otrProfile, webContents));
                        assertFalse(
                                DevToolsWindowAndroid.isDevToolsAllowedFor(
                                        mTestRule.getActivity(),
                                        otrProfile,
                                        /* webContents= */ null));
                        assertFalse(
                                DevToolsWindowAndroid.isDevToolsAllowedFor(
                                        mTestRule.getActivity(), regularProfile, webContents));
                        assertFalse(
                                DevToolsWindowAndroid.isDevToolsAllowedFor(
                                        mTestRule.getActivity(),
                                        otrProfile,
                                        regularTab.getWebContents()));
                    } finally {
                        webContents.destroy();
                        ProfileManager.destroyWhenAppropriate(otrProfile);
                    }
                });
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testAbortsNonReparentedRecreationAndMissingWebContents() {
        Tab inspectedTab = mTestRule.startOnIncognitoBlankPage().getTab();

        // A launch without a valid in-process WebContents (e.g. after process death) must abort
        // immediately in maybeDispatchLaunchIntent(). Launch from the foreground Activity context
        // to avoid Android background activity launch restrictions on single-window tablets.
        DevToolsActivity missingWebContentsActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        DevToolsActivity.class,
                        Stage.CREATED,
                        () -> {
                            Intent intent =
                                    new Intent(mTestRule.getActivity(), DevToolsActivity.class);
                            intent.addFlags(
                                    Intent.FLAG_ACTIVITY_NEW_TASK
                                            | Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
                            mTestRule.getActivity().startActivity(intent);
                        });
        assertTrue(runOnUiThreadBlocking(missingWebContentsActivity::isFinishing));
        CriteriaHelper.pollUiThread(
                missingWebContentsActivity::isDestroyed,
                "Aborted DevToolsActivity without WebContents wasn't destroyed.");
        assertFalse(
                runOnUiThreadBlocking(missingWebContentsActivity::didFinishNativeInitialization));
        ApplicationTestUtils.waitForActivityState(mTestRule.getActivity(), Stage.RESUMED);

        DevToolsActivity activity = openDevToolsAndWait(inspectedTab);
        assertDevToolsTabAttached(activity, /* incognito= */ true);

        // An OS-initiated recreation (without TabReparentingController) must abort in
        // maybeDispatchLaunchIntent() before native initialization or cookie restoration runs.
        DevToolsActivity recreated =
                ApplicationTestUtils.waitForActivityWithClass(
                        DevToolsActivity.class, Stage.CREATED, activity::recreate);
        assertNotSame(activity, recreated);
        assertTrue(runOnUiThreadBlocking(recreated::isFinishing));
        CriteriaHelper.pollUiThread(
                activity::isDestroyed, "Original DevToolsActivity wasn't destroyed.");
        CriteriaHelper.pollUiThread(
                recreated::isDestroyed, "Aborted recreated DevToolsActivity wasn't destroyed.");
        assertFalse(runOnUiThreadBlocking(recreated::didFinishNativeInitialization));
        ApplicationTestUtils.waitForActivityState(mTestRule.getActivity(), Stage.RESUMED);
    }

    /** Opens DevTools for the given tab and waits for the DevToolsActivity to be initialized. */
    private DevToolsActivity openDevToolsAndWait(Tab inspectedTab) {
        Profile profile = runOnUiThreadBlocking(inspectedTab::getProfile);
        WebContents inspectedContents = runOnUiThreadBlocking(inspectedTab::getWebContents);
        assertTrue(
                runOnUiThreadBlocking(
                        () ->
                                DevToolsWindowAndroid.isDevToolsAllowedFor(
                                        mTestRule.getActivity(), profile, inspectedContents)));

        // Calling openDevTools() shows a DevToolsActivity.
        int incognitoTabHostCount =
                runOnUiThreadBlocking(
                        () -> IncognitoTabHostRegistry.getInstance().getHosts().size());
        DevToolsActivity activity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        DevToolsActivity.class,
                        () ->
                                runOnUiThreadBlocking(
                                        () ->
                                                DevToolsWindowAndroid.openDevTools(
                                                        inspectedContents)));
        CriteriaHelper.pollUiThread(
                activity::didFinishNativeInitialization,
                "DevToolsActivity did not finish native initialization.");

        // DevTools for an Incognito tab must be registered as an IncognitoTabHost, so that the
        // primary OTR profile is not destroyed while DevTools still uses it.
        boolean isIncognito = runOnUiThreadBlocking(profile::isPrimaryOtrProfile);
        assertEquals(
                incognitoTabHostCount + (isIncognito ? 1 : 0),
                (int)
                        runOnUiThreadBlocking(
                                () -> IncognitoTabHostRegistry.getInstance().getHosts().size()));
        return activity;
    }

    /**
     * Asserts that the DevTools frontend is the only tab in the DevToolsActivity's tab model of the
     * expected profile type, and that the activity stays open.
     */
    private static void assertDevToolsTabAttached(DevToolsActivity activity, boolean incognito) {
        runOnUiThreadBlocking(
                () -> {
                    assertFalse("DevToolsActivity is finishing.", activity.isFinishing());

                    TabModel devToolsTabModel = activity.getTabModelSelector().getModel(incognito);
                    assertEquals(1, devToolsTabModel.getCount());
                    Tab devToolsTab = devToolsTabModel.getTabAt(0);
                    assertEquals(incognito, devToolsTab.isIncognitoBranded());
                    assertEquals(incognito, devToolsTab.getProfile().isPrimaryOtrProfile());

                    assertEquals(0, activity.getTabModelSelector().getModel(!incognito).getCount());
                });
    }

    /** Closes the inspected tab and waits for the DevToolsActivity to be closed as well. */
    private static void closeInspectedTabAndWaitForDevToolsToClose(
            Tab inspectedTab, DevToolsActivity activity) {
        runOnUiThreadBlocking(
                () -> {
                    // Look up the tab model through the tab's own window, since an Incognito tab
                    // may live in a separate window when Incognito opens as a window.
                    TabModelSelector selector =
                            TabModelSelectorSupplier.getValueOrNullFrom(
                                    inspectedTab.getWindowAndroid());
                    assertNotNull(selector);
                    TabModel tabModel = selector.getModel(inspectedTab.isIncognitoBranded());
                    tabModel.getTabRemover()
                            .closeTabs(TabClosureParams.closeTab(inspectedTab).build(), false);
                    tabModel.commitAllTabClosures();
                });
        CriteriaHelper.pollUiThread(
                activity::isDestroyed,
                "DevToolsActivity wasn't closed.",
                CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL_LONG,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }
}
