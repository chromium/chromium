// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.GONE;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.VISIBLE;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.IdRes;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.TabStripUtils;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** Tests for the {@link TabSwitcher} on tablet */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "force-fieldtrials=Study/Group"
})
@Restriction({Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE, DeviceFormFactor.TABLET})
@Batch(Batch.PER_CLASS)
public class TabSwitcherTabletTest {
    private static final long CALLBACK_WAIT_TIMEOUT = 15L;

    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Before
    public void setUp() throws ExecutionException {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(cta.getTabModelSelectorSupplier().get()::isTabStateInitialized);
    }

    @After
    public void cleanup() throws TimeoutException {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();

        LayoutManagerChrome layoutManager = cta.getLayoutManager();
        if (layoutManager.isLayoutVisible(LayoutType.TAB_SWITCHER)
                && !layoutManager.isLayoutStartingToHide(LayoutType.TAB_SWITCHER)) {
            TabModelSelector selector = cta.getTabModelSelectorSupplier().get();
            if (selector.getModel(false).getCount() == 0) {
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            TabCreator tabCreator = cta.getTabCreator(/* incognito= */ false);
                            return tabCreator.createNewTab(
                                    new LoadUrlParams("about:blank"),
                                    TabLaunchType.FROM_CHROME_UI,
                                    null,
                                    0);
                        });
                LayoutTestUtils.waitForLayout(layoutManager, LayoutType.BROWSING);
            } else {
                if (selector.getCurrentModel().isIncognito()) {
                    clickIncognitoToggleButton();
                }
                exitSwitcherWithTabClick(0);
            }
        } else {
            LayoutTestUtils.waitForLayout(layoutManager, LayoutType.BROWSING);
        }
    }

    @Test
    @MediumTest
    @RequiresRestart
    @DisabledTest(message = "Flaky, see crbug.com/327457591")
    public void testEnterAndExitTabSwitcher() throws TimeoutException {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        checkHubLayout(cta, false);
        checkTabSwitcherViewHolderStub(cta, true);
        checkTabSwitcherViewHolder(cta, false);

        TabUiTestHelper.prepareTabsWithThumbnail(sActivityTestRule, 1, 0, null);
        TabUiTestHelper.enterTabSwitcher(cta);
        ensureHubLayout();

        checkHubLayout(cta, true);
        checkTabSwitcherViewHolderStub(cta, false);
        checkTabSwitcherViewHolder(cta, true);

        exitSwitcherWithTabClick(0);
        assertFalse(cta.getLayoutManager().isLayoutVisible(LayoutType.TAB_SWITCHER));
    }

    @Test
    @MediumTest
    public void testToggleIncognitoSwitcher() throws Exception {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        prepareTabs(1, 1);
        TabUiTestHelper.enterTabSwitcher(cta);

        // Start with incognito switcher.
        assertTrue("Expected to be in Incognito model", cta.getCurrentTabModel().isIncognito());

        // Toggle to normal switcher.
        clickIncognitoToggleButton();

        final Tab newTab = cta.getCurrentTabModel().getTabAt(0);
        assertFalse(newTab.isIncognito());

        exitSwitcherWithTabClick(0);
    }

    @Test
    @MediumTest
    public void testTabSwitcherScrim() throws TimeoutException {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        prepareTabs(1, 1);
        TabUiTestHelper.enterTabSwitcher(cta);

        ScrimCoordinator scrimCoordinator =
                cta.getRootUiCoordinatorForTesting().getScrimCoordinator();
        assertTrue(scrimCoordinator.isShowingScrim());
        assertEquals(
                ChromeColors.getPrimaryBackgroundColor(cta, true),
                cta.getRootUiCoordinatorForTesting()
                        .getStatusBarColorController()
                        .getScrimColorForTesting());

        exitSwitcherWithTabClick(0);
        assertFalse(scrimCoordinator.isShowingScrim());
    }

    @Test
    @MediumTest
    public void testGridTabSwitcherOnNoNextTab_WithoutRestart() throws ExecutionException {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        ensureHubLayout();

        checkHubLayout(cta, /* isInitialized= */ true);
        checkTabSwitcherViewHolderStub(cta, /* exists= */ false);
        checkTabSwitcherViewHolder(cta, /* exists= */ true);

        // Assert the grid tab switcher is not yet showing.
        checkTabSwitcherViewHolderVisibility(cta, false);

        // Close the only tab through the tab strip.
        closeTab(false, sActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0).getId());

        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.TAB_SWITCHER);

        // Assert the grid tab switcher is shown automatically, since there is no next tab.
        checkTabSwitcherViewHolderVisibility(cta, true);
    }

    @Test
    @MediumTest
    @RequiresRestart
    @DisabledTest(message = "crbug.com/342983248")
    public void testGridTabSwitcherOnNoNextTab_WithRestart() throws ExecutionException {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        checkHubLayout(cta, /* isInitialized= */ false);
        checkTabSwitcherViewHolderStub(cta, /* exists= */ true);
        checkTabSwitcherViewHolder(cta, /* exists= */ false);

        // Close the only tab through the tab strip.
        closeTab(false, sActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0).getId());

        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.TAB_SWITCHER);

        // Assert the grid tab switcher is shown automatically, since there is no next tab.
        checkTabSwitcherViewHolderVisibility(cta, true);

        checkHubLayout(cta, /* isInitialized= */ true);
        checkTabSwitcherViewHolderStub(cta, /* exists= */ false);
        checkTabSwitcherViewHolder(cta, /* exists= */ true);
    }

    @Test
    @MediumTest
    public void testGridTabSwitcherOnCloseAllTabs_WithoutRestart() throws ExecutionException {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        ensureHubLayout();

        checkHubLayout(cta, /* isInitialized= */ true);
        checkTabSwitcherViewHolderStub(cta, /* exists= */ false);
        checkTabSwitcherViewHolder(cta, /* exists= */ true);

        // Assert the grid tab switcher is not yet showing.
        checkTabSwitcherViewHolderVisibility(cta, false);

        // Close all tabs.
        ChromeTabUtils.closeAllTabs(
                InstrumentationRegistry.getInstrumentation(), cta.getTabModelSelectorSupplier());

        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.TAB_SWITCHER);

        // Assert the grid tab switcher is shown automatically, since there is no next tab.
        checkTabSwitcherViewHolderVisibility(cta, true);
    }

    @Test
    @MediumTest
    public void testGridTabSwitcherToggleIncognitoWithNoRegularTab() throws ExecutionException {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        TabModel regularModel = cta.getTabModelSelectorSupplier().get().getModel(false);

        // Open an incognito tab.
        prepareTabs(1, 1);

        // Close all the regular tabs.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    regularModel.closeTabs(
                            TabClosureParams.closeTab(regularModel.getTabAt(0))
                                    .allowUndo(false)
                                    .build());
                });
        assertEquals("Expected to be 0 tabs in regular model", 0, regularModel.getCount());
        assertTrue("Expected to be in Incognito model", cta.getCurrentTabModel().isIncognito());

        // Assert the grid tab switcher is not yet showing.
        checkTabSwitcherViewHolderVisibility(cta, false);

        TabUiTestHelper.enterTabSwitcher(cta);

        // Toggle to normal switcher.
        clickIncognitoToggleButton();

        checkTabSwitcherViewHolderVisibility(cta, true);
    }

    // Regression test for crbug.com/1487114.
    @Test
    @MediumTest
    @RequiresRestart
    public void testGridTabSwitcher_DeferredHubLayoutCreation() throws ExecutionException {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        prepareTabs(2, 0);
        // Verifies that the dialog visibility supplier doesn't crash when closing a Tab without the
        // grid tab switcher is inflated.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    cta.getCurrentTabModel()
                            .closeTabs(
                                    TabClosureParams.closeTab(cta.getActivityTab())
                                            .allowUndo(false)
                                            .build());
                });

        checkHubLayout(cta, /* isInitialized= */ false);
        checkTabSwitcherViewHolderStub(cta, /* exists= */ true);
        checkTabSwitcherViewHolder(cta, /* exists= */ false);

        // Click tab switcher button
        TabUiTestHelper.enterTabSwitcher(sActivityTestRule.getActivity());

        checkHubLayout(cta, /* isInitialized= */ true);
        checkTabSwitcherViewHolderStub(cta, /* exists= */ false);
        checkTabSwitcherViewHolder(cta, /* exists= */ true);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/342983248")
    public void testEmptyStateView() throws Exception {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        prepareTabs(1, 0);
        TabUiTestHelper.enterTabSwitcher(cta);

        // Close the last tab.
        closeTab(false, cta.getCurrentTabModel().getTabAt(0).getId());

        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.TAB_SWITCHER);

        // Check whether empty view show up.
        @IdRes int tabSwitcherAncestorId = TabUiTestHelper.getTabSwitcherAncestorId(cta);
        onView(
                        allOf(
                                withId(R.id.empty_state_container),
                                isDescendantOfA(withId(tabSwitcherAncestorId))))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testEmptyStateView_ToggleIncognito() throws Exception {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        prepareTabs(1, 1);
        TabUiTestHelper.enterTabSwitcher(sActivityTestRule.getActivity());

        // Close the last normal tab.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel model = cta.getTabModelSelector().getModel(false);
                    model.closeTabs(
                            TabClosureParams.closeTab(model.getTabAt(0)).allowUndo(false).build());
                });

        // Check empty view should never show up in incognito tab switcher.
        @IdRes int tabSwitcherAncestorId = TabUiTestHelper.getTabSwitcherAncestorId(cta);
        onView(
                        allOf(
                                withId(R.id.empty_state_container),
                                isDescendantOfA(withId(tabSwitcherAncestorId))))
                .check(doesNotExist());

        // Close the last incognito tab.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel model = cta.getTabModelSelector().getModel(true);
                    model.closeTabs(
                            TabClosureParams.closeTab(model.getTabAt(0)).allowUndo(false).build());
                });

        // Incognito tab switcher should exit to go to normal tab switcher and we should see empty
        // view.
        onView(
                        allOf(
                                withId(R.id.empty_state_container),
                                isDescendantOfA(withId(tabSwitcherAncestorId))))
                .check(matches(isDisplayed()));
    }

    protected void clickIncognitoToggleButton() {
        final CallbackHelper tabModelSelectedCallback = new CallbackHelper();
        Callback<TabModel> observer =
                (tabModel) -> {
                    tabModelSelectedCallback.notifyCalled();
                };
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        sActivityTestRule
                                .getActivity()
                                .getTabModelSelectorSupplier()
                                .get()
                                .getCurrentTabModelSupplier()
                                .addObserver(observer));
        StripLayoutHelperManager manager =
                TabStripUtils.getStripLayoutHelperManager(sActivityTestRule.getActivity());
        TabStripUtils.clickCompositorButton(
                manager.getModelSelectorButton(),
                InstrumentationRegistry.getInstrumentation(),
                sActivityTestRule.getActivity());
        try {
            tabModelSelectedCallback.waitForCallback(0);
        } catch (TimeoutException e) {
            throw new AssertionError("Tab model selected event never occurred.", e);
        }
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivityTestRule
                            .getActivity()
                            .getTabModelSelector()
                            .getCurrentTabModelSupplier()
                            .removeObserver(observer);
                });
    }

    private void prepareTabs(int numTabs, int numIncognitoTabs) {
        TabUiTestHelper.createTabs(sActivityTestRule.getActivity(), false, numTabs);
        TabUiTestHelper.createTabs(sActivityTestRule.getActivity(), true, numIncognitoTabs);
    }

    private void exitSwitcherWithTabClick(int index) throws TimeoutException {
        TabUiTestHelper.clickNthCardFromTabSwitcher(sActivityTestRule.getActivity(), index);
        LayoutTestUtils.waitForLayout(
                sActivityTestRule.getActivity().getLayoutManager(), LayoutType.BROWSING);
    }

    private void closeTab(final boolean incognito, final int id) {
        ChromeTabUtils.closeTabWithAction(
                InstrumentationRegistry.getInstrumentation(),
                sActivityTestRule.getActivity(),
                () -> {
                    StripLayoutTab tab =
                            TabStripUtils.findStripLayoutTab(
                                    sActivityTestRule.getActivity(), incognito, id);
                    TabStripUtils.clickCompositorButton(
                            tab.getCloseButton(),
                            InstrumentationRegistry.getInstrumentation(),
                            sActivityTestRule.getActivity());
                });
    }

    private void ensureHubLayout() {
        LayoutManagerChrome layoutManager = sActivityTestRule.getActivity().getLayoutManager();
        Layout tabSwitcherLayout = layoutManager.getHubLayoutForTesting();
        if (tabSwitcherLayout == null) {
            ThreadUtils.runOnUiThreadBlocking(layoutManager::initHubLayoutForTesting);
            tabSwitcherLayout = layoutManager.getHubLayoutForTesting();
        }
        assertNotNull(tabSwitcherLayout);
    }

    private void checkHubLayout(ChromeTabbedActivity cta, boolean isInitialized) {
        Layout layout = cta.getLayoutManager().getHubLayoutForTesting();
        if (isInitialized) {
            assertNotNull("HubLayout should be initialized", layout);
        } else {
            assertNull("HubLayout should not be initialized", layout);
        }
    }

    private void checkTabSwitcherViewHolderStub(ChromeTabbedActivity cta, boolean exists) {
        ViewStub tabSwitcherStub = cta.findViewById(R.id.tab_switcher_view_holder_stub);
        if (exists) {
            assertTrue(
                    "TabSwitcher view stub should not be inflated",
                    tabSwitcherStub != null && tabSwitcherStub.getParent() != null);
        } else {
            assertTrue(
                    "TabSwitcher view stub should have been inflated",
                    tabSwitcherStub == null || tabSwitcherStub.getParent() == null);
        }
    }

    private void checkTabSwitcherViewHolder(ChromeTabbedActivity cta, boolean exists) {
        ViewGroup tabSwitcherViewHolder = cta.findViewById(R.id.tab_switcher_view_holder);
        if (exists) {
            assertNotNull("TabSwitcher view should be inflated", tabSwitcherViewHolder);
        } else {
            assertNull("TabSwitcher view should not be inflated", tabSwitcherViewHolder);
        }
    }

    private void checkTabSwitcherViewHolderVisibility(ChromeTabbedActivity cta, boolean visible) {
        @IdRes int tabSwitcherId = R.id.tab_switcher_view_holder;
        if (visible) {
            onView(withId(tabSwitcherId)).check(matches(withEffectiveVisibility(VISIBLE)));
        } else {
            onView(withId(tabSwitcherId)).check(matches(withEffectiveVisibility(GONE)));
        }
    }
}
