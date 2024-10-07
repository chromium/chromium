// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.ViewGroup;

import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.SysUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.RecyclerViewPosition;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

/** End-to-end test for closable TabListEditor. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class ClosableTabListEditorTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Mock private Callback<RecyclerViewPosition> mSetRecyclerViewPosition;
    @Mock private TabListEditorCoordinator.NavigationProvider mNavigationProvider;
    @Mock private ModalDialogManager mModalDialogManager;

    private final TabListEditorTestingRobot mRobot = new TabListEditorTestingRobot();

    private TabModelSelector mTabModelSelector;
    private TabListEditorCoordinator.TabListEditorController mTabListEditorController;
    private TabListEditorLayout mTabListEditorLayout;
    private TabListEditorCoordinator mTabListEditorCoordinator;
    private WeakReference<TabListEditorLayout> mRef;
    private ObservableSupplierImpl<EdgeToEdgeController> mEdgeToEdgeSupplier;

    private ViewGroup mParentView;
    private SnackbarManager mSnackbarManager;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mTabModelSelector = sActivityTestRule.getActivity().getTabModelSelector();
        mParentView = (ViewGroup) sActivityTestRule.getActivity().findViewById(R.id.coordinator);
        mSnackbarManager = sActivityTestRule.getActivity().getSnackbarManager();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var currentTabModelFilterSupplier =
                            mTabModelSelector
                                    .getTabModelFilterProvider()
                                    .getCurrentTabModelFilterSupplier();
                    mEdgeToEdgeSupplier = new ObservableSupplierImpl<>();
                    mTabListEditorCoordinator =
                            new TabListEditorCoordinator(
                                    sActivityTestRule.getActivity(),
                                    sActivityTestRule
                                            .getActivity()
                                            .getCompositorViewHolderForTesting(),
                                    mParentView,
                                    sActivityTestRule.getActivity().getBrowserControlsManager(),
                                    currentTabModelFilterSupplier,
                                    sActivityTestRule.getActivity().getTabContentManager(),
                                    mSetRecyclerViewPosition,
                                    getMode(),
                                    /* displayGroups= */ true,
                                    mSnackbarManager,
                                    /* bottomSheetController= */ null,
                                    TabProperties.TabActionState.CLOSABLE,
                                    /* gridCardOnClickListenerProvider= */ null,
                                    mModalDialogManager,
                                    /* desktopWindowStateProvider= */ null,
                                    mEdgeToEdgeSupplier);

                    mTabListEditorController = mTabListEditorCoordinator.getController();
                    mTabListEditorLayout =
                            mTabListEditorCoordinator.getTabListEditorLayoutForTesting();
                    mRef = new WeakReference<>(mTabListEditorLayout);
                });
    }

    @After
    public void tearDown() {
        if (mTabListEditorCoordinator != null) {
            if (sActivityTestRule.getActivity().findViewById(R.id.app_menu_list) != null) {
                Espresso.pressBack();
            }

            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        if (mTabListEditorController.isVisible()) {
                            mTabListEditorController.hide();
                        }
                        mTabListEditorCoordinator.destroy();
                    });

            if (sActivityTestRule
                    .getActivity()
                    .getLayoutManager()
                    .isLayoutVisible(LayoutType.TAB_SWITCHER)) {
                TabUiTestHelper.leaveTabSwitcher(sActivityTestRule.getActivity());
            }
        }
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSnackbarManager.dismissAllSnackbars();
                });
    }

    private @TabListCoordinator.TabListMode int getMode() {
        return SysUtils.isLowEndDevice()
                ? TabListCoordinator.TabListMode.LIST
                : TabListCoordinator.TabListMode.GRID;
    }

    private void prepareBlankTab(int num, boolean isIncognito) {
        for (int i = 0; i < num - 1; i++) {
            ChromeTabUtils.newTabFromMenu(
                    InstrumentationRegistry.getInstrumentation(),
                    sActivityTestRule.getActivity(),
                    isIncognito,
                    true);
            sActivityTestRule.loadUrl("about:blank");
        }
    }

    @Test
    @MediumTest
    public void testClosableTabListEditor_openTab() {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();
        TabUiTestHelper.enterTabSwitcher(cta);

        showTabListEditor(tabs);
        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.resultRobot.verifyTabListEditorIsHidden();
    }

    @Test
    @MediumTest
    public void testClosableTabListEditor_closeTab() {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();
        TabUiTestHelper.enterTabSwitcher(cta);

        showTabListEditor(tabs);
        TabUiTestHelper.closeFirstTabInTabSwitcher(cta);
        TabUiTestHelper.verifyTabSwitcherCardCount(cta, 1);
        mRobot.resultRobot.verifyTabListEditorIsVisible();
    }

    @Test
    @MediumTest
    public void testCustomToolbarTitle() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabListEditorController.show(tabs, /* recyclerViewPosition= */ null);
                    mTabListEditorController.setToolbarTitle("testing");
                });

        mRobot.resultRobot.verifyTabListEditorIsVisible().verifyToolbarSelectionText("testing");
    }

    @Test
    @MediumTest
    public void testCustomNavigationProvider() {
        prepareBlankTab(2, false);
        List<Tab> tabs = getTabsInCurrentTabModel();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabListEditorController.show(tabs, /* recyclerViewPosition= */ null);
                    mTabListEditorController.setNavigationProvider(mNavigationProvider);
                    mTabListEditorController.handleBackPress();
                });

        Mockito.verify(mNavigationProvider).goBack();
    }

    /** Retrieves all tabs from the current tab model */
    private List<Tab> getTabsInCurrentTabModel() {
        List<Tab> tabs = new ArrayList<>();

        TabModel currentTabModel = mTabModelSelector.getCurrentModel();
        for (int i = 0; i < currentTabModel.getCount(); i++) {
            tabs.add(currentTabModel.getTabAt(i));
        }

        return tabs;
    }

    private void showTabListEditor(List<Tab> tabs) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabListEditorController.show(tabs, /* recyclerViewPosition= */ null);
                });
    }
}
