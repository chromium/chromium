// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.TAB_TO_GTS_ANIMATION;

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
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.SysUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

/** End-to-end test for TabSelectionEditorCoordinator with closable item type. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DisableFeatures(TAB_TO_GTS_ANIMATION)
@Batch(Batch.PER_CLASS)
public class ClosableTabListEditorTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Mock private Callback<RecyclerViewPosition> mSetRecyclerViewPosition;

    private TabSelectionEditorTestingRobot mRobot = new TabSelectionEditorTestingRobot();

    private TabModelSelector mTabModelSelector;
    private TabSelectionEditorCoordinator.TabSelectionEditorController
            mTabSelectionEditorController;
    private TabSelectionEditorLayout mTabSelectionEditorLayout;
    private TabSelectionEditorCoordinator mTabSelectionEditorCoordinator;
    private WeakReference<TabSelectionEditorLayout> mRef;

    private ViewGroup mParentView;
    private SnackbarManager mSnackbarManager;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mTabModelSelector = sActivityTestRule.getActivity().getTabModelSelector();
        mParentView = (ViewGroup) sActivityTestRule.getActivity().findViewById(R.id.coordinator);
        mSnackbarManager = sActivityTestRule.getActivity().getSnackbarManager();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabSelectionEditorCoordinator =
                            new TabSelectionEditorCoordinator(
                                    sActivityTestRule.getActivity(),
                                    mParentView,
                                    sActivityTestRule.getActivity().getBrowserControlsManager(),
                                    mTabModelSelector,
                                    sActivityTestRule.getActivity().getTabContentManager(),
                                    mSetRecyclerViewPosition,
                                    getMode(),
                                    sActivityTestRule
                                            .getActivity()
                                            .getCompositorViewHolderForTesting(),
                                    /* displayGroups= */ true,
                                    mSnackbarManager,
                                    TabProperties.UiType.CLOSABLE);

                    mTabSelectionEditorController = mTabSelectionEditorCoordinator.getController();
                    mTabSelectionEditorLayout =
                            mTabSelectionEditorCoordinator.getTabSelectionEditorLayoutForTesting();
                    mRef = new WeakReference<>(mTabSelectionEditorLayout);
                });
    }

    @After
    public void tearDown() {
        if (mTabSelectionEditorCoordinator != null) {
            if (sActivityTestRule.getActivity().findViewById(R.id.app_menu_list) != null) {
                Espresso.pressBack();
            }

            TestThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        if (mTabSelectionEditorController.isVisible()) {
                            mTabSelectionEditorController.hide();
                        }
                        mTabSelectionEditorCoordinator.destroy();
                    });

            if (sActivityTestRule
                    .getActivity()
                    .getLayoutManager()
                    .isLayoutVisible(LayoutType.TAB_SWITCHER)) {
                TabUiTestHelper.leaveTabSwitcher(sActivityTestRule.getActivity());
            }
        }
        TestThreadUtils.runOnUiThreadBlocking(
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
        mRobot.resultRobot.verifyTabSelectionEditorIsHidden();
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
        mRobot.resultRobot.verifyTabSelectionEditorIsVisible();
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
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabSelectionEditorController.show(
                            tabs, /* preSelectedTabCount= */ 0, /* recyclerViewPosition= */ null);
                });
    }
}
