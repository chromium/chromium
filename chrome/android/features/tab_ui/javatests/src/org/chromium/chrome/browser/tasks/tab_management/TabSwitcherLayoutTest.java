// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.Espresso.closeSoftKeyboard;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.longClick;
import static androidx.test.espresso.action.ViewActions.pressImeActionButton;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.VISIBLE;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.core.AllOf.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.closeFirstTabGroupInTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.closeFirstTabInTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.createTabGroup;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.createTabs;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.getSwipeToDismissAction;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.mergeAllNormalTabsToAGroup;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.switchTabModel;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabModelTabCount;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabSwitcherCardCount;
import static org.chromium.components.embedder_support.util.UrlConstants.NTP_URL;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.Espresso;
import androidx.test.espresso.NoMatchingViewException;
import androidx.test.espresso.ViewAssertion;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.espresso.matcher.BoundedMatcher;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabThumbnailView;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupColorUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoBarController;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.widget.ViewLookupCachingFrameLayout;

import java.io.FileOutputStream;
import java.io.IOException;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.ExecutionException;

/** Tests for the {@link TabSwitcherLayout}. */
@SuppressWarnings("ConstantConditions")
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "force-fieldtrials=Study/Group"
})
@DisableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
@Restriction({DeviceFormFactor.PHONE, Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
public class TabSwitcherLayoutTest {
    private static final String TEST_URL = "/chrome/test/data/android/google.html";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @SuppressWarnings("FieldCanBeLocal")
    private EmbeddedTestServer mTestServer;

    private String mUrl;
    private int mRepeat;
    private List<WeakReference<Bitmap>> mAllBitmaps = new LinkedList<>();
    private Callback<Bitmap> mBitmapListener =
            (bitmap) -> mAllBitmaps.add(new WeakReference<>(bitmap));
    private ModalDialogManager mModalDialogManager;

    @Before
    public void setUp() throws ExecutionException {
        mTestServer = mActivityTestRule.getTestServer();

        // After setUp, Chrome is launched and has one NTP.
        mActivityTestRule.startMainActivityWithURL(NTP_URL);

        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        mUrl = mTestServer.getURL("/chrome/test/data/android/navigate/simple.html");
        mRepeat = 1;

        cta.getTabContentManager().setCaptureMinRequestTimeForTesting(0);

        CriteriaHelper.pollUiThread(cta.getTabModelSelector()::isTabStateInitialized);
        mModalDialogManager = ThreadUtils.runOnUiThreadBlocking(cta::getModalDialogManager);
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                ChromeNightModeTestUtils::tearDownNightModeAfterChromeActivityDestroyed);
        ThreadUtils.runOnUiThreadBlocking(
                () -> ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(null));
        dismissAllModalDialogs();
    }

    /**
     * Make Chrome have {@code numTabs} of regular Tabs and {@code numIncognitoTabs} of incognito
     * tabs with {@code url} loaded, and assert no bitmap fetching occurred.
     *
     * @param numTabs The number of regular tabs.
     * @param numIncognitoTabs The number of incognito tabs.
     * @param url The URL to load.
     */
    private void prepareTabs(int numTabs, int numIncognitoTabs, @Nullable String url) {
        TabUiTestHelper.prepareTabsWithThumbnail(mActivityTestRule, numTabs, numIncognitoTabs, url);
    }

    @Test
    @MediumTest
    public void testUrlUpdatedNotCrashing_ForUndoableClosedTab() throws Exception {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        cta.getSnackbarManager().disableForTesting();
        prepareTabs(2, 0, null);
        enterTabSwitcher(cta);

        Tab tab = cta.getTabModelSelector().getCurrentTab();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    cta.getTabModelSelector()
                            .getCurrentModel()
                            .closeTabs(TabClosureParams.closeTab(tab).build());
                });
        mActivityTestRule.loadUrlInTab(
                mUrl, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR, tab);
    }

    @Test
    @MediumTest
    public void testUrlUpdatedNotCrashing_ForTabNotInCurrentModel() throws Exception {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabs(1, 1, null);
        enterTabSwitcher(cta);

        Tab tab = cta.getTabModelSelector().getCurrentTab();
        switchTabModel(cta, false);

        mActivityTestRule.loadUrlInTab(
                mUrl, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR, tab);
    }

    private int getTabCountInCurrentTabModel() {
        return mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel().getCount();
    }

    @Test
    @LargeTest
    public void testThumbnailAspectRatio_default() {
        prepareTabs(2, 0, "about:blank");
        enterTabSwitcher(mActivityTestRule.getActivity());
        onViewWaiting(tabSwitcherViewMatcher())
                .check(
                        ThumbnailAspectRatioAssertion.havingAspectRatio(
                                TabUtils.getTabThumbnailAspectRatio(
                                        mActivityTestRule.getActivity(),
                                        mActivityTestRule
                                                .getActivity()
                                                .getBrowserControlsManager())));
    }

    @Test
    @MediumTest
    public void testThumbnailFetchingResult_jpeg() throws Exception {
        // May be called when setting both grid card size and thumbnail fetcher.
        var histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT,
                                TabContentManager.ThumbnailFetchingResult.GOT_JPEG)
                        .allowExtraRecords(TabContentManager.UMA_THUMBNAIL_FETCHING_RESULT)
                        .build();

        simulateJpegHasCachedWithDefaultAspectRatio();

        enterTabSwitcher(mActivityTestRule.getActivity());

        onViewWaiting(
                        allOf(
                                isDescendantOfA(
                                        withId(
                                                TabUiTestHelper.getTabSwitcherAncestorId(
                                                        mActivityTestRule.getActivity()))),
                                withId(R.id.tab_thumbnail)))
                .check(matches(isDisplayed()));

        histograms.assertExpected();
    }

    @Test
    @MediumTest
    public void testRecycling_defaultAspectRatio() {
        prepareTabs(10, 0, mUrl);
        ChromeTabUtils.switchTabInCurrentTabModel(mActivityTestRule.getActivity(), 0);
        enterTabSwitcher(mActivityTestRule.getActivity());
        onView(tabSwitcherViewMatcher()).perform(RecyclerViewActions.scrollToPosition(9));
    }

    @Test
    @MediumTest
    public void testCloseTabViaCloseButton() throws Exception {
        mActivityTestRule.getActivity().getSnackbarManager().disableForTesting();
        prepareTabs(1, 0, null);
        enterTabSwitcher(mActivityTestRule.getActivity());

        onView(
                        allOf(
                                withId(R.id.action_button),
                                withParent(withId(R.id.content_view)),
                                withEffectiveVisibility(VISIBLE)))
                .perform(click());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "Flaky - https://crbug.com/1124041, crbug.com/1061178")
    public void testSwipeToDismiss_Gts() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        // Create 3 tabs and merge the first two tabs into one group.
        createTabs(cta, false, 3);
        enterTabSwitcher(cta);
        TabModel normalTabModel = cta.getTabModelSelector().getModel(false);
        List<Tab> tabGroup =
                new ArrayList<>(
                        Arrays.asList(normalTabModel.getTabAt(0), normalTabModel.getTabAt(1)));
        createTabGroup(cta, false, tabGroup);
        verifyTabSwitcherCardCount(cta, 2);
        verifyTabModelTabCount(cta, 3, 0);

        // Swipe to dismiss a single tab card.
        onView(tabSwitcherViewMatcher())
                .perform(
                        RecyclerViewActions.actionOnItemAtPosition(
                                1, getSwipeToDismissAction(false)));
        verifyTabSwitcherCardCount(cta, 1);
        verifyTabModelTabCount(cta, 2, 0);

        // Swipe to dismiss a tab group card.
        onView(tabSwitcherViewMatcher())
                .perform(
                        RecyclerViewActions.actionOnItemAtPosition(
                                0, getSwipeToDismissAction(true)));
        verifyTabSwitcherCardCount(cta, 0);
        verifyTabModelTabCount(cta, 0, 0);
    }

    @Test
    @MediumTest
    public void testCloseButtonDescription() {
        String expectedDescription = "Close New tab tab";
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        // Test single tab.
        onView(
                        allOf(
                                withParent(withId(R.id.content_view)),
                                withId(R.id.action_button),
                                withEffectiveVisibility(VISIBLE)))
                .check(ViewContentDescription.havingDescription(expectedDescription));

        // Create 2 tabs and merge them into one group.
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        TabModel normalTabModel = cta.getTabModelSelector().getModel(false);
        List<Tab> tabGroup =
                new ArrayList<>(
                        Arrays.asList(normalTabModel.getTabAt(0), normalTabModel.getTabAt(1)));
        createTabGroup(cta, false, tabGroup);
        verifyTabSwitcherCardCount(cta, 1);

        // Test group tab.
        expectedDescription = "Close tab group with 2 tabs.";
        onView(
                        allOf(
                                withParent(withId(R.id.content_view)),
                                withId(R.id.action_button),
                                withEffectiveVisibility(VISIBLE)))
                .check(ViewContentDescription.havingDescription(expectedDescription));
    }

    private static class ViewContentDescription implements ViewAssertion {
        private String mExpectedDescription;

        public static ViewContentDescription havingDescription(String description) {
            return new ViewContentDescription(description);
        }

        public ViewContentDescription(String description) {
            mExpectedDescription = description;
        }

        @Override
        public void check(View view, NoMatchingViewException noMatchException) {
            if (noMatchException != null) throw noMatchException;

            assertEquals(mExpectedDescription, view.getContentDescription());
        }
    }

    private static class ThumbnailAspectRatioAssertion implements ViewAssertion {
        private double mExpectedRatio;

        public static ThumbnailAspectRatioAssertion havingAspectRatio(double ratio) {
            return new ThumbnailAspectRatioAssertion(ratio);
        }

        private ThumbnailAspectRatioAssertion(double expectedRatio) {
            mExpectedRatio = expectedRatio;
        }

        @Override
        public void check(View view, NoMatchingViewException noMatchException) {
            if (noMatchException != null) throw noMatchException;

            RecyclerView recyclerView = (RecyclerView) view;

            RecyclerView.Adapter adapter = recyclerView.getAdapter();
            boolean hasAtLeastOneValidViewHolder = false;
            for (int i = 0; i < adapter.getItemCount(); i++) {
                RecyclerView.ViewHolder viewHolder =
                        recyclerView.findViewHolderForAdapterPosition(i);
                if (viewHolder != null) {
                    hasAtLeastOneValidViewHolder = true;
                    ViewLookupCachingFrameLayout tabView =
                            (ViewLookupCachingFrameLayout) viewHolder.itemView;
                    TabThumbnailView thumbnail =
                            (TabThumbnailView) tabView.fastFindViewById(R.id.tab_thumbnail);

                    double thumbnailViewRatio = thumbnail.getWidth() * 1.0 / thumbnail.getHeight();
                    int pixelDelta =
                            Math.abs(
                                    (int) Math.round(thumbnail.getHeight() * mExpectedRatio)
                                            - thumbnail.getWidth());
                    assertTrue(
                            "Actual ratio: "
                                    + thumbnailViewRatio
                                    + "; Expected ratio: "
                                    + mExpectedRatio
                                    + "; Pixel delta: "
                                    + pixelDelta,
                            pixelDelta
                                    <= thumbnail.getWidth()
                                            * TabContentManager.PIXEL_TOLERANCE_PERCENT);
                }
            }
            assertTrue("should have at least one valid ViewHolder", hasAtLeastOneValidViewHolder);
        }
    }

    @Test
    @MediumTest
    public void verifyTabGroupStateAfterReparenting() throws Exception {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        assertTrue(
                cta.getTabModelSelector().getTabModelFilterProvider().getCurrentTabModelFilter()
                        instanceof TabGroupModelFilter);
        mActivityTestRule.loadUrl(mUrl);
        Tab parentTab = cta.getTabModelSelector().getCurrentTab();

        // Create a tab group.
        TabCreator tabCreator = ThreadUtils.runOnUiThreadBlocking(() -> cta.getTabCreator(false));
        LoadUrlParams loadUrlParams = new LoadUrlParams(mUrl);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        tabCreator.createNewTab(
                                loadUrlParams,
                                TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                                parentTab));
        Tab childTab = cta.getTabModelSelector().getCurrentModel().getTabAt(1);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);
        TabGroupModelFilter filter =
                (TabGroupModelFilter)
                        cta.getTabModelSelector()
                                .getTabModelFilterProvider()
                                .getCurrentTabModelFilter();
        ThreadUtils.runOnUiThreadBlocking(
                () -> filter.moveTabOutOfGroupInDirection(childTab.getId(), /* trailing= */ true));
        verifyTabSwitcherCardCount(cta, 2);

        ThreadUtils.runOnUiThreadBlocking(
                () -> ChromeNightModeTestUtils.setUpNightModeForChromeActivity(true));
        final ChromeTabbedActivity ctaNightMode =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(), ChromeTabbedActivity.class);
        assertTrue(ColorUtils.inNightMode(ctaNightMode));
        CriteriaHelper.pollUiThread(ctaNightMode.getTabModelSelector()::isTabStateInitialized);
        enterTabSwitcher(ctaNightMode);
        verifyTabSwitcherCardCount(ctaNightMode, 2);
    }

    @Test
    @MediumTest
    public void testUndoClosure_AccessibilityMode() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(true));
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        SnackbarManager snackbarManager = mActivityTestRule.getActivity().getSnackbarManager();
        createTabs(cta, false, 3);

        // When grid tab switcher is enabled for accessibility mode, tab closure should still show
        // undo snack bar.
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);
        assertNull(snackbarManager.getCurrentSnackbarForTesting());
        closeFirstTabInTabSwitcher(cta);
        assertTrue(
                snackbarManager.getCurrentSnackbarForTesting().getController()
                        instanceof UndoBarController);
        verifyTabSwitcherCardCount(cta, 2);
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        verifyTabSwitcherCardCount(cta, 3);
    }

    @Test
    @MediumTest
    public void testUndoGroupClosureInTabSwitcher() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        SnackbarManager snackbarManager = mActivityTestRule.getActivity().getSnackbarManager();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        assertNotNull(snackbarManager.getCurrentSnackbarForTesting());

        // Verify close this tab group and undo in tab switcher.
        closeFirstTabGroupInTabSwitcher(cta);
        assertTrue(
                snackbarManager.getCurrentSnackbarForTesting().getController()
                        instanceof UndoBarController);
        verifyTabSwitcherCardCount(cta, 0);
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        verifyTabSwitcherCardCount(cta, 1);
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_PARITY_ANDROID,
        ChromeFeatureList.TAB_GROUP_PANE_ANDROID
    })
    @DisabledTest(message = "crbug.com/353946452")
    public void testTabGroupOverflowMenuInTabSwitcher_closeGroup() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        SnackbarManager snackbarManager = cta.getSnackbarManager();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        // Create a tab group.
        mergeNormalTabsToGroupWithDialog(cta, 2);
        verifyGroupVisualDataDialogOpenedAndDismiss(cta);
        verifyTabSwitcherCardCount(cta, 1);

        verifyFirstCardTitle("2 tabs");
        verifyFirstCardColor(TabGroupColorId.GREY);

        // Click the close action button to close the group
        String closeButtonText = cta.getString(R.string.close_tab_group_menu_item);
        onView(withId(R.id.action_button)).perform(click());
        onView(allOf(withText(closeButtonText), withId(R.id.menu_item_text))).perform(click());

        // Verify the tab group was closed.
        assertTrue(
                snackbarManager.getCurrentSnackbarForTesting().getController()
                        instanceof UndoBarController);
        verifyTabSwitcherCardCount(cta, 0);
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        verifyTabSwitcherCardCount(cta, 1);
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_PARITY_ANDROID,
        ChromeFeatureList.TAB_GROUP_PANE_ANDROID
    })
    @DisabledTest(message = "crbug.com/360393681")
    public void testTabGroupOverflowMenuInTabSwitcher_renameGroupAccept() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        // Create a tab group.
        mergeNormalTabsToGroupWithDialog(cta, 2);
        verifyGroupVisualDataDialogOpenedAndDismiss(cta);
        verifyTabSwitcherCardCount(cta, 1);

        verifyFirstCardTitle("2 tabs");
        verifyFirstCardColor(TabGroupColorId.GREY);

        // Click the rename action button to rename the group
        String renameButtonText = cta.getString(R.string.rename_tab_group_menu_item);
        onView(withId(R.id.action_button)).perform(click());
        onView(allOf(withText(renameButtonText), withId(R.id.menu_item_text))).perform(click());

        // Verify that the modal dialog is now showing.
        verifyModalDialogShowingAnimationCompleteInTabSwitcher();
        // Verify the visual data dialog exists.
        onViewWaiting(withId(R.id.visual_data_dialog_layout), /* checkRootDialog= */ true)
                .check(matches(isDisplayed()));
        // Wait until the keyboard is showing.
        KeyboardVisibilityDelegate delegate = cta.getWindowAndroid().getKeyboardDelegate();
        CriteriaHelper.pollUiThread(
                () -> delegate.isKeyboardShowing(cta, cta.getCompositorViewHolderForTesting()));

        // Change the title.
        editGroupVisualDataDialogTitle(cta, "Test");
        // Change the color.
        String blueColor =
                cta.getString(R.string.accessibility_tab_group_color_picker_color_item_blue);
        String notSelectedStringBlue =
                cta.getString(
                        R.string
                                .accessibility_tab_group_color_picker_color_item_not_selected_description,
                        blueColor);
        onView(withContentDescription(notSelectedStringBlue)).perform(click());

        // Accept the change.
        onView(withId(R.id.positive_button)).perform(click());
        verifyModalDialogHidingAnimationCompleteInTabSwitcher();
        // Check that the title change is reflected.
        verifyFirstCardTitle("Test");
        // Verify the color icon exists.
        verifyFirstCardColor(TabGroupColorId.BLUE);
        verifyTabSwitcherCardCount(cta, 1);
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_PARITY_ANDROID,
        ChromeFeatureList.TAB_GROUP_PANE_ANDROID
    })
    @DisabledTest(message = "crbug.com/360393681")
    public void testTabGroupOverflowMenuInTabSwitcher_renameGroupDecline() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        // Create a tab group.
        mergeNormalTabsToGroupWithDialog(cta, 2);
        verifyGroupVisualDataDialogOpenedAndDismiss(cta);
        verifyTabSwitcherCardCount(cta, 1);

        verifyFirstCardTitle("2 tabs");
        verifyFirstCardColor(TabGroupColorId.GREY);

        // Click the rename action button to rename the group
        String renameButtonText = cta.getString(R.string.rename_tab_group_menu_item);
        onView(withId(R.id.action_button)).perform(click());
        onView(allOf(withText(renameButtonText), withId(R.id.menu_item_text))).perform(click());

        // Verify that the modal dialog is now showing.
        verifyModalDialogShowingAnimationCompleteInTabSwitcher();
        // Verify the visual data dialog exists.
        onViewWaiting(withId(R.id.visual_data_dialog_layout), /* checkRootDialog= */ true)
                .check(matches(isDisplayed()));
        // Wait until the keyboard is showing.
        KeyboardVisibilityDelegate delegate = cta.getWindowAndroid().getKeyboardDelegate();
        CriteriaHelper.pollUiThread(
                () -> delegate.isKeyboardShowing(cta, cta.getCompositorViewHolderForTesting()));

        // Change the title.
        editGroupVisualDataDialogTitle(cta, "Test");
        // Change the color.
        String blueColor =
                cta.getString(R.string.accessibility_tab_group_color_picker_color_item_blue);
        String notSelectedStringBlue =
                cta.getString(
                        R.string
                                .accessibility_tab_group_color_picker_color_item_not_selected_description,
                        blueColor);
        onView(withContentDescription(notSelectedStringBlue)).perform(click());

        // Decline the change.
        onView(withId(R.id.negative_button)).perform(click());
        verifyModalDialogHidingAnimationCompleteInTabSwitcher();
        // Check that the title change is reflected.
        verifyFirstCardTitle("2 tabs");
        // Verify the color icon exists.
        verifyFirstCardColor(TabGroupColorId.GREY);
        verifyTabSwitcherCardCount(cta, 1);
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_PARITY_ANDROID,
        ChromeFeatureList.TAB_GROUP_PANE_ANDROID,
        ChromeFeatureList.TAB_GROUP_SYNC_ANDROID,
    })
    @DisabledTest(message = "crbug.com/360393681")
    public void testTabGroupOverflowMenuInTabSwitcher_ungroupAccept() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        // Create a tab group.
        mergeNormalTabsToGroupWithDialog(cta, 2);
        verifyGroupVisualDataDialogOpenedAndDismiss(cta);
        verifyTabSwitcherCardCount(cta, 1);

        verifyFirstCardTitle("2 tabs");
        verifyFirstCardColor(TabGroupColorId.GREY);

        // Click the ungroup action button to ungroup the group
        String ungroupButtonText = cta.getString(R.string.ungroup_tab_group_menu_item);
        onView(withId(R.id.action_button)).perform(click());
        onView(allOf(withText(ungroupButtonText), withId(R.id.menu_item_text))).perform(click());

        // Verify that the modal dialog is now showing.
        verifyModalDialogShowingAnimationCompleteInTabSwitcher();
        // Verify the ungroup dialog exists.
        onViewWaiting(withId(R.id.stop_showing_check_box), /* checkRootDialog= */ true)
                .check(matches(isDisplayed()));
        // Confirm the action.
        onView(withId(R.id.positive_button)).perform(click());
        // Verify the tab group was ungrouped.
        verifyTabSwitcherCardCount(cta, 2);
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_PARITY_ANDROID,
        ChromeFeatureList.TAB_GROUP_PANE_ANDROID,
    })
    @DisabledTest(message = "crbug.com/353946452")
    public void testTabGroupOverflowMenuInTabSwitcher_ungroupDecline() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        // Create a tab group.
        mergeNormalTabsToGroupWithDialog(cta, 2);
        verifyGroupVisualDataDialogOpenedAndDismiss(cta);
        verifyTabSwitcherCardCount(cta, 1);

        verifyFirstCardTitle("2 tabs");
        verifyFirstCardColor(TabGroupColorId.GREY);

        // Click the ungroup action button to ungroup the group
        String ungroupButtonText = cta.getString(R.string.ungroup_tab_group_menu_item);
        onView(withId(R.id.action_button)).perform(click());
        onView(allOf(withText(ungroupButtonText), withId(R.id.menu_item_text))).perform(click());

        // Verify that the modal dialog is now showing.
        verifyModalDialogShowingAnimationCompleteInTabSwitcher();
        // Verify the ungroup dialog exists.
        onViewWaiting(withId(R.id.stop_showing_check_box), /* checkRootDialog= */ true)
                .check(matches(isDisplayed()));
        // Decline the action.
        onView(withId(R.id.negative_button)).perform(click());
        // Verify the tab group was not ungrouped.
        verifyTabSwitcherCardCount(cta, 1);
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_PARITY_ANDROID,
        ChromeFeatureList.TAB_GROUP_PANE_ANDROID,
    })
    @DisabledTest(message = "Flaky - crbug.com/353463207")
    public void testTabGroupOverflowMenuInTabSwitcher_ungroupDoNotShowAgain() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        // Create a tab group.
        mergeNormalTabsToGroupWithDialog(cta, 2);
        verifyGroupVisualDataDialogOpenedAndDismiss(cta);
        verifyTabSwitcherCardCount(cta, 1);

        verifyFirstCardTitle("2 tabs");
        verifyFirstCardColor(TabGroupColorId.GREY);

        // Click the ungroup action button to ungroup the group
        String ungroupButtonText = cta.getString(R.string.ungroup_tab_group_menu_item);
        onView(withId(R.id.action_button)).perform(click());
        onView(allOf(withText(ungroupButtonText), withId(R.id.menu_item_text))).perform(click());

        // Verify that the modal dialog is now showing.
        verifyModalDialogShowingAnimationCompleteInTabSwitcher();
        // Verify the ungroup dialog exists.
        onViewWaiting(withId(R.id.stop_showing_check_box), /* checkRootDialog= */ true)
                .check(matches(isDisplayed()));
        // Select the checkbox.
        onView(withId(R.id.stop_showing_check_box)).perform(click());
        // Confirm the action.
        onView(withId(R.id.positive_button)).perform(click());
        // Verify the tab group was ungrouped.
        verifyTabSwitcherCardCount(cta, 2);

        // Regroup the tabs.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Click the ungroup action button to ungroup the group
        onView(withId(R.id.action_button)).perform(click());
        onView(allOf(withText(ungroupButtonText), withId(R.id.menu_item_text))).perform(click());
        // Verify the ungroup dialog does not exist.
        onView(withId(R.id.stop_showing_check_box)).check(doesNotExist());
        // Verify the tab group was ungrouped.
        verifyTabSwitcherCardCount(cta, 2);
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_PARITY_ANDROID,
        ChromeFeatureList.TAB_GROUP_PANE_ANDROID,
        ChromeFeatureList.TAB_GROUP_SYNC_ANDROID,
    })
    @DisabledTest(message = "Flaky - crbug.com/353463207")
    public void testTabGroupOverflowMenuInTabSwitcher_deleteGroupAccept() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        // Create a tab group.
        mergeNormalTabsToGroupWithDialog(cta, 2);
        verifyGroupVisualDataDialogOpenedAndDismiss(cta);
        verifyTabSwitcherCardCount(cta, 1);

        verifyFirstCardTitle("2 tabs");
        verifyFirstCardColor(TabGroupColorId.GREY);

        // Click the delete action button to close the group
        String deleteButtonText = cta.getString(R.string.delete_tab_group_menu_item);
        onView(withId(R.id.action_button)).perform(click());
        onView(allOf(withText(deleteButtonText), withId(R.id.menu_item_text))).perform(click());

        // Verify that the modal dialog is now showing.
        verifyModalDialogShowingAnimationCompleteInTabSwitcher();
        // Verify the delete dialog exists.
        onViewWaiting(withId(R.id.stop_showing_check_box), /* checkRootDialog= */ true)
                .check(matches(isDisplayed()));
        // Confirm the action.
        onView(withId(R.id.positive_button)).perform(click());
        // Verify the tab group was closed.
        verifyTabSwitcherCardCount(cta, 0);
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_PARITY_ANDROID,
        ChromeFeatureList.TAB_GROUP_PANE_ANDROID,
        ChromeFeatureList.TAB_GROUP_SYNC_ANDROID,
    })
    @DisabledTest(message = "crbug.com/360393681")
    public void testTabGroupOverflowMenuInTabSwitcher_noDeleteIncognito() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabs(1, 2, "about:blank");
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        // Create a tab group.
        mergeNormalTabsToGroupWithDialog(cta, 2);
        verifyGroupVisualDataDialogOpenedAndDismiss(cta);
        verifyTabSwitcherCardCount(cta, 1);

        verifyFirstCardTitle("2 tabs");
        // Ignore color because incognito's color state list is different.

        // Click the delete action button to close the group
        String deleteButtonText = cta.getString(R.string.delete_tab_group_menu_item);
        onView(withId(R.id.action_button)).perform(click());
        onView(allOf(withText(deleteButtonText), withId(R.id.menu_item_text)))
                .check(doesNotExist());

        String closeButtonText = cta.getString(R.string.close_tab_group_menu_item);
        onView(allOf(withText(closeButtonText), withId(R.id.menu_item_text))).perform(click());

        // Closing the last incognito tabs will select the regular tab model.
        CriteriaHelper.pollUiThread(
                () -> {
                    return !cta.getTabModelSelectorSupplier()
                            .get()
                            .isIncognitoBrandedModelSelected();
                });

        // Verify the regular tab model still has a tab.
        verifyTabSwitcherCardCount(cta, 1);
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_PARITY_ANDROID,
        ChromeFeatureList.TAB_GROUP_PANE_ANDROID,
        ChromeFeatureList.TAB_GROUP_SYNC_ANDROID,
    })
    @DisabledTest(message = "crbug.com/353463207")
    public void testTabGroupOverflowMenuInTabSwitcher_deleteGroupDecline() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        // Create a tab group.
        mergeNormalTabsToGroupWithDialog(cta, 2);
        verifyGroupVisualDataDialogOpenedAndDismiss(cta);
        verifyTabSwitcherCardCount(cta, 1);

        verifyFirstCardTitle("2 tabs");
        verifyFirstCardColor(TabGroupColorId.GREY);

        // Click the delete action button to close the group
        String deleteButtonText = cta.getString(R.string.delete_tab_group_menu_item);
        onView(withId(R.id.action_button)).perform(click());
        onView(allOf(withText(deleteButtonText), withId(R.id.menu_item_text))).perform(click());

        // Verify that the modal dialog is now showing.
        verifyModalDialogShowingAnimationCompleteInTabSwitcher();
        // Verify the delete dialog exists.
        onViewWaiting(withId(R.id.stop_showing_check_box), /* checkRootDialog= */ true)
                .check(matches(isDisplayed()));
        // Decline the action.
        Espresso.pressBack();
        // Verify the tab group was not closed.
        verifyTabSwitcherCardCount(cta, 1);
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_PARITY_ANDROID,
        ChromeFeatureList.TAB_GROUP_PANE_ANDROID,
        ChromeFeatureList.TAB_GROUP_SYNC_ANDROID,
    })
    @DisabledTest(message = "crbug.com/360393681")
    public void testTabGroupOverflowMenuInTabSwitcher_deleteGroupDoNotShowAgain() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        String expectedDescription = "Open the tab group action menu for tab group Test";
        SnackbarManager snackbarManager = cta.getSnackbarManager();
        createTabs(cta, false, 4);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 4);

        // Merge last two tabs into a group.
        TabListEditorTestingRobot robot = new TabListEditorTestingRobot();
        enterTabListEditor(cta);
        robot.resultRobot.verifyTabListEditorIsVisible();

        robot.actionRobot.clickItemAtAdapterPosition(2);
        robot.actionRobot.clickItemAtAdapterPosition(3);
        robot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Group tabs");
        robot.resultRobot.verifyTabListEditorIsHidden();

        // Verify the visual data dialog exists.
        verifyModalDialogShowingAnimationCompleteInTabSwitcher();
        onViewWaiting(withId(R.id.visual_data_dialog_layout), /* checkRootDialog= */ true)
                .check(matches(isDisplayed()));

        // Change the title.
        editGroupVisualDataDialogTitle(cta, "Test");
        // Accept the change.
        onView(withId(R.id.positive_button)).perform(click());
        verifyModalDialogHidingAnimationCompleteInTabSwitcher();
        verifyTabSwitcherCardCount(cta, 3);

        // Merge first two tabs into a group.
        robot = new TabListEditorTestingRobot();
        enterTabListEditor(cta);
        robot.resultRobot.verifyTabListEditorIsVisible();

        robot.actionRobot.clickItemAtAdapterPosition(0);
        robot.actionRobot.clickItemAtAdapterPosition(1);
        robot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Group tabs");
        robot.resultRobot.verifyTabListEditorIsHidden();

        verifyGroupVisualDataDialogOpenedAndDismiss(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Click the delete action button to close the group "Test"
        String deleteButtonText = cta.getString(R.string.delete_tab_group_menu_item);
        onView(allOf(withContentDescription(expectedDescription), withId(R.id.action_button)))
                .perform(click());
        onView(allOf(withText(deleteButtonText), withId(R.id.menu_item_text))).perform(click());

        // Verify that the modal dialog is now showing.
        verifyModalDialogShowingAnimationCompleteInTabSwitcher();
        // Verify the delete dialog exists.
        onViewWaiting(withId(R.id.stop_showing_check_box), /* checkRootDialog= */ true)
                .check(matches(isDisplayed()));
        // Select the checkbox.
        onView(withId(R.id.stop_showing_check_box)).perform(click());
        // Confirm the action.
        onView(withId(R.id.positive_button)).perform(click());
        // Verify the tab group was closed.
        verifyTabSwitcherCardCount(cta, 1);

        // Click the delete action button to delete the group
        onView(withId(R.id.action_button)).perform(click());
        onView(allOf(withText(deleteButtonText), withId(R.id.menu_item_text))).perform(click());
        // Verify the delete dialog does not exist.
        onView(withId(R.id.stop_showing_check_box)).check(doesNotExist());
        // Verify the tab group was closed.
        assertTrue(
                snackbarManager.getCurrentSnackbarForTesting().getController()
                        instanceof UndoBarController);
        verifyTabSwitcherCardCount(cta, 0);
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        verifyTabSwitcherCardCount(cta, 1);
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_PARITY_ANDROID,
        ChromeFeatureList.TAB_GROUP_PANE_ANDROID,
        ChromeFeatureList.TAB_GROUP_CREATION_DIALOG_ANDROID,
    })
    @DisableFeatures({
        ChromeFeatureList.TAB_GROUP_SYNC_ANDROID,
    })
    public void testTabGroupDialogSingleTab() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 1);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);
        // Create a tab group.
        mergeNormalTabsToGroupWithDialog(cta, 1);
        verifyGroupVisualDataDialogOpenedAndDismiss(cta);
        verifyTabSwitcherCardCount(cta, 1);
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_PARITY_ANDROID,
        ChromeFeatureList.TAB_GROUP_PANE_ANDROID,
    })
    @DisableFeatures({
        ChromeFeatureList.TAB_GROUP_SYNC_ANDROID,
        ChromeFeatureList.TAB_GROUP_CREATION_DIALOG_ANDROID,
    })
    public void testNoTabGroupDialogSingleTab() {
        TabGroupModelFilter.SKIP_TAB_GROUP_CREATION_DIALOG.setForTesting(true);

        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        SnackbarManager snackbarManager = mActivityTestRule.getActivity().getSnackbarManager();
        createTabs(cta, false, 1);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);
        // Create a tab group.
        mergeNormalTabsToGroupWithDialog(cta, 1);
        // Verify the undo group merge snackbar is showing.
        assertTrue(
                snackbarManager.getCurrentSnackbarForTesting().getController()
                        instanceof UndoGroupSnackbarController);
        // Verify that no modal dialog was shown.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mModalDialogManager.isShowing(), Matchers.is(false));
                });
        verifyTabSwitcherCardCount(cta, 1);

        TabGroupModelFilter.SKIP_TAB_GROUP_CREATION_DIALOG.setForTesting(false);
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_PARITY_ANDROID,
        ChromeFeatureList.TAB_GROUP_PANE_ANDROID,
    })
    @DisableFeatures({
        ChromeFeatureList.TAB_GROUP_SYNC_ANDROID,
    })
    @DisabledTest(message = "crbug.com/360393681")
    public void testTabGroupOverflowMenuInTabSwitcher_deleteGroupNoShowSyncDisabled() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        // Create a tab group.
        mergeNormalTabsToGroupWithDialog(cta, 2);
        verifyGroupVisualDataDialogOpenedAndDismiss(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Verify the delete action button does not exist
        String deleteButtonText = cta.getString(R.string.delete_tab_group_menu_item);
        onView(withId(R.id.action_button)).perform(click());
        onView(allOf(withText(deleteButtonText), withId(R.id.menu_item_text)))
                .check(doesNotExist());
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
    @DisabledTest(message = "crbug.com/360393681")
    public void testTabGroupColorInTabSwitcher() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        // Create a tab group.
        mergeNormalTabsToGroupWithDialog(cta, 2);

        // Expect that the the dialog is dismissed via another action.
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabGroupParity.TabGroupCreationDialogResultAction", 2);

        verifyGroupVisualDataDialogOpenedAndDismiss(cta);

        // Verify the color icon exists.
        onView(allOf(withId(R.id.tab_favicon), withParent(withId(R.id.card_view))))
                .check(matches(isDisplayed()));
        watcher.assertExpected();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
    @DisabledTest(message = "crbug.com/360393681")
    public void testTabGroupCreation_acceptInputValues() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeNormalTabsToGroupWithDialog(cta, 2);
        // Verify the visual data dialog exists.
        verifyModalDialogShowingAnimationCompleteInTabSwitcher();
        onViewWaiting(withId(R.id.visual_data_dialog_layout), /* checkRootDialog= */ true)
                .check(matches(isDisplayed()));

        // Change the title.
        editGroupVisualDataDialogTitle(cta, "Test");
        // Change the color.
        String blueColor =
                cta.getString(R.string.accessibility_tab_group_color_picker_color_item_blue);
        String notSelectedStringBlue =
                cta.getString(
                        R.string
                                .accessibility_tab_group_color_picker_color_item_not_selected_description,
                        blueColor);
        onView(withContentDescription(notSelectedStringBlue)).perform(click());

        // Expect a changed color and title selection to be recorded and an acceptance action.
        var histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.TabGroupParity.TabGroupCreationFinalSelections", 3)
                        .expectIntRecord(
                                "Android.TabGroupParity.TabGroupCreationDialogResultAction", 0)
                        .build();

        // Accept the change.
        onView(withId(R.id.positive_button)).perform(click());
        verifyModalDialogHidingAnimationCompleteInTabSwitcher();
        // Check that the title change is reflected.
        verifyFirstCardTitle("Test");
        // Verify the color icon exists.
        verifyFirstCardColor(TabGroupColorId.BLUE);
        histograms.assertExpected();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
    @DisabledTest(message = "crbug.com/360393681")
    public void testTabGroupCreation_acceptNullTitle() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeNormalTabsToGroupWithDialog(cta, 2);
        // Verify the visual data dialog exists.
        verifyModalDialogShowingAnimationCompleteInTabSwitcher();
        onViewWaiting(withId(R.id.visual_data_dialog_layout), /* checkRootDialog= */ true)
                .check(matches(isDisplayed()));

        // Close the soft keyboard that appears when the dialog is shown.
        closeSoftKeyboard();

        // Expect changed color and title selection to be recorded.
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabGroupParity.TabGroupCreationFinalSelections", 0);

        // Accept without changing the title.
        onView(withId(R.id.positive_button)).perform(click());
        verifyModalDialogHidingAnimationCompleteInTabSwitcher();

        // Check that the title change is reflected.
        verifyFirstCardTitle("2 tabs");
        verifyFirstCardColor(TabGroupColorId.GREY);
        watcher.assertExpected();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
    @DisabledTest(message = "crbug.com/360393681")
    public void testTabGroupCreation_dismissEmptyTitle() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeNormalTabsToGroupWithDialog(cta, 2);
        // Verify the visual data dialog exists.
        verifyModalDialogShowingAnimationCompleteInTabSwitcher();
        onViewWaiting(withId(R.id.visual_data_dialog_layout), /* checkRootDialog= */ true)
                .check(matches(isDisplayed()));

        // Close the soft keyboard that appears when the dialog is shown.
        closeSoftKeyboard();

        // Change the title.
        editGroupVisualDataDialogTitle(cta, "");
        // Change the color.
        String blueColor =
                cta.getString(R.string.accessibility_tab_group_color_picker_color_item_blue);
        String notSelectedStringBlue =
                cta.getString(
                        R.string
                                .accessibility_tab_group_color_picker_color_item_not_selected_description,
                        blueColor);
        onView(withContentDescription(notSelectedStringBlue)).perform(click());

        // Enact a backpress to dismiss the dialog.
        Espresso.pressBack();
        verifyModalDialogHidingAnimationCompleteInTabSwitcher();
        // Check that the title change has reverted to the default null state.
        verifyFirstCardTitle("2 tabs");
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
    @DisabledTest(message = "Flaky - crbug.com/353463207")
    public void testTabGroupCreation_rejectInvalidTitle() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeNormalTabsToGroupWithDialog(cta, 2);
        // Verify the visual data dialog exists.
        verifyModalDialogShowingAnimationCompleteInTabSwitcher();
        onViewWaiting(withId(R.id.visual_data_dialog_layout), /* checkRootDialog= */ true)
                .check(matches(isDisplayed()));

        // Change the title and accept.
        editGroupVisualDataDialogTitle(cta, "");
        onView(withId(R.id.positive_button)).perform(click());

        // Verify that the change was rejected and the dialog is still showing.
        onViewWaiting(withId(R.id.visual_data_dialog_layout), /* checkRootDialog= */ true)
                .check(matches(isDisplayed()));

        // Exit the dialog.
        Espresso.pressBack();
        verifyModalDialogHidingAnimationCompleteInTabSwitcher();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
    @DisabledTest(message = "Flaky - crbug.com/353463207")
    public void testTabGroupCreation_dismissSavesState() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeNormalTabsToGroupWithDialog(cta, 2);
        // Verify the visual data dialog exists.
        verifyModalDialogShowingAnimationCompleteInTabSwitcher();
        onViewWaiting(withId(R.id.visual_data_dialog_layout), /* checkRootDialog= */ true)
                .check(matches(isDisplayed()));

        // Change the title.
        editGroupVisualDataDialogTitle(cta, "Test");
        // Change the color.
        String blueColor =
                cta.getString(R.string.accessibility_tab_group_color_picker_color_item_blue);
        String notSelectedStringBlue =
                cta.getString(
                        R.string
                                .accessibility_tab_group_color_picker_color_item_not_selected_description,
                        blueColor);
        onView(withContentDescription(notSelectedStringBlue)).perform(click());

        // Expect that the dismiss action is recorded.
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabGroupParity.TabGroupCreationDialogResultAction", 1);

        // Enact a backpress to dismiss the dialog.
        Espresso.pressBack();
        verifyModalDialogHidingAnimationCompleteInTabSwitcher();
        // Check that the title change is reflected.
        verifyFirstCardTitle("Test");
        // Verify the color icon exists.
        verifyFirstCardColor(TabGroupColorId.BLUE);
        watcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testLongPressTab_entryInTabSwitcher_verifyNoSelectionOccurs() {
        TabUiFeatureUtilities.setTabListEditorLongPressEntryEnabledForTesting(true);

        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // LongPress entry to TabListEditor.
        onView(tabSwitcherViewMatcher())
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, longClick()));

        TabListEditorTestingRobot mSelectionEditorRobot = new TabListEditorTestingRobot();
        mSelectionEditorRobot.resultRobot.verifyTabListEditorIsVisible();

        // Verify no selection action occurred to switch the selected tab in the tab model
        Criteria.checkThat(
                mActivityTestRule.getActivity().getCurrentTabModel().index(), Matchers.is(1));
    }

    @Test
    @MediumTest
    public void testLongPressTabGroup_entryInTabSwitcher() {
        TabUiFeatureUtilities.setTabListEditorLongPressEntryEnabledForTesting(true);

        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // LongPress entry to TabListEditor.
        onView(tabSwitcherViewMatcher())
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, longClick()));

        TabListEditorTestingRobot mSelectionEditorRobot = new TabListEditorTestingRobot();
        mSelectionEditorRobot.resultRobot.verifyTabListEditorIsVisible();
    }

    @Test
    @MediumTest
    public void testLongPressTab_verifyPostLongPressClickNoSelectionEditor() {
        TabUiFeatureUtilities.setTabListEditorLongPressEntryEnabledForTesting(true);

        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // LongPress entry to TabListEditor.
        onView(tabSwitcherViewMatcher())
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, longClick()));

        TabListEditorTestingRobot mSelectionEditorRobot = new TabListEditorTestingRobot();
        mSelectionEditorRobot.resultRobot.verifyTabListEditorIsVisible();
        mSelectionEditorRobot.actionRobot.clickItemAtAdapterPosition(0);
        mSelectionEditorRobot.resultRobot.verifyItemSelectedAtAdapterPosition(0);
        Espresso.pressBack();

        onView(tabSwitcherViewMatcher())
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, click()));
        // Check the selected tab in the tab model switches from the second tab to the first to
        // verify clicking the tab worked.
        Criteria.checkThat(
                mActivityTestRule.getActivity().getCurrentTabModel().index(), Matchers.is(0));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
    public void testUndoGroupMergeInTabSwitcher_TabToGroupAdjacent() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        SnackbarManager snackbarManager = mActivityTestRule.getActivity().getSnackbarManager();
        createTabs(cta, false, 3);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);

        // Get the next suggested color id.
        TabGroupModelFilter filter = getTabGroupModelFilter();
        int nextSuggestedColorId = TabGroupColorUtils.getNextSuggestedColorId(filter);

        // Merge first two tabs into a group.
        TabModel normalTabModel = cta.getTabModelSelector().getModel(false);
        List<Tab> tabGroup =
                new ArrayList<>(
                        Arrays.asList(normalTabModel.getTabAt(0), normalTabModel.getTabAt(1)));
        createTabGroup(cta, false, tabGroup);
        ThreadUtils.runOnUiThreadBlocking(
                () -> filter.setTabGroupTitle(normalTabModel.getTabAt(0).getRootId(), "Foo"));
        verifyTabSwitcherCardCount(cta, 2);

        // Assert default color was set properly.
        assertEquals(
                nextSuggestedColorId,
                filter.getTabGroupColor(normalTabModel.getTabAt(0).getRootId()));

        // Merge tab group of 2 at first index with the 3rd tab.
        mergeAllNormalTabsToAGroup(cta);
        assertTrue(
                snackbarManager.getCurrentSnackbarForTesting().getController()
                        instanceof UndoGroupSnackbarController);

        // Assert the default color is still the tab group color
        assertEquals(
                nextSuggestedColorId,
                filter.getTabGroupColor(normalTabModel.getTabAt(0).getRootId()));

        // Undo merge in tab switcher.
        verifyTabSwitcherCardCount(cta, 1);
        assertEquals("3", snackbarManager.getCurrentSnackbarForTesting().getTextForTesting());
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        verifyTabSwitcherCardCount(cta, 2);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            "Foo", filter.getTabGroupTitle(normalTabModel.getTabAt(1).getRootId()));
                    assertNull(filter.getTabGroupTitle(normalTabModel.getTabAt(2).getRootId()));
                    assertEquals(
                            nextSuggestedColorId,
                            filter.getTabGroupColor(normalTabModel.getTabAt(1).getRootId()));
                    assertEquals(
                            TabGroupColorUtils.INVALID_COLOR_ID,
                            filter.getTabGroupColor(normalTabModel.getTabAt(2).getRootId()));
                });
    }

    @Test
    @MediumTest
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_PARITY_ANDROID,
        ChromeFeatureList.TAB_GROUP_PANE_ANDROID
    })
    public void testGroupMerge_UndoBarGoneAfterManualUngroup() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 3);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);

        // Merge first two tabs into a group.
        TabModel normalTabModel = cta.getTabModelSelector().getModel(false);
        List<Tab> tabGroup =
                new ArrayList<>(
                        Arrays.asList(normalTabModel.getTabAt(0), normalTabModel.getTabAt(1)));
        createTabGroup(cta, false, tabGroup);

        // Merge tab group of 2 at first index with the 3rd tab.
        mergeAllNormalTabsToAGroup(cta);

        onViewWaiting(withId(R.id.snackbar_button)).check(matches(isCompletelyDisplayed()));

        String ungroupButtonText = cta.getString(R.string.ungroup_tab_group_menu_item);
        onView(withId(R.id.action_button)).perform(click());
        onView(allOf(withText(ungroupButtonText), withId(R.id.menu_item_text))).perform(click());

        onView(withId(R.id.snackbar_button)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
    public void testUndoGroupMergeInTabSwitcher_GroupToGroupNonAdjacent() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        SnackbarManager snackbarManager = mActivityTestRule.getActivity().getSnackbarManager();
        createTabs(cta, false, 5);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 5);

        // Get the next suggested color id.
        TabGroupModelFilter filter =
                (TabGroupModelFilter)
                        cta.getTabModelSelectorSupplier()
                                .get()
                                .getTabModelFilterProvider()
                                .getCurrentTabModelFilter();
        int nextSuggestedColorId1 = TabGroupColorUtils.getNextSuggestedColorId(filter);

        // Merge last two tabs into a group.
        TabModel normalTabModel = cta.getTabModelSelector().getModel(false);
        List<Tab> tabGroup =
                new ArrayList<>(
                        Arrays.asList(normalTabModel.getTabAt(3), normalTabModel.getTabAt(4)));
        createTabGroup(cta, false, tabGroup);
        verifyTabSwitcherCardCount(cta, 4);

        // Assert default color 1 was set properly.
        assertEquals(
                nextSuggestedColorId1,
                filter.getTabGroupColor(normalTabModel.getTabAt(4).getRootId()));

        // Get the next suggested color id.
        int nextSuggestedColorId2 =
                TabGroupColorUtils.getNextSuggestedColorId(
                        (TabGroupModelFilter)
                                cta.getTabModelSelectorSupplier()
                                        .get()
                                        .getTabModelFilterProvider()
                                        .getCurrentTabModelFilter());

        // Merge first two tabs into a group.
        List<Tab> tabGroup2 =
                new ArrayList<>(
                        Arrays.asList(normalTabModel.getTabAt(0), normalTabModel.getTabAt(1)));
        createTabGroup(cta, false, tabGroup2);
        verifyTabSwitcherCardCount(cta, 3);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    filter.setTabGroupTitle(normalTabModel.getTabAt(3).getRootId(), "Foo");
                    filter.setTabGroupTitle(normalTabModel.getTabAt(1).getRootId(), "Bar");
                });

        // Assert default color 2 was set properly.
        assertEquals(
                nextSuggestedColorId2,
                filter.getTabGroupColor(normalTabModel.getTabAt(1).getRootId()));

        // Merge the two tab groups into a group.
        List<Tab> tabGroup3 =
                new ArrayList<>(
                        Arrays.asList(normalTabModel.getTabAt(0), normalTabModel.getTabAt(3)));
        createTabGroup(cta, false, tabGroup3);
        assertTrue(
                snackbarManager.getCurrentSnackbarForTesting().getController()
                        instanceof UndoGroupSnackbarController);

        // Assert default color 2 was set as the overall merged group color.
        assertEquals(
                nextSuggestedColorId2,
                filter.getTabGroupColor(normalTabModel.getTabAt(3).getRootId()));

        // Undo merge in tab switcher.
        verifyTabSwitcherCardCount(cta, 2);
        assertEquals("4", snackbarManager.getCurrentSnackbarForTesting().getTextForTesting());
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        verifyTabSwitcherCardCount(cta, 3);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            "Foo", filter.getTabGroupTitle(normalTabModel.getTabAt(4).getRootId()));
                    assertEquals(
                            "Bar", filter.getTabGroupTitle(normalTabModel.getTabAt(0).getRootId()));
                    assertEquals(
                            nextSuggestedColorId1,
                            filter.getTabGroupColor(normalTabModel.getTabAt(4).getRootId()));
                    assertEquals(
                            nextSuggestedColorId2,
                            filter.getTabGroupColor(normalTabModel.getTabAt(0).getRootId()));
                });
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
    public void testUndoGroupMergeInTabSwitcher_GroupToGroupNonAdjacent_TabsAreSelected() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        SnackbarManager snackbarManager = mActivityTestRule.getActivity().getSnackbarManager();
        createTabs(cta, false, 5);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 5);

        // Get the next suggested color id.
        TabGroupModelFilter filter =
                (TabGroupModelFilter)
                        cta.getTabModelSelectorSupplier()
                                .get()
                                .getTabModelFilterProvider()
                                .getCurrentTabModelFilter();
        int nextSuggestedColorId1 = TabGroupColorUtils.getNextSuggestedColorId(filter);

        // Merge last two tabs into a group.
        TabModel normalTabModel = cta.getTabModelSelector().getModel(false);
        List<Tab> tabGroup =
                new ArrayList<>(
                        Arrays.asList(normalTabModel.getTabAt(3), normalTabModel.getTabAt(4)));
        createTabGroup(cta, false, tabGroup);
        verifyTabSwitcherCardCount(cta, 4);

        // Assert default color 1 was set properly.
        assertEquals(
                nextSuggestedColorId1,
                filter.getTabGroupColor(normalTabModel.getTabAt(4).getRootId()));

        // Get the next suggested color id.
        int nextSuggestedColorId2 =
                TabGroupColorUtils.getNextSuggestedColorId(
                        (TabGroupModelFilter)
                                cta.getTabModelSelectorSupplier()
                                        .get()
                                        .getTabModelFilterProvider()
                                        .getCurrentTabModelFilter());

        // Merge first two tabs into a group.
        List<Tab> tabGroup2 =
                new ArrayList<>(
                        Arrays.asList(normalTabModel.getTabAt(0), normalTabModel.getTabAt(1)));
        createTabGroup(cta, false, tabGroup2);
        verifyTabSwitcherCardCount(cta, 3);

        // Assert default color 2 was set properly.
        assertEquals(
                nextSuggestedColorId2,
                filter.getTabGroupColor(normalTabModel.getTabAt(1).getRootId()));

        // Verify the 2nd tab group is selected.
        verifyItemSelectedAtPosition(2);

        // Merge the two tab groups into a group.
        List<Tab> tabGroup3 =
                new ArrayList<>(
                        Arrays.asList(normalTabModel.getTabAt(0), normalTabModel.getTabAt(3)));
        createTabGroup(cta, false, tabGroup3);
        assertTrue(
                snackbarManager.getCurrentSnackbarForTesting().getController()
                        instanceof UndoGroupSnackbarController);

        // Assert default color 2 was set as the overall merged group color.
        assertEquals(
                nextSuggestedColorId2,
                filter.getTabGroupColor(normalTabModel.getTabAt(3).getRootId()));

        // After the merge, the tab group which was merged should now be selected.
        verifyItemSelectedAtPosition(0);

        // Undo merge in tab switcher.
        verifyTabSwitcherCardCount(cta, 2);
        assertEquals("4", snackbarManager.getCurrentSnackbarForTesting().getTextForTesting());
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        verifyTabSwitcherCardCount(cta, 3);

        // After the undo, the original tab group should be selected.
        verifyItemSelectedAtPosition(2);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
    public void testUndoGroupMergeInTabSwitcher_PostMergeGroupTitleCommit() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        SnackbarManager snackbarManager = mActivityTestRule.getActivity().getSnackbarManager();
        createTabs(cta, false, 3);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);

        // Get the next suggested color id.
        TabGroupModelFilter filter =
                (TabGroupModelFilter)
                        cta.getTabModelSelectorSupplier()
                                .get()
                                .getTabModelFilterProvider()
                                .getCurrentTabModelFilter();
        int nextSuggestedColorId = TabGroupColorUtils.getNextSuggestedColorId(filter);

        // Merge first two tabs into a group.
        TabModel normalTabModel = cta.getTabModelSelector().getModel(false);
        List<Tab> tabGroup =
                new ArrayList<>(
                        Arrays.asList(normalTabModel.getTabAt(0), normalTabModel.getTabAt(1)));
        createTabGroup(cta, false, tabGroup);
        int[] ungroupedRootId = new int[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    filter.setTabGroupTitle(normalTabModel.getTabAt(0).getRootId(), "Foo");
                    ungroupedRootId[0] = normalTabModel.getTabAt(2).getRootId();
                    filter.setTabGroupTitle(ungroupedRootId[0], "Bar");
                });
        verifyTabSwitcherCardCount(cta, 2);

        // Assert default color was set properly.
        assertEquals(
                nextSuggestedColorId,
                filter.getTabGroupColor(normalTabModel.getTabAt(1).getRootId()));

        // Merge tab group of 2 at first index with the 3rd tab.
        mergeAllNormalTabsToAGroup(cta);
        assertTrue(
                snackbarManager.getCurrentSnackbarForTesting().getController()
                        instanceof UndoGroupSnackbarController);

        // Assert default color was set properly for the overall merged group.
        assertEquals(
                nextSuggestedColorId,
                filter.getTabGroupColor(normalTabModel.getTabAt(2).getRootId()));

        // Check that the old group title was handed over when the group merge is committed
        // and no longer exists.
        ThreadUtils.runOnUiThreadBlocking(() -> snackbarManager.dismissAllSnackbars());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertNull(filter.getTabGroupTitle(ungroupedRootId[0]));
                    assertEquals(
                            "Foo", filter.getTabGroupTitle(normalTabModel.getTabAt(0).getRootId()));
                });

        // Assert color still exists post snackbar dismissal.
        assertEquals(
                nextSuggestedColorId,
                filter.getTabGroupColor(normalTabModel.getTabAt(1).getRootId()));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
    public void testUndoClosure_UndoGroupClosure() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        SnackbarManager snackbarManager = mActivityTestRule.getActivity().getSnackbarManager();
        TabGroupModelFilter filter = getTabGroupModelFilter();
        createTabs(cta, false, 2);

        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        assertNull(snackbarManager.getCurrentSnackbarForTesting());

        // Get the next suggested color id.
        int nextSuggestedColorId =
                TabGroupColorUtils.getNextSuggestedColorId(
                        (TabGroupModelFilter)
                                cta.getTabModelSelectorSupplier()
                                        .get()
                                        .getTabModelFilterProvider()
                                        .getCurrentTabModelFilter());

        // Merge first two tabs into a group.
        TabModel normalTabModel = cta.getTabModelSelector().getModel(false);
        List<Tab> tabGroup =
                new ArrayList<>(
                        Arrays.asList(normalTabModel.getTabAt(0), normalTabModel.getTabAt(1)));
        createTabGroup(cta, false, tabGroup);
        verifyTabSwitcherCardCount(cta, 1);

        // Assert default color was set properly.
        assertEquals(
                nextSuggestedColorId,
                filter.getTabGroupColor(normalTabModel.getTabAt(1).getRootId()));
        ThreadUtils.runOnUiThreadBlocking(() -> snackbarManager.dismissAllSnackbars());

        // Temporarily save the tab to get the rootId later.
        Tab tab2 = normalTabModel.getTabAt(1);

        closeFirstTabGroupInTabSwitcher(cta);
        assertTrue(
                snackbarManager.getCurrentSnackbarForTesting().getController()
                        instanceof UndoBarController);
        verifyTabSwitcherCardCount(cta, 0);

        // Default color should still persist, though the root id might change.
        assertEquals(nextSuggestedColorId, filter.getTabGroupColor(tab2.getRootId()));

        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        verifyTabSwitcherCardCount(cta, 1);

        // Assert default color still persists.
        assertEquals(
                nextSuggestedColorId,
                filter.getTabGroupColor(normalTabModel.getTabAt(1).getRootId()));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
    public void testUndoClosure_AcceptGroupClosure() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        SnackbarManager snackbarManager = mActivityTestRule.getActivity().getSnackbarManager();
        TabGroupModelFilter filter = getTabGroupModelFilter();
        createTabs(cta, false, 2);

        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        assertNull(snackbarManager.getCurrentSnackbarForTesting());

        // Get the next suggested color id.
        int nextSuggestedColorId =
                TabGroupColorUtils.getNextSuggestedColorId(
                        (TabGroupModelFilter)
                                cta.getTabModelSelectorSupplier()
                                        .get()
                                        .getTabModelFilterProvider()
                                        .getCurrentTabModelFilter());

        // Merge first two tabs into a group.
        TabModel normalTabModel = cta.getTabModelSelector().getModel(false);
        List<Tab> tabGroup =
                new ArrayList<>(
                        Arrays.asList(normalTabModel.getTabAt(0), normalTabModel.getTabAt(1)));
        createTabGroup(cta, false, tabGroup);
        verifyTabSwitcherCardCount(cta, 1);

        // Assert default color was set properly.
        assertEquals(
                nextSuggestedColorId,
                filter.getTabGroupColor(normalTabModel.getTabAt(1).getRootId()));
        ThreadUtils.runOnUiThreadBlocking(() -> snackbarManager.dismissAllSnackbars());

        // Temporarily save the rootID to check during closure.
        Tab tab2 = normalTabModel.getTabAt(1);
        int groupRootId = tab2.getRootId();

        closeFirstTabGroupInTabSwitcher(cta);
        assertTrue(
                snackbarManager.getCurrentSnackbarForTesting().getController()
                        instanceof UndoBarController);
        verifyTabSwitcherCardCount(cta, 0);

        // Default color should still persist, though the root id might change.
        assertEquals(nextSuggestedColorId, filter.getTabGroupColor(tab2.getRootId()));

        ThreadUtils.runOnUiThreadBlocking(() -> snackbarManager.dismissAllSnackbars());

        // Assert default color is cleared.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            TabGroupColorUtils.INVALID_COLOR_ID,
                            filter.getTabGroupColor(groupRootId));
                    assertEquals(
                            TabGroupColorUtils.INVALID_COLOR_ID,
                            filter.getTabGroupColor(tab2.getRootId()));
                });
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ANDROID_HUB_SEARCH)
    public void testSearchClickedOpensSearchActivity() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);

        onView(withId(R.id.search_box_text)).perform(click());
        ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SearchActivity.class);
    }

    private void enterTabListEditor(ChromeTabbedActivity cta) {
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(), cta, R.id.menu_select_tabs);
    }

    private void simulateJpegHasCachedWithAspectRatio(double aspectRatio) throws IOException {
        TabModel currentModel = mActivityTestRule.getActivity().getCurrentTabModel();
        int jpegWidth = 125;
        int jpegHeight = (int) (jpegWidth * 1.0 / aspectRatio);
        for (int i = 0; i < currentModel.getCount(); i++) {
            Tab tab = currentModel.getTabAt(i);
            Bitmap bitmap = Bitmap.createBitmap(jpegWidth, jpegHeight, Config.ARGB_8888);
            encodeJpeg(tab, bitmap);
        }
    }

    private void simulateJpegHasCachedWithDefaultAspectRatio() throws IOException {
        simulateJpegHasCachedWithAspectRatio(
                TabUtils.getTabThumbnailAspectRatio(
                        mActivityTestRule.getActivity(),
                        mActivityTestRule.getActivity().getBrowserControlsManager()));
    }

    private void encodeJpeg(Tab tab, Bitmap bitmap) throws IOException {
        FileOutputStream outputStream =
                new FileOutputStream(TabContentManager.getTabThumbnailFileJpeg(tab.getId()));
        bitmap.compress(Bitmap.CompressFormat.JPEG, 50, outputStream);
        outputStream.close();
    }

    private Matcher<View> tabSwitcherViewMatcher() {
        return allOf(
                isDescendantOfA(
                        withId(
                                TabUiTestHelper.getTabSwitcherAncestorId(
                                        mActivityTestRule.getActivity()))),
                withId(R.id.tab_list_recycler_view));
    }

    private void mergeNormalTabsToGroupWithDialog(ChromeTabbedActivity cta, int tabCount) {
        TabListEditorTestingRobot robot = new TabListEditorTestingRobot();
        enterTabListEditor(cta);
        robot.resultRobot.verifyTabListEditorIsVisible();

        for (int i = 0; i < tabCount; i++) {
            robot.actionRobot.clickItemAtAdapterPosition(i);
        }
        robot.actionRobot
                .clickToolbarMenuButton()
                .clickToolbarMenuItem(tabCount == 1 ? "Group tab" : "Group tabs");
        robot.resultRobot.verifyTabListEditorIsHidden();
    }

    private void verifyGroupVisualDataDialogOpenedAndDismiss(ChromeTabbedActivity cta) {
        // Verify that the modal dialog is now showing.
        verifyModalDialogShowingAnimationCompleteInTabSwitcher();
        // Verify the visual data dialog exists.
        onViewWaiting(withId(R.id.visual_data_dialog_layout), /* checkRootDialog= */ true)
                .check(matches(isDisplayed()));
        // Wait until the keyboard is showing.
        KeyboardVisibilityDelegate delegate = cta.getWindowAndroid().getKeyboardDelegate();
        CriteriaHelper.pollUiThread(
                () -> delegate.isKeyboardShowing(cta, cta.getCompositorViewHolderForTesting()));
        // Dismiss the tab group visual data dialog.
        dismissAllModalDialogs();
        // Verify that the modal dialog is now hidden.
        verifyModalDialogHidingAnimationCompleteInTabSwitcher();
    }

    private void editGroupVisualDataDialogTitle(ChromeTabbedActivity cta, String title) {
        onView(withId(R.id.title_input_text))
                .perform(click())
                .perform(replaceText(title))
                .perform(pressImeActionButton());
        // Wait until the keyboard is hidden to make sure the edit has taken effect.
        KeyboardVisibilityDelegate delegate = cta.getWindowAndroid().getKeyboardDelegate();
        CriteriaHelper.pollUiThread(
                () -> !delegate.isKeyboardShowing(cta, cta.getCompositorViewHolderForTesting()));
    }

    private void verifyFirstCardTitle(String title) {
        onView(
                        allOf(
                                isDescendantOfA(
                                        withId(
                                                TabUiTestHelper.getTabSwitcherAncestorId(
                                                        mActivityTestRule.getActivity()))),
                                withId(R.id.tab_list_recycler_view)))
                .check(
                        (v, noMatchException) -> {
                            if (noMatchException != null) throw noMatchException;

                            RecyclerView recyclerView = (RecyclerView) v;
                            TextView firstCardTitleTextView =
                                    recyclerView
                                            .findViewHolderForAdapterPosition(0)
                                            .itemView
                                            .findViewById(R.id.tab_title);
                            assertEquals(title, firstCardTitleTextView.getText().toString());
                        });
    }

    private void verifyFirstCardColor(@TabGroupColorId int color) {
        onView(allOf(withId(R.id.tab_favicon), withParent(withId(R.id.card_view))))
                .check(
                        (v, noMatchException) -> {
                            if (noMatchException != null) throw noMatchException;

                            ImageView imageView = (ImageView) v;
                            LayerDrawable layerDrawable = (LayerDrawable) imageView.getDrawable();
                            GradientDrawable drawable =
                                    (GradientDrawable) layerDrawable.getDrawable(1);

                            assertEquals(
                                    ColorStateList.valueOf(
                                            ColorPickerUtils.getTabGroupColorPickerItemColor(
                                                    mActivityTestRule.getActivity(), color, false)),
                                    drawable.getColor());
                        });
    }

    private void verifyModalDialogShowingAnimationCompleteInTabSwitcher() {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mModalDialogManager.isShowing(), Matchers.is(true));
                });
    }

    private void verifyModalDialogHidingAnimationCompleteInTabSwitcher() {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mModalDialogManager.isShowing(), Matchers.is(false));
                });
    }

    private void dismissAllModalDialogs() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModalDialogManager.dismissAllDialogs(DialogDismissalCause.UNKNOWN);
                });
    }

    private void verifyItemSelectedAtPosition(int position) {
        onView(withId(R.id.tab_list_recycler_view))
                .check(
                        matches(
                                RecyclerViewMatcherUtils.atPosition(
                                        position,
                                        new BoundedMatcher<View, TabGridView>(TabGridView.class) {

                                            @Override
                                            protected boolean matchesSafely(
                                                    TabGridView selectableTabGridView) {

                                                return TabUiTestHelper.isTabViewSelected(
                                                        selectableTabGridView);
                                            }

                                            @Override
                                            public void describeTo(Description description) {
                                                description.appendText(
                                                        "has selected view at position "
                                                                + position);
                                            }
                                        })));
    }

    private TabGroupModelFilter getTabGroupModelFilter() {
        return (TabGroupModelFilter)
                mActivityTestRule
                        .getActivity()
                        .getTabModelSelectorSupplier()
                        .get()
                        .getTabModelFilterProvider()
                        .getCurrentTabModelFilter();
    }
}
