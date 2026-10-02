// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.junit.Assert.assertEquals;

import android.view.View;

import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableLeakChecks;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.glic.GlicKeyedService.GlicInvocationSource;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Java Integration tests for Glic Android native bottom sheet and JNI boundary. */
@DoNotBatch(reason = "Runs full C++ environment.")
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    // Glic guest page runs in an isolated storage partition which blocks cleartext
    // HTTP traffic by default on Android (ERR_CLEARTEXT_NOT_PERMITTED). The test
    // environment runs an HTTPS server instead; ignore cert errors for this local
    // test server.
    "ignore-certificate-errors"
})
@EnableFeatures({ChromeFeatureList.GLIC, ChromeFeatureList.TAB_BOTTOM_SHEET})
@DisableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL)
// Pre-existing Activity leak in StartupSigninStateCheckController, hit by every
// test here because setUp() signs in.
// TODO(crbug.com/561654982): Remove once the signin leak is fixed.
@DisableLeakChecks("crbug.com/561654982")
public class GlicAndroidMojoIntegrationTest {
    // Increase polling timeout to reduce flakiness hopefully.
    private static final long MOJO_BINDING_TIMEOUT_MS = 10000L;
    private static final long MOJO_BINDING_POLLING_INTERVAL_MS = 100L;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private GlicTestEnvironmentAndroid mTestEnv;
    private Tab mTab;

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    GlicEnabling.setEnabledForTesting(true, /* forwardToNative= */ true);
                    mTestEnv = new GlicTestEnvironmentAndroid();
                });

        mActivityTestRule.startMainActivityOnBlankPage();
        mTab = mActivityTestRule.getActivityTab();
        ChromeTabUtils.waitForTabPageLoaded(mTab, (String) null);
    }

    @After
    public void tearDown() {
        if (mTestEnv != null) {
            ThreadUtils.runOnUiThreadBlocking(() -> mTestEnv.destroy());
        }
    }

    /** Must be called on the UI thread. */
    private GlicKeyedService getService() {
        return GlicKeyedServiceFactory.getForProfile(mTab.getProfile());
    }

    private void waitForWebClientConnected() {
        CriteriaHelper.pollUiThread(
                () -> mTestEnv.isWebClientConnected() && mTestEnv.getGuestWebContents() != null,
                MOJO_BINDING_TIMEOUT_MS,
                MOJO_BINDING_POLLING_INTERVAL_MS);
    }

    private void toggleGlic(boolean open) {
        TabbedRootUiCoordinator coordinator =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                (TabbedRootUiCoordinator)
                                        mActivityTestRule
                                                .getActivity()
                                                .getRootUiCoordinatorForTesting());
        ThreadUtils.runOnUiThreadBlocking(
                () -> coordinator.toggleGlic(open, GlicInvocationSource.UNSUPPORTED));
        CriteriaHelper.pollUiThread(
                () -> coordinator.getTabBottomSheetManagerForTesting().isSheetShowing() == open,
                MOJO_BINDING_TIMEOUT_MS,
                MOJO_BINDING_POLLING_INTERVAL_MS);
    }

    /** Must be called on the UI thread. */
    private ModalDialogManager getModalDialogManager() {
        WindowAndroid windowAndroid = mActivityTestRule.getActivity().getWindowAndroid();
        return windowAndroid == null ? null : windowAndroid.getModalDialogManager();
    }

    private String getLastPrompt() throws Exception {
        WebContents guestWebContents =
                ThreadUtils.runOnUiThreadBlocking(() -> mTestEnv.getGuestWebContents());
        Assert.assertNotNull(guestWebContents);
        String result =
                JavaScriptUtils.runJavascriptWithAsyncResult(
                        guestWebContents,
                        "(async () => {"
                                + "  const res = await window.glicTestClient.waitForPrompt();"
                                + "  window.domAutomationController.send(res);"
                                + "})();");
        if (result.startsWith("\"") && result.endsWith("\"") && result.length() >= 2) {
            result = result.substring(1, result.length() - 1);
        }
        return result;
    }

    @Test
    @LargeTest
    public void testMojoBindingAndInvoke() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        getService()
                                .invokeWithAutoSubmit(
                                        mTab, "Hello from Java integration test!", 27));

        // Wait until WebClient mojo connection has completed
        waitForWebClientConnected();

        // Assert that the Mojo WebClient received the auto-submit prompt
        assertEquals("Hello from Java integration test!", getLastPrompt());
    }

    @Test
    @LargeTest
    public void testLatestChatInstanceRecovery() throws Throwable {
        // 1. Open Glic, type/invoke a prompt
        ThreadUtils.runOnUiThreadBlocking(
                () -> getService().invokeWithAutoSubmit(mTab, "State preservation test", 27));
        waitForWebClientConnected();
        assertEquals("State preservation test", getLastPrompt());

        // 2. Close bottom sheet (releasing the view container)
        toggleGlic(false);

        // 3. Re-open Glic
        toggleGlic(true);
        waitForWebClientConnected();

        // 4. Assert that the guest instance state has been preserved (the prompt still exists)
        assertEquals("State preservation test", getLastPrompt());
    }

    @Test
    @LargeTest
    public void testShowExperimentalOptInDialogAndDismissViaCloseButton() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(
                () -> getService().showExperimentalOptInDialogForTesting(mTab));

        CriteriaHelper.pollUiThread(
                () -> {
                    ModalDialogManager modalDialogManager = getModalDialogManager();
                    return modalDialogManager != null && modalDialogManager.isShowing();
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel model = getModalDialogManager().getCurrentDialogForTest();
                    Assert.assertNotNull(model);
                    View customView = model.get(ModalDialogProperties.CUSTOM_VIEW);
                    Assert.assertNotNull(customView);
                    View closeButton =
                            customView.findViewById(R.id.glic_experimental_opt_in_close_button);
                    Assert.assertNotNull(closeButton);
                    closeButton.performClick();
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    ModalDialogManager modalDialogManager = getModalDialogManager();
                    return modalDialogManager == null || !modalDialogManager.isShowing();
                });
    }

    @Test
    @LargeTest
    public void testShareAndUnshareTabsAndRecentlyActiveInstances() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        getService()
                                .invokeWithConversation(
                                        mTab, "test-conv-1", GlicInvocationSource.UNSUPPORTED));
        waitForWebClientConnected();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    GlicKeyedService service = getService();
                    List<ConversationInfo> recent = service.getRecentlyActiveInstances(10);
                    assertEquals(1, recent.size());
                    String instanceId = recent.get(0).instanceId;
                    Assert.assertFalse(instanceId.isEmpty());

                    List<Tab> tabs = List.of(mTab);
                    service.unshareTabs(tabs);
                    Assert.assertFalse(service.isTabPinnedToAnyInstance(tabs));

                    service.shareTabs(
                            tabs,
                            instanceId,
                            /* newConversation= */ false,
                            GlicInvocationSource.UNSUPPORTED);
                    Assert.assertTrue(service.isTabPinnedToAnyInstance(tabs));

                    service.unshareTabs(tabs);
                    Assert.assertFalse(service.isTabPinnedToAnyInstance(tabs));
                });
    }
}
