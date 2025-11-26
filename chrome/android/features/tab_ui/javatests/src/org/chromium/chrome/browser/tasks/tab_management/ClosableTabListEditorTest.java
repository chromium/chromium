// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.test.util.ChromeTabUtils.getTabCountOnUiThread;

import android.view.ViewGroup;

import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.RecyclerViewPosition;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.CreationMode;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

/** End-to-end test for closable TabListEditor. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class ClosableTabListEditorTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

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
    private WebPageStation mInitialPage;

    @Before
    public void setUp() throws Exception {
        mInitialPage = mActivityTestRule.startOnBlankPage();
        mTabModelSelector = mActivityTestRule.getActivity().getTabModelSelector();
        mParentView = (ViewGroup) mActivityTestRule.getActivity().findViewById(R.id.coordinator);
        mSnackbarManager = mActivityTestRule.getActivity().getSnackbarManager();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var currentTabGroupModelFilterSupplier =
                            mTabModelSelector
                                    .getTabGroupModelFilterProvider()
                                    .getCurrentTabGroupModelFilterSupplier();
                    mEdgeToEdgeSupplier = new ObservableSupplierImpl<>();
                    mTabListEditorCoordinator =
                            new TabListEditorCoordinator(
                                    mActivityTestRule.getActivity(),
                                    mActivityTestRule
                                            .getActivity()
                                            .getCompositorViewHolderForTesting(),
                                    mParentView,
                                    mActivityTestRule.getActivity().getBrowserControlsManager(),
                                    currentTabGroupModelFilterSupplier,
                                    mActivityTestRule.getActivity().getTabContentManager(),
                                    mSetRecyclerViewPosition,
                                    TabListCoordinator.TabListMode.GRID,
                                    /* displayGroups= */ true,
                                    mSnackbarManager,
                                    /* bottomSheetController= */ null,
                                    TabProperties.TabActionState.CLOSABLE,
                                    /* gridCardOnClickListenerProvider= */ null,
                                    mModalDialogManager,
                                    /* desktopWindowStateManager= */ null,
                                    mEdgeToEdgeSupplier,
                                    CreationMode.FULL_SCREEN,
                                    /* undoBarExplicitTrigger= */ null,
                                    /* componentName= */ null,
                                    TabListEditorCoordinator.UNLIMITED_SELECTION);

                    mTabListEditorController = mTabListEditorCoordinator.getController();
                    mTabListEditorLayout =
                            mTabListEditorCoordinator.getTabListEditorLayoutForTesting();
                    mRef = new WeakReference<>(mTabListEditorLayout);
                });
    }

    @After
    public void tearDown() {
        if (mTabListEditorCoordinator != null) {
            if (mActivityTestRule.getActivity().findViewById(R.id.app_menu_list) != null) {
                Espresso.pressBack();
            }

            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        if (mTabListEditorController.isVisible()) {
                            mTabListEditorController.hide();
                        }
                        mTabListEditorCoordinator.destroy();
                    });

            if (mActivityTestRule
                    .getActivity()
                    .getLayoutManager()
                    .isLayoutVisible(LayoutType.TAB_SWITCHER)) {
                TabUiTestHelper.leaveTabSwitcher(mActivityTestRule.getActivity());
            }
        }
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSnackbarManager.dismissAllSnackbars();
                });
    }

    private void prepareBlankTab(int num, boolean isIncognito) {
        for (int i = 0; i < num - 1; i++) {
            ChromeTabUtils.newTabFromMenu(
                    InstrumentationRegistry.getInstrumentation(),
                    mActivityTestRule.getActivity(),
                    isIncognito,
                    true);
            mActivityTestRule.loadUrl("about:blank");
        }
    }

    @Test
    @MediumTest
    public void testClosableTabListEditor_openTab() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
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
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
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
                    mTabListEditorController.show(
                            tabs, new ArrayList<>(), /* recyclerViewPosition= */ null);
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
                    mTabListEditorController.show(
                            tabs, new ArrayList<>(), /* recyclerViewPosition= */ null);
                    mTabListEditorController.setNavigationProvider(mNavigationProvider);
                    mTabListEditorController.handleBackPress();
                });

        Mockito.verify(mNavigationProvider).goBack();
    }

    /** Retrieves all tabs from the current tab model */
    private List<Tab> getTabsInCurrentTabModel() {
        List<Tab> tabs = new ArrayList<>();

        TabModel currentTabModel = mTabModelSelector.getCurrentModel();
        for (int i = 0; i < getTabCountOnUiThread(currentTabModel); i++) {
            int j = i;
            tabs.add(ThreadUtils.runOnUiThreadBlocking(() -> currentTabModel.getTabAt(j)));
        }

        return tabs;
    }

    private void showTabListEditor(List<Tab> tabs) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabListEditorController.show(
                            tabs, new ArrayList<>(), /* recyclerViewPosition= */ null);
                });
    }
}
