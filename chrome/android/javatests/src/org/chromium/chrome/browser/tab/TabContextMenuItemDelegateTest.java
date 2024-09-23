// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.common.Referrer;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

/** Integration tests for {@link TabContextMenuItemDelegate}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_ANDROID)
public class TabContextMenuItemDelegateTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mContextMenuCopyLinkObserver;
    private ModalDialogManager mModalDialogManager;
    private TabContextMenuItemDelegate mContextMenuDelegate;

    @Before
    public void setUp() {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(cta.getTabModelSelectorSupplier().get()::isTabStateInitialized);

        mModalDialogManager = cta.getModalDialogManager();
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
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_PARITY_ANDROID,
        ChromeFeatureList.TAB_GROUP_CREATION_DIALOG_ANDROID
    })
    public void testOpenInNewTabInGroup_NewGroup_ParityEnabled_ContextMenuDialogDisabled() {
        openNewTabUsingContextMenu();

        assertFalse(mModalDialogManager.isShowing());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_ANDROID)
    public void testOpenInNewTabInGroup_ExistingGroup_ParityEnabled() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeTabbedActivity cta = sActivityTestRule.getActivity();
                    var tabModelSelector = cta.getTabModelSelectorSupplier().get();
                    var filter =
                            (TabGroupModelFilter)
                                    tabModelSelector
                                            .getTabModelFilterProvider()
                                            .getTabModelFilter(false);
                    var tab = cta.getActivityTab();
                    filter.createSingleTabGroup(tab, /* notify= */ false);
                });

        openNewTabUsingContextMenu();

        assertFalse(mModalDialogManager.isShowing());
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_ANDROID)
    public void testOpenInNewTabInGroup_NewGroup_ParityDisabled() {
        openNewTabUsingContextMenu();

        assertFalse(mModalDialogManager.isShowing());
    }

    private void createContextMenuForCurrentTab() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeTabbedActivity cta = sActivityTestRule.getActivity();
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
                                    tab,
                                    tabModelSelector,
                                    ephemeralTabCoordinatorSupplier,
                                    mContextMenuCopyLinkObserver,
                                    snackbarManagerSupplier,
                                    bottomSheetControllerSupplier,
                                    () -> mModalDialogManager);
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
