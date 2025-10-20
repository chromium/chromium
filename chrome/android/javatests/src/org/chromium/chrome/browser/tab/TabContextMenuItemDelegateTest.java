// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.os.Build;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.common.Referrer;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

import java.util.function.Supplier;

/** Integration tests for {@link TabContextMenuItemDelegate}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class TabContextMenuItemDelegateTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mContextMenuCopyLinkObserver;
    private WebPageStation mInitialPage;
    private ModalDialogManager mModalDialogManager;
    private MultiInstanceManager mMultiInstanceManager;
    private TabContextMenuItemDelegate mContextMenuDelegate;

    @Before
    public void setUp() {
        mInitialPage = mActivityTestRule.startOnBlankPage();
        ChromeTabbedActivity cta = mInitialPage.getActivity();
        CriteriaHelper.pollUiThread(cta.getTabModelSelectorSupplier().get()::isTabStateInitialized);

        mModalDialogManager = cta.getModalDialogManager();
        mMultiInstanceManager = cta.getMultiInstanceMangerForTesting();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModalDialogManager.dismissAllDialogs(DialogDismissalCause.ACTIVITY_DESTROYED);
                });
    }

    @Test
    @SmallTest
    public void testOpenInNewTabInGroup_NewGroup_NoCreationDialog() {
        openNewTabUsingContextMenu();

        assertFalse(mModalDialogManager.isShowing());
    }

    @Test
    @SmallTest
    public void testOpenInNewTabInGroup_ExistingGroup_ParityEnabled() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeTabbedActivity cta = mActivityTestRule.getActivity();
                    var tabModelSelector = cta.getTabModelSelectorSupplier().get();
                    var filter =
                            tabModelSelector
                                    .getTabGroupModelFilterProvider()
                                    .getTabGroupModelFilter(false);
                    var tab = cta.getActivityTab();
                    filter.createSingleTabGroup(tab);
                });

        openNewTabUsingContextMenu();

        assertFalse(mModalDialogManager.isShowing());
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.S)
    @Features.EnableFeatures({ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW})
    public void testOpenInOtherWindow_ShowDialog_incognitoWindowingEnabled() {
        createContextMenuForCurrentTab();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContextMenuDelegate.openInOtherWindow(
                            new GURL("about:blank"),
                            new Referrer("about:blank", 0),
                            /* isIncognito= */ false);
                });
        assertFalse(
                "Window management dialog should not be visible with one window instance",
                mModalDialogManager.isShowing());

        MultiWindowUtils.setInstanceCountForTesting(/* instanceCount= */ 2);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContextMenuDelegate.openInOtherWindow(
                            new GURL("about:blank"),
                            new Referrer("about:blank", 0),
                            /* isIncognito= */ false);
                });
        assertTrue("Window management dialog should be visible", mModalDialogManager.isShowing());
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.S)
    @Features.DisableFeatures({ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW})
    public void testOpenInOtherWindow_incognitoWindowingDisabled() {
        createContextMenuForCurrentTab();

        MultiWindowUtils.setInstanceCountForTesting(/* instanceCount= */ 2);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContextMenuDelegate.openInOtherWindow(
                            new GURL("about:blank"),
                            new Referrer("about:blank", 0),
                            /* isIncognito= */ false);
                });
        assertFalse(
                "Window management dialog should not be visible regardless of instance"
                        + " count with flag disabled",
                mModalDialogManager.isShowing());
    }

    private void createContextMenuForCurrentTab() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeTabbedActivity cta = mActivityTestRule.getActivity();
                    var rootUiCoordinator = cta.getRootUiCoordinatorForTesting();
                    var tab = cta.getActivityTab();
                    var tabModelSelector = cta.getTabModelSelectorSupplier().get();
                    var ephemeralTabCoordinatorSupplier =
                            rootUiCoordinator.getEphemeralTabCoordinatorSupplier();
                    Supplier<SnackbarManager> snackbarManagerSupplier =
                            () -> cta.getSnackbarManager();
                    Supplier<BottomSheetController> bottomSheetControllerSupplier =
                            () -> rootUiCoordinator.getBottomSheetController();
                    mContextMenuDelegate =
                            new TabContextMenuItemDelegate(
                                    cta,
                                    ActivityType.TABBED,
                                    tab,
                                    tabModelSelector,
                                    ephemeralTabCoordinatorSupplier,
                                    mContextMenuCopyLinkObserver,
                                    snackbarManagerSupplier,
                                    bottomSheetControllerSupplier,
                                    mMultiInstanceManager);
                });
        assertNotNull(mContextMenuDelegate);
    }

    private void openNewTabUsingContextMenu() {
        createContextMenuForCurrentTab();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContextMenuDelegate.onOpenInNewTabInGroup(
                            new GURL("about:blank"), new Referrer("about:blank", 0));
                });
    }
}
