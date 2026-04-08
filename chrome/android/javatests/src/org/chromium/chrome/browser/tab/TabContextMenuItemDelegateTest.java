// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.os.Build;

import androidx.test.filters.SmallTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.CloseWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.common.Referrer;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.function.Supplier;

/** Integration tests for {@link TabContextMenuItemDelegate}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "This class runs tests that create new activities.")
public class TabContextMenuItemDelegateTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mContextMenuCopyLinkObserver;
    private ModalDialogManager mModalDialogManager;
    private TabContextMenuItemDelegate mContextMenuDelegate;
    private List<ChromeTabbedActivity> mExtraTabbedActivities;

    @Before
    public void setUp() {
        ChromeTabbedActivity cta = mActivityTestRule.startOnBlankPage().getActivity();
        CriteriaHelper.pollUiThread(cta.getTabModelSelectorSupplier().get()::isTabStateInitialized);

        mModalDialogManager = cta.getModalDialogManager();
        mExtraTabbedActivities = new ArrayList<>();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModalDialogManager.dismissAllDialogs(DialogDismissalCause.ACTIVITY_DESTROYED);
                    // Cleanup extra activities that were created.
                    for (ChromeTabbedActivity activity : mExtraTabbedActivities) {
                        var multiInstanceManager = activity.getMultiInstanceMangerForTesting();
                        multiInstanceManager.closeWindows(
                                Collections.singletonList(activity.getWindowId()),
                                CloseWindowAppSource.OTHER);
                    }
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
    @DisableIf.Device(DeviceFormFactor.DESKTOP) // crbug.com/497724495
    public void testOpenInNewTabInGroup_ExistingGroup_ParityEnabled() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeTabbedActivity cta = mActivityTestRule.getActivity();
                    var tabModelSelector = cta.getTabModelSelectorSupplier().get();
                    var tabModel = tabModelSelector.getModel(false);
                    var tab = cta.getActivityTab();
                    tabModel.createSingleTabGroup(tab);
                });

        openNewTabUsingContextMenu();

        assertFalse(mModalDialogManager.isShowing());
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.S)
    @Restriction({
        DeviceFormFactor.TABLET_OR_DESKTOP,
        DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
        DeviceRestriction.RESTRICTION_TYPE_NON_FOLDABLE
    })
    public void testOpenInOtherWindow_ExistingWindow_ShowsDialog() {
        createContextMenuForCurrentTab();

        // Open a new window when there is only one existing window.
        ChromeTabbedActivity secondActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.RESUMED,
                        () ->
                                mContextMenuDelegate.openInOtherWindow(
                                        new GURL("about:blank"),
                                        new Referrer("about:blank", 0),
                                        /* isIncognito= */ false,
                                        /* preferNew= */ false));
        mExtraTabbedActivities.add(secondActivity);

        // Don't show instance picker dialog when there is only one other window.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContextMenuDelegate.openInOtherWindow(
                            new GURL("about:blank"),
                            new Referrer("about:blank", 0),
                            /* isIncognito= */ false,
                            /* preferNew= */ false);
                });
        assertFalse(
                "Dialog should not be visible when there is only one other window.",
                mModalDialogManager.isShowing());

        // Create a third window. The instance picker dialog should be shown when there are at least
        // two other windows.
        ChromeTabbedActivity thirdActivity =
                MultiWindowTestHelper.createNewChromeTabbedActivity(
                        mActivityTestRule.getActivity());
        mExtraTabbedActivities.add(thirdActivity);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContextMenuDelegate.openInOtherWindow(
                            new GURL("about:blank"),
                            new Referrer("about:blank", 0),
                            /* isIncognito= */ false,
                            /* preferNew= */ false);
                });
        assertTrue(
                "Dialog should be visible when there are at least two other windows.",
                mModalDialogManager.isShowing());
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.S)
    @Restriction({
        DeviceFormFactor.TABLET_OR_DESKTOP,
        DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
        DeviceRestriction.RESTRICTION_TYPE_NON_FOLDABLE
    })
    public void testOpenInOtherWindow_PreferNew_CreatesNewWindow() {
        createContextMenuForCurrentTab();

        ChromeTabbedActivity activity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.RESUMED,
                        () ->
                                mContextMenuDelegate.openInOtherWindow(
                                        new GURL("about:blank"),
                                        new Referrer("about:blank", 0),
                                        /* isIncognito= */ false,
                                        /* preferNew= */ true));
        mExtraTabbedActivities.add(activity);
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
                                    bottomSheetControllerSupplier);
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
