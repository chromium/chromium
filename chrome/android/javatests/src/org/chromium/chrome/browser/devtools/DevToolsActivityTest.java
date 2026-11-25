// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.devtools;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.app.Dialog;
import android.content.Intent;
import android.util.Base64;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ScrollView;
import android.widget.Spinner;

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
import org.chromium.components.page_info.CertificateChainHelper;
import org.chromium.components.page_info.CertificateViewer;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeoutException;

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

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testRecreateOnNightModeChange() {
        Tab inspectedTab = mTestRule.startOnBlankPage().getTab();
        verifyRecreateOnNightModeChange(inspectedTab, /* incognito= */ false);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testRecreateOnNightModeChangeForIncognitoTab() {
        Tab inspectedTab = mTestRule.startOnIncognitoBlankPage().getTab();
        assertTrue(runOnUiThreadBlocking(() -> inspectedTab.getProfile().isPrimaryOtrProfile()));
        verifyRecreateOnNightModeChange(inspectedTab, /* incognito= */ true);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testShowCertificateViewer() throws TimeoutException {
        mTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
        Tab inspectedTab =
                mTestRule.startOnTestServerUrl("/chrome/test/data/android/about.html").getTab();
        WebContents inspectedContents = runOnUiThreadBlocking(inspectedTab::getWebContents);

        byte[][] certChain =
                runOnUiThreadBlocking(
                        () -> CertificateChainHelper.getCertificateChain(inspectedContents));
        assertNotNull(certChain);
        assertTrue(certChain.length > 0);

        StringBuilder jsCertArray = new StringBuilder("[");
        for (int i = 0; i < certChain.length; i++) {
            if (i > 0) jsCertArray.append(",");
            jsCertArray.append("\"");
            jsCertArray.append(Base64.encodeToString(certChain[i], Base64.NO_WRAP));
            jsCertArray.append("\"");
        }
        jsCertArray.append("]");

        DevToolsActivity activity = openDevToolsAndWait(inspectedTab);
        assertDevToolsTabAttached(activity, /* incognito= */ false);

        CriteriaHelper.pollUiThread(
                () ->
                        activity.getActivityTab() != null
                                && activity.getActivityTab().getWebContents() != null,
                "DevTools frontend WebContents was not attached.");
        WebContents devToolsContents =
                runOnUiThreadBlocking(() -> activity.getActivityTab().getWebContents());
        CriteriaHelper.pollUiThread(
                () -> !devToolsContents.isLoading(),
                "DevTools frontend did not finish loading.",
                CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL_LONG,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        return "true"
                                .equals(
                                        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                                devToolsContents,
                                                "typeof window.InspectorFrontendHost"
                                                        + "?.showCertificateViewer ==="
                                                        + " 'function'"));
                    } catch (TimeoutException e) {
                        return false;
                    }
                },
                "DevTools InspectorFrontendHost.showCertificateViewer was not ready.",
                CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL_LONG,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        String showCertJs =
                "window.InspectorFrontendHost.showCertificateViewer(" + jsCertArray + ");";
        assertEquals(
                "null",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        devToolsContents, showCertJs + " null;"));

        CriteriaHelper.pollUiThread(
                () -> {
                    CertificateViewer viewer = activity.getCertificateViewerForTesting();
                    Dialog dialog = viewer != null ? viewer.getDialogForTesting() : null;
                    return dialog != null && dialog.isShowing();
                },
                "CertificateViewer dialog was not shown.");
        Dialog dialog =
                runOnUiThreadBlocking(
                        () -> activity.getCertificateViewerForTesting().getDialogForTesting());
        assertNotNull(dialog);
        assertCertificateDialogPopulated(dialog, certChain.length);

        // A second invocation while the dialog is open should keep the existing dialog.
        assertEquals(
                "null",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        devToolsContents, showCertJs + " null;"));
        assertSame(
                dialog,
                runOnUiThreadBlocking(
                        () -> activity.getCertificateViewerForTesting().getDialogForTesting()));

        // Dismissing the dialog and invoking showCertificateViewer again (e.g. after navigating or
        // inspecting another origin) should open a new populated dialog on the cached viewer.
        runOnUiThreadBlocking(dialog::dismiss);
        assertFalse(runOnUiThreadBlocking(dialog::isShowing));
        assertEquals(
                "null",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        devToolsContents, showCertJs + " null;"));
        CriteriaHelper.pollUiThread(
                () -> {
                    CertificateViewer viewer = activity.getCertificateViewerForTesting();
                    Dialog currentDialog = viewer != null ? viewer.getDialogForTesting() : null;
                    return currentDialog != null
                            && currentDialog != dialog
                            && currentDialog.isShowing();
                },
                "CertificateViewer dialog was not reopened after dismissal.");
        Dialog reopenedDialog =
                runOnUiThreadBlocking(
                        () -> activity.getCertificateViewerForTesting().getDialogForTesting());
        assertNotNull(reopenedDialog);
        assertNotSame(dialog, reopenedDialog);
        assertCertificateDialogPopulated(reopenedDialog, certChain.length);

        // Closing the inspected tab destroys DevToolsActivity and dismisses the reopened dialog.
        closeInspectedTabAndWaitForDevToolsToClose(inspectedTab, activity);
        assertFalse(runOnUiThreadBlocking(reopenedDialog::isShowing));
    }

    private void verifyRecreateOnNightModeChange(Tab inspectedTab, boolean incognito) {
        DevToolsActivity initialActivity = openDevToolsAndWait(inspectedTab);
        assertDevToolsTabAttached(initialActivity, incognito);
        WebContents devToolsWebContents =
                runOnUiThreadBlocking(() -> initialActivity.getActivityTab().getWebContents());
        assertNotNull(devToolsWebContents);

        // Recreate on night mode change (e.g. light -> dark) and verify DevTools stays open with
        // the same frontend WebContents.
        DevToolsActivity recreatedOnce = recreateOnNightModeChange(initialActivity);
        assertDevToolsActivityAliveWithWebContents(recreatedOnce, devToolsWebContents, incognito);

        // Recreate a second time (e.g. dark -> light) to verify repeated theme toggles.
        DevToolsActivity recreatedTwice = recreateOnNightModeChange(recreatedOnce);
        assertDevToolsActivityAliveWithWebContents(recreatedTwice, devToolsWebContents, incognito);

        // Closing the inspected tab should still close the recreated DevToolsActivity.
        closeInspectedTabAndWaitForDevToolsToClose(inspectedTab, recreatedTwice);
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

    private static DevToolsActivity recreateOnNightModeChange(DevToolsActivity activity) {
        DevToolsActivity recreated =
                ApplicationTestUtils.waitForActivityWithClass(
                        DevToolsActivity.class, Stage.RESUMED, activity::onNightModeStateChanged);
        assertNotSame(activity, recreated);
        CriteriaHelper.pollUiThread(
                activity::isDestroyed,
                "Previous DevToolsActivity wasn't destroyed.",
                CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL_LONG,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        CriteriaHelper.pollUiThread(
                recreated::didFinishNativeInitialization,
                "Recreated DevToolsActivity did not finish native initialization.");
        return recreated;
    }

    private static void assertDevToolsActivityAliveWithWebContents(
            DevToolsActivity activity, WebContents expectedWebContents, boolean incognito) {
        assertDevToolsTabAttached(activity, incognito);
        runOnUiThreadBlocking(
                () -> {
                    assertFalse("DevToolsActivity should not be destroyed", activity.isDestroyed());
                    assertNotNull(activity.getActivityTab());
                    assertSame(expectedWebContents, activity.getActivityTab().getWebContents());
                    assertFalse(
                            "DevTools WebContents should not be destroyed",
                            expectedWebContents.isDestroyed());
                });
    }

    private static void assertCertificateDialogPopulated(Dialog dialog, int expectedCertCount) {
        runOnUiThreadBlocking(
                () -> {
                    ViewGroup content = dialog.findViewById(android.R.id.content);
                    assertNotNull(content);
                    ViewGroup dialogContainer = (ViewGroup) content.getChildAt(0);
                    Spinner spinner = (Spinner) dialogContainer.getChildAt(1);
                    assertEquals(expectedCertCount, spinner.getCount());
                    assertFalse(((String) spinner.getSelectedItem()).isEmpty());

                    ScrollView scrollView = (ScrollView) dialogContainer.getChildAt(2);
                    ViewGroup certContainer = (ViewGroup) scrollView.getChildAt(0);
                    assertEquals(expectedCertCount, certContainer.getChildCount());
                    assertEquals(View.VISIBLE, certContainer.getChildAt(0).getVisibility());
                    assertTrue(((ViewGroup) certContainer.getChildAt(0)).getChildCount() > 0);
                });
    }
}
