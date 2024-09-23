// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.previewtab;

import android.view.ViewGroup;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabObserver;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabSheetContent;
import org.chromium.chrome.browser.firstrun.DisableFirstRun;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.contextmenu.ContextMenuUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/**
 * Tests the Preview Tab, also known as the Ephemeral Tab. Based on the
 * FocusedEditableTextFieldZoomTest and TabsTest.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE)
public class PreviewTabTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public EmbeddedTestServerRule mTestServer = new EmbeddedTestServerRule();

    /** Needed to ensure the First Run Flow is disabled automatically during setUp, etc. */
    @Rule public DisableFirstRun mDisableFirstRunFlowRule = new DisableFirstRun();

    private static final String BASE_PAGE = "/chrome/test/data/android/previewtab/base_page.html";
    private static final String PREVIEW_TAB =
            "/chrome/test/data/android/previewtab/preview_tab.html";
    private static final String PREVIEW_TAB_DOM_ID = "previewTab";
    private static final String NEAR_BOTTOM_DOM_ID = "nearBottom";
    private static final String ANOTHER_PAGE_DOM_ID = "anotherPage";

    private EphemeralTabCoordinator mEphemeralTabCoordinator;
    private BottomSheetTestSupport mSheetTestSupport;
    private TestEphemeralTabObserver mEphemeralTabObserver;

    private static class TestEphemeralTabObserver implements EphemeralTabObserver {
        public final CallbackHelper onToolbarCreatedCallback = new CallbackHelper();
        public final CallbackHelper onNavigationStartedCallback = new CallbackHelper();
        public final CallbackHelper onTitleSetCallback = new CallbackHelper();

        @Override
        public void onToolbarCreated(ViewGroup toolbarView) {
            onToolbarCreatedCallback.notifyCalled();
        }

        @Override
        public void onNavigationStarted(GURL clickedUrl) {
            onNavigationStartedCallback.notifyCalled();
        }

        @Override
        public void onTitleSet(EphemeralTabSheetContent sheetContent, String title) {
            onTitleSetCallback.notifyCalled();
        }
    }

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityWithURL(mTestServer.getServer().getURL(BASE_PAGE));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabbedRootUiCoordinator tabbedRootUiCoordinator =
                            ((TabbedRootUiCoordinator)
                                    mActivityTestRule
                                            .getActivity()
                                            .getRootUiCoordinatorForTesting());
                    mEphemeralTabCoordinator =
                            tabbedRootUiCoordinator.getEphemeralTabCoordinatorSupplier().get();
                });
        mSheetTestSupport =
                new BottomSheetTestSupport(
                        mActivityTestRule
                                .getActivity()
                                .getRootUiCoordinatorForTesting()
                                .getBottomSheetController());
        mEphemeralTabObserver = new TestEphemeralTabObserver();
    }

    /**
     * End all animations that already started before so that the UI will be in a state ready for
     * the next command.
     */
    private void endAnimations() {
        ThreadUtils.runOnUiThreadBlocking(mSheetTestSupport::endAllAnimations);
    }

    private void closePreviewTab() {
        ThreadUtils.runOnUiThreadBlocking(mEphemeralTabCoordinator::close);
        endAnimations();
        Assert.assertFalse(
                "The Preview Tab should have closed but did not indicate closed",
                mEphemeralTabCoordinator.isOpened());
    }

    /**
     * Test bringing up the PT, scrolling the base page but never expanding the PT, then closing it.
     */
    @Test
    @MediumTest
    @Feature({"PreviewTab"})
    @DisabledTest(message = "b/337124281")
    public void testOpenAndClose() throws Throwable {
        Assert.assertFalse(
                "Test should have started without any Preview Tab",
                mEphemeralTabCoordinator.isOpened());

        ChromeActivity activity = mActivityTestRule.getActivity();
        Tab tab = activity.getActivityTab();
        ContextMenuUtils.selectContextMenuItem(
                InstrumentationRegistry.getInstrumentation(),
                activity,
                tab,
                PREVIEW_TAB_DOM_ID,
                R.id.contextmenu_open_in_ephemeral_tab);
        endAnimations();
        Assert.assertTrue("The Preview Tab did not open", mEphemeralTabCoordinator.isOpened());

        // Scroll the base page.
        DOMUtils.scrollNodeIntoView(tab.getWebContents(), NEAR_BOTTOM_DOM_ID);
        endAnimations();
        Assert.assertTrue(
                "The Preview Tab did not stay open after a scroll action",
                mEphemeralTabCoordinator.isOpened());

        closePreviewTab();
    }

    /**
     * Test that closing all incognito tabs successfully handles the base tab and its preview tab
     * opened in incognito mode. This makes sure an incognito profile shared by the tabs is
     * destroyed safely.
     */
    @Test
    @MediumTest
    @Feature({"PreviewTab"})
    @DisabledTest(message = "b/337124281")
    public void testCloseAllIncognitoTabsClosesPreviewTab() throws Throwable {
        Assert.assertFalse(
                "Test should have started without any Preview Tab",
                mEphemeralTabCoordinator.isOpened());

        mActivityTestRule.loadUrlInNewTab(
                mTestServer.getServer().getURL(BASE_PAGE), /* incognito= */ true);
        mActivityTestRule.getActivity().getTabModelSelector().selectModel(true);
        ChromeActivity activity = mActivityTestRule.getActivity();
        Tab tab = activity.getActivityTab();
        Assert.assertTrue(tab.isIncognito());

        ContextMenuUtils.selectContextMenuItem(
                InstrumentationRegistry.getInstrumentation(),
                activity,
                tab,
                PREVIEW_TAB_DOM_ID,
                R.id.contextmenu_open_in_ephemeral_tab);
        endAnimations();
        BottomSheetController bottomSheet =
                activity.getRootUiCoordinatorForTesting().getBottomSheetController();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    bottomSheet.expandSheet();
                    endAnimations();
                    IncognitoTabHostUtils.closeAllIncognitoTabs();
                    endAnimations();
                });
        Assert.assertEquals(SheetState.HIDDEN, bottomSheet.getSheetState());
    }

    /** Test preview tab suppresses contextual search. */
    @Test
    @MediumTest
    @Feature({"PreviewTab"})
    public void testSuppressContextualSearch() throws Throwable {
        ChromeActivity activity = mActivityTestRule.getActivity();
        ContextualSearchManager csManager = activity.getContextualSearchManagerForTesting();
        Assert.assertFalse("Contextual Search should be active", csManager.isSuppressed());

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mEphemeralTabCoordinator.requestOpenSheet(
                                new GURL(mTestServer.getServer().getURL(PREVIEW_TAB)),
                                "PreviewTab",
                                mActivityTestRule.getProfile(false)));
        endAnimations();
        Assert.assertTrue("The Preview Tab did not open", mEphemeralTabCoordinator.isOpened());
        Assert.assertTrue("Contextual Search should be suppressed", csManager.isSuppressed());

        closePreviewTab();
        Assert.assertFalse("Contextual Search should be active", csManager.isSuppressed());
    }

    /** Test that the observer methods are being notified on events. */
    @Test
    @MediumTest
    @Feature({"PreviewTab"})
    @DisabledTest(message = "https://crbug.com/1412050")
    public void testObserverMethods() throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mEphemeralTabCoordinator.addObserver(mEphemeralTabObserver));

        // Open Preview Tab.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mEphemeralTabCoordinator.requestOpenSheetWithFullPageUrl(
                                new GURL(mTestServer.getServer().getURL(PREVIEW_TAB)),
                                null,
                                "PreviewTab",
                                mActivityTestRule.getProfile(false)));
        endAnimations();

        mEphemeralTabObserver.onToolbarCreatedCallback.waitForCallback(0, 1);
        mEphemeralTabObserver.onNavigationStartedCallback.waitForCallback(0, 1);
        mEphemeralTabObserver.onTitleSetCallback.waitForCallback(0, 1);

        // Navigate to another page in preview tab.
        DOMUtils.clickNode(
                mEphemeralTabCoordinator.getWebContentsForTesting(), ANOTHER_PAGE_DOM_ID);
        endAnimations();

        mEphemeralTabObserver.onNavigationStartedCallback.waitForCallback(1, 1);
        mEphemeralTabObserver.onTitleSetCallback.waitForCallback(1, 1);
        Assert.assertEquals(1, mEphemeralTabObserver.onToolbarCreatedCallback.getCallCount());

        closePreviewTab();
    }
}
