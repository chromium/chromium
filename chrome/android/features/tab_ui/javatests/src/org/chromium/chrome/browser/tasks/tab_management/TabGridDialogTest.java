// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.view.View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.longClick;
import static androidx.test.espresso.action.ViewActions.pressImeActionButton;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.matcher.BundleMatchers.hasEntry;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasExtras;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasType;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.isClickable;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.Matchers.notNullValue;
import static org.hamcrest.core.IsEqual.equalTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.TAB_GROUPS_ANDROID;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.TAB_GROUPS_FOR_TABLETS;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.TAB_STRIP_IMPROVEMENTS;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickFirstCardFromTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickFirstTabInDialog;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickNthTabInDialog;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.closeFirstTabInDialog;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.closeNthTabInDialog;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.createTabs;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.finishActivity;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.getSwipeToDismissAction;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.getTabSwitcherParentId;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.mergeAllNormalTabsToAGroup;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.prepareTabsWithThumbnail;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyAllTabsHaveThumbnail;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabStripFaviconCount;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabSwitcherCardCount;
import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.START_SURFACE_TEST_SINGLE_ENABLED_PARAMS;
import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.createTabStateFile;
import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;
import static org.chromium.ui.test.util.ViewUtils.waitForView;

import android.content.Intent;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Rect;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.support.test.InstrumentationRegistry;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.Espresso;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.espresso.intent.Intents;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkAddEditFolderActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkEditActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.homepage.HomepagePolicyManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.tasks.pseudotab.TabAttributeCache;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.UiDisableIf;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.concurrent.ExecutionException;

/** End-to-end tests for TabGridDialog component. */
// clang-format off
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction({Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@Features.EnableFeatures({TAB_GRID_LAYOUT_ANDROID, TAB_GROUPS_ANDROID,
    TAB_GROUPS_FOR_TABLETS, TAB_STRIP_IMPROVEMENTS})
@DoNotBatch(reason = "crbug.com/1380489")
public class TabGridDialogTest {
    // clang-format on
    private static final String CUSTOMIZED_TITLE1 = "wfh tips";
    private static final String CUSTOMIZED_TITLE2 = "wfh funs";
    private static final String PAGE_WITH_HTTPS_CANONICAL_URL =
            "/chrome/test/data/android/share/link_share_https_canonical.html";
    private static final String PAGE_WITH_HTTP_CANONICAL_URL =
            "/chrome/test/data/android/share/link_share_http_canonical.html";

    private boolean mHasReceivedSourceRect;
    private TabSelectionEditorTestingRobot mSelectionEditorRobot =
            new TabSelectionEditorTestingRobot();

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(
                            ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_TAB_SWITCHER_GRID)
                    .build();

    @Mock
    private HomepagePolicyManager mHomepagePolicyManager;

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        ChromeNightModeTestUtils.setUpNightModeBeforeChromeActivityLaunched();
    }

    @ParameterAnnotations.UseMethodParameterBefore(NightModeTestUtils.NightModeParams.class)
    public void setupNightMode(boolean nightModeEnabled) {
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Intents.init();
        TabUiFeatureUtilities.setTabManagementModuleSupportedForTesting(true);
        mActivityTestRule.startMainActivityOnBlankPage();
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);
    }

    @After
    public void tearDown() {
        TabSelectionEditorShareAction.setIntentCallbackForTesting(null);
        TabUiFeatureUtilities.setTabManagementModuleSupportedForTesting(null);
        ActivityTestUtils.clearActivityOrientation(mActivityTestRule.getActivity());
        Intents.release();
    }

    @Test
    @MediumTest
    public void testBackPressCloseDialog() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        // Open dialog from tab switcher and verify dialog is showing correct content.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);

        // Press back and dialog should be hidden.
        Espresso.pressBack();
        waitForDialogHidingAnimationInTabSwitcher(cta);

        verifyTabSwitcherCardCount(cta, 1);

        // Enter first tab page.
        assertTrue(cta.getLayoutManager().isLayoutVisible(LayoutType.TAB_SWITCHER));
        clickFirstCardFromTabSwitcher(cta);
        clickFirstTabInDialog(cta);
        waitForDialogHidingAnimation(cta);

        if (isPhone()) {
            // Open dialog from tab strip and verify dialog is showing correct content.
            openDialogFromStripAndVerify(cta, 2, null);

            // Press back and dialog should be hidden.
            Espresso.pressBack();
            waitForDialogHidingAnimation(cta);
        }
    }

    @Test
    @MediumTest
    public void testBackPressCloseDialogViaGroupStrip() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
    }

    @Test
    @MediumTest
    public void testClickScrimCloseDialog() throws ExecutionException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        // Open dialog from tab switcher and verify dialog is showing correct content.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);

        // Click scrim view and dialog should be hidden.
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimationInTabSwitcher(cta);

        verifyTabSwitcherCardCount(cta, 1);

        // Enter first tab page.
        assertTrue(cta.getLayoutManager().isLayoutVisible(LayoutType.TAB_SWITCHER));
        clickFirstCardFromTabSwitcher(cta);
        clickFirstTabInDialog(cta);
        waitForDialogHidingAnimation(cta);

        if (isPhone()) {
            // Open dialog from tab strip and verify dialog is showing correct content.
            openDialogFromStripAndVerify(cta, 2, null);

            // Click scrim view and dialog should be hidden.
            clickScrimToExitDialog(cta);
            waitForDialogHidingAnimation(cta);
        }

        // Checkout the scrim view observer is correctly setup by closing dialog in tab switcher
        // again.
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimationInTabSwitcher(cta);
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.TAB_SELECTION_EDITOR_V2})
    public void testDisableTabGroupsContinuation() {
        // TabSelectionEditorV2 enables the menu, but not edit text for this test
        // so needs to be disabled.

        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open dialog and verify dialog is showing correct content.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);

        // Verify TabGroupsContinuation related functionality is not exposed.
        verifyTabGroupsContinuation(cta, false);
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.TAB_SELECTION_EDITOR_V2})
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void testEnableTabGroupsContinuation() {
        // TabSelectionEditorV2 enables the menu. Ensure Tab Groups Continuation also enables the
        // menu independently.

        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open dialog and verify dialog is showing correct content.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);

        // Verify TabGroupsContinuation related functionality is exposed.
        verifyTabGroupsContinuation(cta, true);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "Flaky test - see: https://crbug.com/1177149")
    public void testTabGridDialogAnimation() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Add 400px top margin to the recyclerView.
        RecyclerView recyclerView = cta.findViewById(R.id.tab_list_view);
        float tabGridCardPadding = TabUiThemeProvider.getTabGridCardMargin(cta);
        int deltaTopMargin = 400;
        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams) recyclerView.getLayoutParams();
        params.topMargin += deltaTopMargin;
        TestThreadUtils.runOnUiThreadBlocking(() -> recyclerView.setLayoutParams(params));
        CriteriaHelper.pollUiThread(() -> !recyclerView.isComputingLayout());

        // Calculate expected values of animation source rect.
        mHasReceivedSourceRect = false;
        View parentView = cta.getCompositorViewHolderForTesting();
        Rect parentRect = new Rect();
        parentView.getGlobalVisibleRect(parentRect);
        Rect sourceRect = new Rect();
        recyclerView.getChildAt(0).getGlobalVisibleRect(sourceRect);
        // TODO(yuezhanggg): Figure out why the sourceRect.left is wrong after setting the margin.
        float expectedTop = sourceRect.top - parentRect.top + tabGridCardPadding;
        float expectedWidth = sourceRect.width() - 2 * tabGridCardPadding;
        float expectedHeight = sourceRect.height() - 2 * tabGridCardPadding;

        // Setup the callback to verify the animation source Rect.
        TabGridDialogView.setSourceRectCallbackForTesting((result -> {
            mHasReceivedSourceRect = true;
            assertEquals(expectedTop, result.top, 0.0);
            assertEquals(expectedHeight, result.height(), 0.0);
            assertEquals(expectedWidth, result.width(), 0.0);
        }));

        TabUiTestHelper.clickFirstCardFromTabSwitcher(cta);
        CriteriaHelper.pollUiThread(() -> mHasReceivedSourceRect);
        CriteriaHelper.pollUiThread(() -> isDialogShowing(cta));
    }

    @Test
    @MediumTest
    public void testUndoClosureInDialog_DialogUndoBar() throws ExecutionException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        openDialogFromTabSwitcherAndVerify(cta, 2, null);

        // Verify close and undo in dialog from tab switcher.
        closeFirstTabInDialog();
        verifyShowingDialog(cta, 1, null);
        verifyDialogUndoBarAndClick();
        verifyShowingDialog(cta, 2, null);

        if (isPhone()) {
            // Verify close and undo in dialog from tab strip.
            clickFirstTabInDialog(cta);
            openDialogFromStripAndVerify(cta, 2, null);
            closeFirstTabInDialog();
            verifyShowingDialog(cta, 1, null);
            verifyDialogUndoBarAndClick();
            verifyShowingDialog(cta, 2, null);
            clickScrimToExitDialog(cta);
            verifyTabStripFaviconCount(cta, 2);
        }
    }

    @Test
    @MediumTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:enable_launch_polish/true"})
    public void testSelectionEditorShowHide() throws ExecutionException {
        // clang-format on
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open dialog and open selection editor.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        openSelectionEditorV2AndVerify(cta, 2);

        // Click navigation button should close selection editor but not tab grid dialog.
        mSelectionEditorRobot.actionRobot.clickToolbarNavigationButton();
        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        assertTrue(isDialogShowing(cta));

        // Back press should close both the dialog and selection editor.
        openSelectionEditorV2AndVerify(cta, 2);
        Espresso.pressBack();
        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        waitForDialogHidingAnimationInTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Clicking ScrimView should close both the dialog and selection editor.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        openSelectionEditorV2AndVerify(cta, 2);
        clickScrimToExitDialog(cta);
        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        waitForDialogHidingAnimationInTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);
    }

    @Test
    @MediumTest
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_SELECTION_EDITOR_V2})
    public void testDialogToolbarSelectionEditorV2() throws ExecutionException {
        // clang-format on
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open dialog and open selection editor and confirm the share action isn't visible.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        openDialogToolbarMenuAndVerify(cta);
        onView(withText("Share group"))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .check(doesNotExist());
        Espresso.pressBack();
        openSelectionEditorV2AndVerify(cta, 2);

        // Click navigation button should close selection editor but not tab grid dialog.
        mSelectionEditorRobot.actionRobot.clickToolbarNavigationButton();
        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        assertTrue(isDialogShowing(cta));

        // Back press should close both the dialog and selection editor.
        openSelectionEditorV2AndVerify(cta, 2);
        Espresso.pressBack();
        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        waitForDialogHidingAnimationInTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Clicking ScrimView should close both the dialog and selection editor.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        openSelectionEditorV2AndVerify(cta, 2);
        clickScrimToExitDialog(cta);
        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        waitForDialogHidingAnimationInTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.TAB_SELECTION_EDITOR_V2 + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:enable_longpress_entrypoint/true"})
    public void
    testDialogSelectionEditorV2_LongPressTabAndVerifyNoSelectionOccurs() throws ExecutionException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open the selection editor with longpress.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        onView(allOf(withId(R.id.tab_list_view), withParent(withId(R.id.dialog_container_view))))
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, longClick()));

        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsVisible();
        // Verify no selection action occurred to switch the selected tab in the tab model
        Criteria.checkThat(
                mActivityTestRule.getActivity().getCurrentTabModel().index(), Matchers.is(1));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.TAB_SELECTION_EDITOR_V2 + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:enable_longpress_entrypoint/true"})
    public void
    testDialogSelectionEditorV2_PostLongPressClickNoSelectionEditor() throws ExecutionException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open the selection editor with longpress.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        onView(allOf(withId(R.id.tab_list_view), withParent(withId(R.id.dialog_container_view))))
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, longClick()));

        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsVisible();
        Espresso.pressBack();

        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        clickFirstTabInDialog(cta);
        waitForDialogHidingAnimation(cta);

        // Make sure tab switcher strip (and by extension a tab page) is showing to verify clicking
        // the tab worked.
        CriteriaHelper.pollUiThread(()
                                            -> mActivityTestRule.getActivity()
                                                       .getBrowserControlsManager()
                                                       .getBottomControlOffset()
                        == 0);
        waitForView(allOf(withId(R.id.toolbar_left_button), isCompletelyDisplayed()));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.TAB_SELECTION_EDITOR_V2 + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:enable_bookmarks/true"})
    public void
    testDialogSelectionEditorV2_BookmarkSingleTabView() throws ExecutionException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        SnackbarManager snackbarManager = cta.getSnackbarManager();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        TestThreadUtils.runOnUiThreadBlocking(() -> snackbarManager.dismissAllSnackbars());
        verifyTabSwitcherCardCount(cta, 1);

        // Open the selection editor.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        openSelectionEditorV2AndVerify(cta, 2);

        // Bookmark one tab and verify edit snackbar.
        mSelectionEditorRobot.actionRobot.clickItemAtAdapterPosition(0)
                .clickToolbarMenuButton()
                .clickToolbarMenuItem("Bookmark tab");

        onViewWaiting(allOf(withId(R.id.snackbar_button),
                isDescendantOfA(withId(R.id.selectable_list)), isDisplayed()));
        onView(allOf(withId(R.id.snackbar), isDescendantOfA(withId(R.id.bottom_container))))
                .check(doesNotExist());
        onView(allOf(withId(R.id.snackbar_button), isDescendantOfA(withId(R.id.selectable_list)),
                       isDisplayed()))
                .perform(click());

        BookmarkEditActivity activity = BookmarkTestUtil.waitForEditActivity();
        activity.finish();

        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsVisible();
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.TAB_SELECTION_EDITOR_V2 + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
            "force-fieldtrial-params=Study.Group:enable_bookmarks/true"})
    public void
    testDialogSelectionEditorV2_BookmarkTabsView() throws ExecutionException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        SnackbarManager snackbarManager = cta.getSnackbarManager();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        TestThreadUtils.runOnUiThreadBlocking(() -> snackbarManager.dismissAllSnackbars());
        verifyTabSwitcherCardCount(cta, 1);

        // Open the selection editor.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        openSelectionEditorV2AndVerify(cta, 2);

        // Bookmark two tabs and verify edit snackbar.
        mSelectionEditorRobot.actionRobot.clickItemAtAdapterPosition(0)
                .clickItemAtAdapterPosition(1)
                .clickToolbarMenuButton()
                .clickToolbarMenuItem("Bookmark tabs");

        onViewWaiting(allOf(withId(R.id.snackbar_button),
                isDescendantOfA(withId(R.id.selectable_list)), isDisplayed()));
        onView(allOf(withId(R.id.snackbar), isDescendantOfA(withId(R.id.bottom_container))))
                .check(doesNotExist());
        onView(allOf(withId(R.id.snackbar_button), isDescendantOfA(withId(R.id.selectable_list)),
                       isDisplayed()))
                .perform(click());

        BookmarkAddEditFolderActivity activity = BookmarkTestUtil.waitForAddEditFolderActivity();
        activity.finish();

        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsVisible();
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.TAB_SELECTION_EDITOR_V2 + "<Study"})
    @CommandLineFlags.
    Add({"force-fieldtrials=Study/Group", "force-fieldtrial-params=Study.Group:enable_share/true"})
    public void testDialogSelectionEditorV2_ShareActionView() throws ExecutionException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);

        final String httpsCanonicalUrl =
                mActivityTestRule.getTestServer().getURL(PAGE_WITH_HTTPS_CANONICAL_URL);
        mActivityTestRule.loadUrl(httpsCanonicalUrl);

        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open dialog and verify dialog is showing correct content.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        openSelectionEditorV2AndVerify(cta, 2);

        // Share tabs
        mSelectionEditorRobot.actionRobot.clickItemAtAdapterPosition(1)
                .clickToolbarMenuButton()
                .clickToolbarMenuItem("Share tab");

        CriteriaHelper.pollUiThread(()
                                            -> Criteria.checkThat("Share sheet was not shown.",
                                                    mActivityTestRule.getActivity()
                                                            .getRootUiCoordinatorForTesting()
                                                            .getBottomSheetController(),
                                                    notNullValue()));

        intended(allOf(hasAction(equalTo(Intent.ACTION_CHOOSER)),
                hasExtras(hasEntry(equalTo(Intent.EXTRA_INTENT),
                        allOf(hasAction(equalTo(Intent.ACTION_SEND)), hasType("text/plain"))))));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_SELECTION_EDITOR_V2 + "<Study"})
    @CommandLineFlags.
    Add({"force-fieldtrials=Study/Group", "force-fieldtrial-params=Study.Group:enable_share/true"})
    public void testDialogSelectionEditorV2_ShareActionTabs() throws ExecutionException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        final String httpsCanonicalUrl =
                mActivityTestRule.getTestServer().getURL(PAGE_WITH_HTTPS_CANONICAL_URL);
        mActivityTestRule.loadUrlInNewTab(httpsCanonicalUrl);

        final String httpCanonicalUrl =
                mActivityTestRule.getTestServer().getURL(PAGE_WITH_HTTP_CANONICAL_URL);
        mActivityTestRule.loadUrlInNewTab(httpCanonicalUrl);

        ArrayList<String> urls = new ArrayList<String>();
        urls.add(httpsCanonicalUrl);
        urls.add(httpCanonicalUrl);

        for (int i = 0; i < urls.size(); i++) {
            urls.set(i, (i + 1) + ". " + urls.get(i));
        }
        urls.add("");

        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open dialog and verify dialog is showing correct content.
        openDialogFromTabSwitcherAndVerify(cta, 3, null);
        openSelectionEditorV2AndVerify(cta, 3);

        TabSelectionEditorShareAction.setIntentCallbackForTesting((result -> {
            assertEquals(Intent.ACTION_SEND, result.getAction());
            assertEquals(String.join("\n", urls), result.getStringExtra(Intent.EXTRA_TEXT));
            assertEquals("text/plain", result.getType());
            assertEquals("2 links from Chrome", result.getStringExtra(Intent.EXTRA_TITLE));
        }));

        // Share tabs
        mSelectionEditorRobot.actionRobot.clickItemAtAdapterPosition(1)
                .clickItemAtAdapterPosition(2)
                .clickToolbarMenuButton()
                .clickToolbarMenuItem("Share tabs");
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TAB_SELECTION_EDITOR_V2 + "<Study"})
    @CommandLineFlags.
    Add({"force-fieldtrials=Study/Group", "force-fieldtrial-params=Study.Group:enable_share/true"})
    public void testDialogSelectionEditorV2_ShareActionAllFilterableTabs()
            throws ExecutionException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);

        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open dialog and verify dialog is showing correct content.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        openSelectionEditorV2AndVerify(cta, 2);

        mSelectionEditorRobot.actionRobot.clickItemAtAdapterPosition(0)
                .clickItemAtAdapterPosition(1)
                .clickToolbarMenuButton();

        // Check share tabs disabled
        onView(withText("Share tabs")).check(matches(not(isClickable())));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.TAB_SELECTION_EDITOR_V2})
    public void testDialogSelectionEditorV2_UndoClose() throws ExecutionException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 4);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 4);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open the selection editor.
        openDialogFromTabSwitcherAndVerify(cta, 4, null);
        openSelectionEditorV2AndVerify(cta, 4);

        // Close two tabs and undo.
        mSelectionEditorRobot.actionRobot.clickItemAtAdapterPosition(0)
                .clickItemAtAdapterPosition(2)
                .clickToolbarMenuButton()
                .clickToolbarMenuItem("Close tabs");
        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        verifyShowingDialog(cta, 2, null);
        verifyDialogUndoBarAndClick();
        verifyShowingDialog(cta, 4, null);

        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimationInTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.TAB_SELECTION_EDITOR_V2})
    public void testDialogSelectionEditorV2_UndoCloseAll() throws ExecutionException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 4);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 4);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open the selection editor.
        openDialogFromTabSwitcherAndVerify(cta, 4, null);
        openSelectionEditorV2AndVerify(cta, 4);

        // Close two tabs and undo.
        mSelectionEditorRobot.actionRobot.clickItemAtAdapterPosition(0)
                .clickItemAtAdapterPosition(1)
                .clickItemAtAdapterPosition(2)
                .clickItemAtAdapterPosition(3)
                .clickToolbarMenuButton()
                .clickToolbarMenuItem("Close tabs");
        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        waitForDialogHidingAnimationInTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 0);

        verifyGlobalUndoBarAndClick();
        verifyTabSwitcherCardCount(cta, 1);
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.TAB_SELECTION_EDITOR_V2})
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void testSelectionEditorUngroup() throws ExecutionException {
        // TabSelectionEditorV2 replaces this behavior.

        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        assertTrue(cta.getTabModelSelector().getTabModelFilterProvider().getCurrentTabModelFilter()
                           instanceof TabGroupModelFilter);
        final TabGroupModelFilter filter = (TabGroupModelFilter) cta.getTabModelSelector()
                                                   .getTabModelFilterProvider()
                                                   .getCurrentTabModelFilter();
        createTabs(cta, false, 3);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        assertEquals(1, filter.getCount());

        // Open dialog and open selection editor.
        openDialogFromTabSwitcherAndVerify(cta, 3, null);
        openSelectionEditorAndVerify(cta, 3);

        // Select and ungroup the first tab.
        mSelectionEditorRobot.actionRobot.clickItemAtAdapterPosition(0);
        mSelectionEditorRobot.resultRobot.verifyItemSelectedAtAdapterPosition(0)
                .verifyToolbarActionButtonEnabled()
                .verifyToolbarSelectionText("1 selected");

        mSelectionEditorRobot.actionRobot.clickToolbarActionButton();
        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        verifyShowingDialog(cta, 2, null);
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimationInTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        assertEquals(2, filter.getCount());

        // Open dialog and open selection editor.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        openSelectionEditorAndVerify(cta, 2);

        // Select and ungroup all two tabs in dialog.
        mSelectionEditorRobot.actionRobot.clickItemAtAdapterPosition(0).clickItemAtAdapterPosition(
                1);
        mSelectionEditorRobot.resultRobot.verifyItemSelectedAtAdapterPosition(0)
                .verifyItemSelectedAtAdapterPosition(1)
                .verifyToolbarActionButtonEnabled()
                .verifyToolbarSelectionText("2 selected");

        mSelectionEditorRobot.actionRobot.clickToolbarActionButton();
        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        waitForDialogHidingAnimationInTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);
        assertEquals(3, filter.getCount());
    }

    @Test
    @MediumTest
    public void testSwipeToDismiss_Dialog() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        // Create 2 tabs and merge them into one group.
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        openDialogFromTabSwitcherAndVerify(cta, 2, null);

        // Swipe to dismiss two tabs in dialog.
        onView(allOf(withId(R.id.tab_list_view), withParent(withId(R.id.dialog_container_view))))
                .perform(RecyclerViewActions.actionOnItemAtPosition(
                        1, getSwipeToDismissAction(true)));
        verifyShowingDialog(cta, 1, null);
        onView(allOf(withId(R.id.tab_list_view), withParent(withId(R.id.dialog_container_view))))
                .perform(RecyclerViewActions.actionOnItemAtPosition(
                        0, getSwipeToDismissAction(false)));
        waitForDialogHidingAnimation(cta);
        verifyTabSwitcherCardCount(cta, 0);
    }

    @Test
    @MediumTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID + "<Study"})
    // TODO(crbug.com/1380489): Re-enable once test is no longer flaky locally.
    @Features.DisableFeatures({ContentFeatures.SURFACE_SYNC_FULLSCREEN_KILLSWITCH})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:enable_launch_polish/true"})
    public void testSelectionEditorPosition() {
        // clang-format on;
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        // Position in portrait mode.
        ActivityTestUtils.rotateActivityToOrientation(cta, Configuration.ORIENTATION_PORTRAIT);

        View parentView = cta.getCompositorViewHolderForTesting();
        createTabs(cta, false, 3);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Verify the size and position of TabGridDialog in portrait mode.
        openDialogFromTabSwitcherAndVerify(cta, 3, null);
        checkPosition(cta, true, true);

        // Verify the size and position of TabSelectionEditor in portrait mode.
        openSelectionEditorV2AndVerify(cta, 3);
        checkPosition(cta, false, true);

        // Verify the size and position of TabSelectionEditor in landscape mode.
        ActivityTestUtils.rotateActivityToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);
        CriteriaHelper.pollUiThread(() -> parentView.getHeight() < parentView.getWidth());
        checkPosition(cta, false, false);

        // Verify the size and position of TabGridDialog in landscape mode.
        mSelectionEditorRobot.actionRobot.clickToolbarNavigationButton();
        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        assertTrue(isDialogShowing(cta));
        checkPosition(cta, true, false);

        // Verify the positioning in multi-window mode. Adjusting the height of the root view to
        // mock entering/exiting multi-window mode.
        ActivityTestUtils.rotateActivityToOrientation(cta, Configuration.ORIENTATION_PORTRAIT);
        CriteriaHelper.pollUiThread(() -> parentView.getHeight() > parentView.getWidth());
        View rootView = cta.findViewById(R.id.coordinator);
        int rootViewHeight = rootView.getHeight();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ViewGroup.LayoutParams params = rootView.getLayoutParams();
            params.height = rootViewHeight / 2;
            rootView.setLayoutParams(params);
        });
        checkPosition(cta, true, true);
        openSelectionEditorV2AndVerify(cta, 3);
        checkPosition(cta, false, true);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ViewGroup.LayoutParams params = rootView.getLayoutParams();
            params.height = rootViewHeight;
            rootView.setLayoutParams(params);
        });
        checkPosition(cta, false, true);
        checkPosition(cta, true, true);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void testTabGroupNaming() throws ExecutionException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open dialog and modify group title.
        openDialogFromTabSwitcherAndVerify(cta, 2,
                cta.getResources().getQuantityString(
                        R.plurals.bottom_tab_grid_title_placeholder, 2, 2));
        editDialogTitle(cta, CUSTOMIZED_TITLE1);

        // Verify the title is updated in both tab switcher and dialog.
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimation(cta);
        verifyFirstCardTitle(CUSTOMIZED_TITLE1);
        openDialogFromTabSwitcherAndVerify(cta, 2, CUSTOMIZED_TITLE1);

        if(isPhone()) {
            // Modify title in dialog from tab strip.
            clickFirstTabInDialog(cta);
            openDialogFromStripAndVerify(cta, 2, CUSTOMIZED_TITLE1);
            editDialogTitle(cta, CUSTOMIZED_TITLE2);

            clickScrimToExitDialog(cta);
            waitForDialogHidingAnimation(cta);
            enterTabSwitcher(cta);
            verifyFirstCardTitle(CUSTOMIZED_TITLE2);
        }
    }

    @Test
    @MediumTest
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testTabGroupNaming_KeyboardVisibility() throws ExecutionException {
        // clang-format on
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        openDialogFromTabSwitcherAndVerify(cta, 2,
                cta.getResources().getQuantityString(
                        R.plurals.bottom_tab_grid_title_placeholder, 2, 2));

        // Test title text focus in dialog in tab switcher.
        testTitleTextFocus(cta);

        // Test title text focus in dialog from tab strip.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        clickFirstTabInDialog(cta);
        waitForDialogHidingAnimation(cta);
        if (isPhone()) {
            openDialogFromStripAndVerify(cta, 2, null);
            testTitleTextFocus(cta);
        }
    }

    // Regression test for https://crbug.com/1419842
    @Test
    @MediumTest
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testTabGroupNaming_afterFocusNoTitleSaved() throws ExecutionException {
        // clang-format on
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 3);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        openDialogFromTabSwitcherAndVerify(cta, 3,
                cta.getResources().getQuantityString(
                        R.plurals.bottom_tab_grid_title_placeholder, 3, 3));

        // Click on the title this should not save the title.
        onView(allOf(withParent(withId(R.id.main_content)), withId(R.id.title))).perform(click());
        verifyTitleTextFocus(cta, true);
        Espresso.pressBack();
        verifyTitleTextFocus(cta, false);
        verifyShowingDialog(cta, 3, null);

        // Close a tab and exit dialog.
        closeFirstTabInDialog();
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimation(cta);

        // Verify the default title updated.
        verifyTabSwitcherCardCount(cta, 1);
        String twoTabsString = cta.getResources().getQuantityString(
                R.plurals.bottom_tab_grid_title_placeholder, 2, 2);
        verifyFirstCardTitle(twoTabsString);
        openDialogFromTabSwitcherAndVerify(cta, 2, twoTabsString);

        // Click on the title.
        onView(allOf(withParent(withId(R.id.main_content)), withId(R.id.title))).perform(click());
        verifyTitleTextFocus(cta, true);
        Espresso.pressBack();
        verifyTitleTextFocus(cta, false);
        verifyShowingDialog(cta, 2, null);

        // Confirm actually changing the title works.
        editDialogTitle(cta, CUSTOMIZED_TITLE1);

        // Verify the title is updated in both tab switcher and dialog.
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimation(cta);
        verifyFirstCardTitle(CUSTOMIZED_TITLE1);
        openDialogFromTabSwitcherAndVerify(cta, 2, CUSTOMIZED_TITLE1);
    }

    // Regression test for https://crbug.com/1378226.
    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID,
            ChromeFeatureList.TAB_SELECTION_EDITOR_V2})
    public void
    testTabGroupNaming_afterMergeWithSelectionEditorV2() throws ExecutionException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 4);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 4);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open dialog and modify group title.
        openDialogFromTabSwitcherAndVerify(cta, 4,
                cta.getResources().getQuantityString(
                        R.plurals.bottom_tab_grid_title_placeholder, 4, 4));
        editDialogTitle(cta, CUSTOMIZED_TITLE1);

        // Verify the title is updated in both tab switcher and dialog.
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimation(cta);
        verifyFirstCardTitle(CUSTOMIZED_TITLE1);
        openDialogFromTabSwitcherAndVerify(cta, 4, CUSTOMIZED_TITLE1);
        openSelectionEditorV2AndVerify(cta, 4);

        // Ungroup tab.
        mSelectionEditorRobot.actionRobot.clickItemAtAdapterPosition(1)
                .clickItemAtAdapterPosition(2)
                .clickToolbarMenuButton()
                .clickToolbarMenuItem("Ungroup tabs");
        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsHidden();

        // Verify the ungroup occurred.
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimation(cta);
        verifyFirstCardTitle(CUSTOMIZED_TITLE1);
        verifyTabSwitcherCardCount(cta, 3);

        enterTabSelectionEditorV2(cta);
        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsVisible();
        mSelectionEditorRobot.actionRobot.clickItemAtAdapterPosition(0)
                .clickItemAtAdapterPosition(1)
                .clickItemAtAdapterPosition(2)
                .clickToolbarMenuButton()
                .clickToolbarMenuItem("Group tabs");
        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsHidden();

        // Verify the group worked and the title remained.
        verifyFirstCardTitle(CUSTOMIZED_TITLE1);
        openDialogFromTabSwitcherAndVerify(cta, 4, CUSTOMIZED_TITLE1);
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimation(cta);
    }

    @Test
    @MediumTest
    @DisableIf.
    Build(sdk_is_greater_than = VERSION_CODES.N_MR1, message = "https://crbug.com/1124336")
    @DisableIf.Build(supported_abis_includes = "x86", message = "https://crbug.com/1124336")
    @DisableIf.Device(type = UiDisableIf.TABLET)
    public void testDialogInitialShowFromStrip() throws Exception {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabsWithThumbnail(mActivityTestRule, 2, 0, "about:blank");
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Restart the activity and open the dialog from strip to check the initial setup of dialog.
        finishActivity(cta);
        mActivityTestRule.startMainActivityFromLauncher();
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);
        openDialogFromStripAndVerify(mActivityTestRule.getActivity(), 2, null);
        closeNthTabInDialog(0);
        verifyShowingDialog(cta, 1, null);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    @DisabledTest(message = "https://crbug.com/1139475")
    public void testRenderDialog_3Tabs_Portrait(boolean nightModeEnabled) throws Exception {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabsWithThumbnail(mActivityTestRule, 3, 0, "about:blank");
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);
        verifyAllTabsHaveThumbnail(cta.getCurrentTabModel());

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        openDialogFromTabSwitcherAndVerify(cta, 3, null);

        View dialogView = cta.findViewById(R.id.dialog_parent_view);
        mRenderTestRule.render(dialogView, "3_tabs_portrait");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @DisabledTest(message = "https://crbug.com/1110099")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testRenderDialog_3Tabs_Landscape(boolean nightModeEnabled) throws Exception {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabsWithThumbnail(mActivityTestRule, 3, 0, "about:blank");
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);
        verifyAllTabsHaveThumbnail(cta.getCurrentTabModel());

        // Rotate to landscape mode and create a tab group.
        ActivityTestUtils.rotateActivityToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        openDialogFromTabSwitcherAndVerify(cta, 3, null);

        View dialogView = cta.findViewById(R.id.dialog_parent_view);
        mRenderTestRule.render(dialogView, "3_tabs_landscape");
    }

    @Test
    @DisabledTest(message = "crbug.com/1065153")
    @MediumTest
    @Feature({"RenderTest"})
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testRenderDialog_5Tabs_InitialScroll(boolean nightModeEnabled) throws Exception {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabsWithThumbnail(mActivityTestRule, 5, 0, "about:blank");
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 5);
        verifyAllTabsHaveThumbnail(cta.getCurrentTabModel());

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        openDialogFromTabSwitcherAndVerify(cta, 5, null);

        // Select the last tab and reopen the dialog. Verify that the dialog has scrolled to the
        // correct position.
        clickNthTabInDialog(cta, 4);
        enterTabSwitcher(cta);
        openDialogFromTabSwitcherAndVerify(cta, 5, null);

        View dialogView = cta.findViewById(R.id.dialog_parent_view);
        mRenderTestRule.render(dialogView, "5_tabs_select_last");
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.INSTANT_START})
    public void testSetup_WithInstantStart() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabsWithThumbnail(mActivityTestRule, 2, 0, "about:blank");
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        openDialogFromTabSwitcherAndVerify(cta, 2, null);

        // Verify TabModelObserver is correctly setup by checking if tab grid dialog changes with
        // tab closure.
        closeFirstTabInDialog();
        verifyShowingDialog(cta, 1, null);
    }

    @Test
    @MediumTest
    public void testAdjustBackGroundViewAccessibilityImportance() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Verify accessibility importance adjustment when opening dialog from tab switcher.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        verifyBackgroundViewAccessibilityImportance(cta, true);
        Espresso.pressBack();
        waitForDialogHidingAnimationInTabSwitcher(cta);
        verifyBackgroundViewAccessibilityImportance(cta, false);

        // Verify accessibility importance adjustment when opening dialog from tab strip.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        clickFirstTabInDialog(cta);
        waitForDialogHidingAnimation(cta);
        if (isPhone()) {
            openDialogFromStripAndVerify(cta, 2, null);
            verifyBackgroundViewAccessibilityImportance(cta, true);
            Espresso.pressBack();
            waitForDialogHidingAnimation(cta);
        }
        verifyBackgroundViewAccessibilityImportance(cta, false);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "TODO(crbug.com/1128345): Fix flakiness.")
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testAccessibilityString() throws ExecutionException {
        // clang-format on
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 3);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Verify the initial group card content description.
        RecyclerView recyclerView = cta.findViewById(R.id.tab_list_view);
        View firstItem = recyclerView.findViewHolderForAdapterPosition(0).itemView;
        String expandTargetString = "Expand tab group with 3 tabs.";
        assertEquals(expandTargetString, firstItem.getContentDescription());

        // Back button content description should update with group title.
        String collapseTargetString = "Collapse tab group with 3 tabs.";
        openDialogFromTabSwitcherAndVerify(cta, 3, null);
        verifyDialogBackButtonContentDescription(cta, collapseTargetString);
        editDialogTitle(cta, CUSTOMIZED_TITLE1);
        collapseTargetString = "Collapse " + CUSTOMIZED_TITLE1 + " tab group with 3 tabs.";
        verifyDialogBackButtonContentDescription(cta, collapseTargetString);

        // Group card content description should update with group title.
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimationInTabSwitcher(cta);
        verifyFirstCardTitle(CUSTOMIZED_TITLE1);
        expandTargetString = "Expand " + CUSTOMIZED_TITLE1 + " tab group with 3 tabs.";
        assertEquals(expandTargetString, firstItem.getContentDescription());

        // Verify the TabSwitcher group card close button content description should update with
        // group title.
        View closeButton = firstItem.findViewById(R.id.action_button);
        String closeButtonTargetString = "Close " + CUSTOMIZED_TITLE1 + " group with 3 tabs";
        assertEquals(closeButtonTargetString, closeButton.getContentDescription());

        // Back button content description should update with group count change.
        openDialogFromTabSwitcherAndVerify(cta, 3, CUSTOMIZED_TITLE1);
        closeFirstTabInDialog();
        verifyShowingDialog(cta, 2, CUSTOMIZED_TITLE1);
        collapseTargetString = "Collapse " + CUSTOMIZED_TITLE1 + " tab group with 2 tabs.";
        verifyDialogBackButtonContentDescription(cta, collapseTargetString);

        // Group card content description should update with group count change.
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimationInTabSwitcher(cta);
        expandTargetString = "Expand " + CUSTOMIZED_TITLE1 + " tab group with 2 tabs.";
        assertEquals(expandTargetString, firstItem.getContentDescription());

        // TabSwitcher group card Close button content description should update with group count
        // change.
        closeButtonTargetString = "Close " + CUSTOMIZED_TITLE1 + " group with 2 tabs";
        assertEquals(closeButtonTargetString, closeButton.getContentDescription());

        // Back button content description should restore when the group loses customized title.
        openDialogFromTabSwitcherAndVerify(cta, 2, CUSTOMIZED_TITLE1);
        editDialogTitle(cta, "");
        verifyShowingDialog(cta, 2, null);
        collapseTargetString = "Collapse tab group with 2 tabs.";
        verifyDialogBackButtonContentDescription(cta, collapseTargetString);

        // Back button content description should update when the group becomes a single tab.
        closeFirstTabInDialog();
        verifyShowingDialog(cta, 1, "1 tab");
        collapseTargetString = "Collapse 1 tab.";
        verifyDialogBackButtonContentDescription(cta, collapseTargetString);

        // Group card content description should restore when the group becomes a single tab.
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimationInTabSwitcher(cta);
        assertEquals(null, firstItem.getContentDescription());

        // TabSwitcher Group card Close button content description should restore when the group
        // becomes a single tab.
        closeButtonTargetString = "Close New tab tab";
        assertEquals(closeButtonTargetString, closeButton.getContentDescription());
    }

    @Test
    @MediumTest
    @DisableIf.Device(type = UiDisableIf.TABLET)
    @Features.EnableFeatures({ChromeFeatureList.TAB_SELECTION_EDITOR_V2})
    public void testStripDialog_TabSelectionEditorV2CloseAll_NoCustomHomepage() throws Exception {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        // Create a tab group with 2 tabs.
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Enter tab switcher and select first tab.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        clickFirstTabInDialog(cta);
        waitForDialogHidingAnimation(cta);

        // Make sure tab strip is showing.
        CriteriaHelper.pollUiThread(()
                                            -> mActivityTestRule.getActivity()
                                                       .getBrowserControlsManager()
                                                       .getBottomControlOffset()
                        == 0);
        waitForView(allOf(withId(R.id.toolbar_left_button), isCompletelyDisplayed()));

        // Test opening dialog from strip and from tab switcher.
        openDialogFromStripAndVerify(cta, 2, null);
        openSelectionEditorV2AndVerify(cta, 2);

        // Close two tabs.
        mSelectionEditorRobot.actionRobot.clickItemAtAdapterPosition(0)
                .clickItemAtAdapterPosition(1)
                .clickToolbarMenuButton()
                .clickToolbarMenuItem("Close tabs");

        // Rather than destroying the activity the GTS should be showing.
        verifyTabSwitcherCardCount(cta, 0);
    }

    @Test
    @MediumTest
    @DisableIf.Device(type = UiDisableIf.TABLET)
    @Features.EnableFeatures({ChromeFeatureList.TAB_SELECTION_EDITOR_V2})
    public void testStripDialog_TabSelectionEditorV2CloseAll_CustomHomepage() throws Exception {
        GURL url = new GURL(mActivityTestRule.getEmbeddedTestServerRule().getServer().getURL(
                "/chrome/test/data/android/google.html"));
        when(mHomepagePolicyManager.isHomepageLocationPolicyEnabled()).thenReturn(true);
        when(mHomepagePolicyManager.getHomepagePreference()).thenReturn(url);

        HomepagePolicyManager.setInstanceForTests(mHomepagePolicyManager);
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        // Create a tab group with 2 tabs.
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Enter tab switcher and select first tab.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        clickFirstTabInDialog(cta);
        waitForDialogHidingAnimation(cta);

        // Make sure tab strip is showing.
        CriteriaHelper.pollUiThread(()
                                            -> mActivityTestRule.getActivity()
                                                       .getBrowserControlsManager()
                                                       .getBottomControlOffset()
                        == 0);
        waitForView(allOf(withId(R.id.toolbar_left_button), isCompletelyDisplayed()));

        // Test opening dialog from strip and from tab switcher.
        openDialogFromStripAndVerify(cta, 2, null);
        openSelectionEditorV2AndVerify(cta, 2);

        // Close two tabs.
        mSelectionEditorRobot.actionRobot.clickItemAtAdapterPosition(0)
                .clickItemAtAdapterPosition(1)
                .clickToolbarMenuButton()
                .clickToolbarMenuItem("Close tabs");

        // With a custom homepage exit the app.
        CriteriaHelper.pollUiThread(() -> cta.isDestroyed());
        HomepagePolicyManager.setInstanceForTests(null);
    }

    @Test
    @MediumTest
    @DisableIf.Device(type = UiDisableIf.TABLET)
    @Features.EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.
    Add({"force-fieldtrials=Study/Group", START_SURFACE_TEST_SINGLE_ENABLED_PARAMS})
    public void testDialogSetup_WithStartSurface() throws Exception {
        // Create a tab group with 2 tabs.
        finishActivity(mActivityTestRule.getActivity());
        createThumbnailBitmapAndWriteToFile(0);
        createThumbnailBitmapAndWriteToFile(1);
        TabAttributeCache.setRootIdForTesting(0, 0);
        TabAttributeCache.setRootIdForTesting(1, 0);
        createTabStateFile(new int[] {0, 1});

        // Restart Chrome and make sure tab strip is showing.
        mActivityTestRule.startMainActivityFromLauncher();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(cta.getTabModelSelector()::isTabStateInitialized);
        CriteriaHelper.pollUiThread(()
                                            -> mActivityTestRule.getActivity()
                                                       .getBrowserControlsManager()
                                                       .getBottomControlOffset()
                        == 0);
        waitForView(allOf(withId(R.id.toolbar_left_button), isCompletelyDisplayed()));

        // Test opening dialog from strip and from tab switcher.
        openDialogFromStripAndVerify(cta, 2, null);
        Espresso.pressBack();

        // Tab switcher is created, and the dummy signal to hide dialog is sent. This line would
        // crash if the dummy signal is not properly handled. See crbug.com/1096358.
        enterTabSwitcher(cta);
        onView(allOf(withParent(withId(getTabSwitcherParentId(mActivityTestRule.getActivity()))),
                       withId(R.id.tab_list_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, click()));
        CriteriaHelper.pollUiThread(() -> isDialogShowing(mActivityTestRule.getActivity()));
        verifyShowingDialog(cta, 2, null);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
            START_SURFACE_TEST_SINGLE_ENABLED_PARAMS + "/hide_switch_when_no_incognito_tabs/false"})
    @DisableIf.Device(type = UiDisableIf.TABLET)
    @DisabledTest(message = "crbug.com/1119899, crbug.com/1131545")
    // clang-format off
    public void testUndoClosureInDialog_WithStartSurface() throws Exception {
        // clang-format on
        // Create a tab group with 2 tabs.
        finishActivity(mActivityTestRule.getActivity());
        createThumbnailBitmapAndWriteToFile(0);
        createThumbnailBitmapAndWriteToFile(1);
        TabAttributeCache.setRootIdForTesting(0, 0);
        TabAttributeCache.setRootIdForTesting(1, 0);
        createTabStateFile(new int[] {0, 1});

        // Restart Chrome and make sure tab strip is showing.
        mActivityTestRule.startMainActivityFromLauncher();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(cta.getTabModelSelector()::isTabStateInitialized);
        CriteriaHelper.pollUiThread(
                () -> cta.getBrowserControlsManager().getBottomControlOffset() == 0);
        waitForView(allOf(withId(R.id.toolbar_left_button), isCompletelyDisplayed()));

        // Test undo closure in dialog from tab strip.
        openDialogFromStripAndVerify(cta, 2, null);
        closeFirstTabInDialog();
        verifyShowingDialog(cta, 1, null);
        verifyDialogUndoBarAndClick();
        verifyShowingDialog(cta, 2, null);
        clickScrimToExitDialog(cta);
        verifyTabStripFaviconCount(cta, 2);

        // Test undo closure in dialog from StartSurface tab switcher.
        enterTabSwitcher(cta);
        onView(allOf(withParent(withId(getTabSwitcherParentId(mActivityTestRule.getActivity()))),
                       withId(R.id.tab_list_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, click()));
        CriteriaHelper.pollUiThread(() -> isDialogShowing(cta));
        verifyShowingDialog(cta, 2, null);
        closeFirstTabInDialog();
        verifyShowingDialog(cta, 1, null);
        verifyDialogUndoBarAndClick();
        verifyShowingDialog(cta, 2, null);

        // Test undo closure in dialog from StartSurface home page.
        clickScrimToExitDialog(cta);
        onView(withId(R.id.new_tab_button)).perform(click());
        onView(allOf(withParent(withId(org.chromium.chrome.R.id.carousel_tab_switcher_container)),
                       withId(R.id.tab_list_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, click()));
        CriteriaHelper.pollUiThread(() -> isDialogShowing(cta));
        verifyShowingDialog(cta, 2, null);
        closeFirstTabInDialog();
        verifyShowingDialog(cta, 1, null);
        verifyDialogUndoBarAndClick();
        verifyShowingDialog(cta, 2, null);
    }

    @Test
    @MediumTest
    public void testCreateTabInDialog() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        // Open dialog from tab switcher and verify dialog is showing correct content.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);

        // Create a tab by tapping "+" on the dialog.
        onView(allOf(withId(R.id.toolbar_right_button),
                       isDescendantOfA(withId(R.id.dialog_container_view))))
                .perform(click());
        waitForDialogHidingAnimation(cta);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);
        openDialogFromTabSwitcherAndVerify(cta, 3, null);

        // Enter first tab page.
        clickFirstTabInDialog(cta);
        waitForDialogHidingAnimation(cta);

        if (isPhone()) {
            // Open dialog from tab strip and verify dialog is showing correct content.
            openDialogFromStripAndVerify(cta, 3, null);

            // Create a tab by tapping "+" on the dialog.
            onView(allOf(withId(R.id.toolbar_right_button),
                           isDescendantOfA(withId(R.id.dialog_container_view))))
                    .perform(click());
            waitForDialogHidingAnimation(cta);
            openDialogFromStripAndVerify(cta, 4, null);
        }
    }

    private void openDialogFromTabSwitcherAndVerify(
            ChromeTabbedActivity cta, int tabCount, String customizedTitle) {
        clickFirstCardFromTabSwitcher(cta);
        CriteriaHelper.pollUiThread(() -> isDialogShowing(cta));
        verifyShowingDialog(cta, tabCount, customizedTitle);
    }

    private void openDialogFromStripAndVerify(
            ChromeTabbedActivity cta, int tabCount, String customizedTitle) {
        showDialogFromStrip(cta);
        CriteriaHelper.pollUiThread(() -> isDialogShowing(cta));
        verifyShowingDialog(cta, tabCount, customizedTitle);
    }

    private void verifyShowingDialog(
            ChromeTabbedActivity cta, int tabCount, String customizedTitle) {
        onView(allOf(withId(R.id.tab_list_view), withParent(withId(R.id.dialog_container_view))))
                .check(matches(isDisplayed()))
                .check(TabUiTestHelper.ChildrenCountAssertion.havingTabCount(tabCount));

        // Check contents within dialog.
        onView(allOf(withParent(withId(R.id.main_content)), withId(R.id.title)))
                .check((v, noMatchException) -> {
                    if (noMatchException != null) throw noMatchException;

                    Assert.assertTrue(v instanceof EditText);
                    EditText titleText = (EditText) v;
                    String title = customizedTitle == null
                            ? cta.getResources().getQuantityString(
                                    R.plurals.bottom_tab_grid_title_placeholder, tabCount, tabCount)
                            : customizedTitle;
                    Assert.assertEquals(title, titleText.getText().toString());
                    assertFalse(v.isFocused());
                });

        // Check dummy views used for animations are not visible.
        onView(allOf(withParent(withId(R.id.dialog_parent_view)), withId(R.id.dialog_frame)))
                .check((v, e) -> assertEquals(0f, v.getAlpha(), 0.0));
        onView(allOf(withParent(withId(R.id.dialog_parent_view)),
                       withId(R.id.dialog_animation_card_view)))
                .check((v, e) -> assertEquals(0f, v.getAlpha(), 0.0));

        // For devices with version higher or equal to O_MR1 and use light color navigation bar,
        // make sure that the color of navigation bar is changed by dialog scrim.
        Resources resources = cta.getResources();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O_MR1
                || !resources.getBoolean(R.bool.window_light_navigation_bar) || isTablet(cta)) {
            return;
        }
        final @ColorInt int scrimDefaultColor = cta.getColor(R.color.default_scrim_color);
        final @ColorInt int navigationBarColor = SemanticColorUtils.getBottomSystemNavColor(cta);
        float scrimColorAlpha = (scrimDefaultColor >>> 24) / 255f;
        int scrimColorOpaque = scrimDefaultColor & 0xFF000000;
        int navigationBarColorWithScrimOverlay = ColorUtils.getColorWithOverlay(
                navigationBarColor, scrimColorOpaque, scrimColorAlpha, true);

        assertEquals(cta.getWindow().getNavigationBarColor(), navigationBarColorWithScrimOverlay);
        assertNotEquals(navigationBarColor, navigationBarColorWithScrimOverlay);
    }

    private boolean isPhone() {
        return !isTablet(mActivityTestRule.getActivity());
    }

    private boolean isDialogShowing(ChromeTabbedActivity cta) {
        View dialogView = cta.findViewById(R.id.dialog_parent_view);
        View dialogContainerView = cta.findViewById(R.id.dialog_container_view);
        return dialogView.getVisibility() == View.VISIBLE && dialogContainerView.getAlpha() == 1f;
    }

    private boolean isDialogHiding(ChromeTabbedActivity cta) {
        View dialogView = cta.findViewById(R.id.dialog_parent_view);
        return dialogView.getVisibility() == View.GONE;
    }

    private void showDialogFromStrip(ChromeTabbedActivity cta) {
        assertFalse(cta.getLayoutManager().isLayoutVisible(LayoutType.TAB_SWITCHER));
        onView(allOf(withId(R.id.toolbar_left_button),
                       isDescendantOfA(withId(R.id.bottom_controls))))
                .perform(click());
    }

    private void verifyTabGroupsContinuation(ChromeTabbedActivity cta, boolean isEnabled) {
        assertEquals(isEnabled, TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(cta));

        // Verify whether the menu button exists.
        onView(withId(R.id.toolbar_menu_button))
                .check(isEnabled ? matches(isDisplayed()) : doesNotExist());

        // Try to grab focus of the title text field by clicking on it.
        onView(allOf(withParent(withId(R.id.main_content)), withId(R.id.title)))
                .perform(click());
        onView(allOf(withParent(withId(R.id.main_content)), withId(R.id.title)))
                .check((v, noMatchException) -> {
                    if (noMatchException != null) throw noMatchException;

                    // Verify if we can grab focus on the editText or not.
                    assertEquals(isEnabled, v.isFocused());
                });
        // Verify if the keyboard shows or not.
        CriteriaHelper.pollUiThread(()
                                            -> isEnabled
                        == KeyboardVisibilityDelegate.getInstance().isKeyboardShowing(
                                cta, cta.getCompositorViewHolderForTesting()));
    }

    private void openDialogToolbarMenuAndVerify(ChromeTabbedActivity cta) {
        onView(withId(R.id.toolbar_menu_button))
                .perform(click());
        onView(withId(R.id.tab_switcher_action_menu_list))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .check((v, noMatchException) -> {
                    if (noMatchException != null) throw noMatchException;
                    Assert.assertTrue(v instanceof ListView);
                    ListView listView = (ListView) v;
                    int menuItemCount = 1;
                    if (TabUiFeatureUtilities.isTabSelectionEditorV2Enabled(cta)) {
                        verifyTabGridDialogToolbarMenuItem(
                                listView, 0, cta.getString(R.string.menu_select_tabs));
                    } else {
                        verifyTabGridDialogToolbarMenuItem(listView, 0,
                                cta.getString(R.string.tab_grid_dialog_toolbar_remove_from_group));
                    }
                    if (TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled(cta)) {
                        menuItemCount += 1;
                        verifyTabGridDialogToolbarMenuItem(listView, menuItemCount - 1,
                                cta.getString(R.string.tab_grid_dialog_toolbar_edit_group_name));
                    }
                    assertEquals(menuItemCount, listView.getCount());
                });
    }

    private void verifyTabGridDialogToolbarMenuItem(ListView listView, int index, String text) {
        View menuItemView = listView.getChildAt(index);
        TextView menuItemText = menuItemView.findViewById(R.id.menu_item_text);
        assertEquals(text, menuItemText.getText());
    }

    private void selectTabGridDialogToolbarMenuItem(ChromeTabbedActivity cta, String buttonText) {
        onView(withText(buttonText))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .perform(click());
    }

    private void triggerShareGroupAndVerify(ChromeTabbedActivity cta) {
        selectTabGridDialogToolbarMenuItem(cta, "Share group");
        intended(allOf(hasAction(equalTo(Intent.ACTION_CHOOSER)),
                hasExtras(hasEntry(equalTo(Intent.EXTRA_INTENT),
                        allOf(hasAction(equalTo(Intent.ACTION_SEND)), hasType("text/plain"))))));
    }

    private void waitForDialogHidingAnimation(ChromeTabbedActivity cta) {
        CriteriaHelper.pollUiThread(() -> isDialogHiding(cta));
    }

    private void waitForDialogHidingAnimationInTabSwitcher(ChromeTabbedActivity cta) {
        waitForDialogHidingAnimation(cta);
        // Animation source card becomes alpha = 0f when dialog is showing and animates back to 1f
        // when dialog hides. Make sure the source card has restored its alpha change.
        CriteriaHelper.pollUiThread(() -> {
            RecyclerView recyclerView = cta.findViewById(R.id.tab_list_view);
            for (int i = 0; i < recyclerView.getAdapter().getItemCount(); i++) {
                RecyclerView.ViewHolder viewHolder =
                        recyclerView.findViewHolderForAdapterPosition(i);
                if (viewHolder == null) continue;
                if (viewHolder.itemView.getAlpha() != 1f) return false;
            }
            return true;
        });
    }

    private void openSelectionEditorAndVerify(ChromeTabbedActivity cta, int count) {
        // Open tab selection editor by selecting ungroup item in tab grid dialog menu.
        onView(withId(R.id.toolbar_menu_button))
                .perform(click());
        onView(withText(cta.getString(R.string.tab_grid_dialog_toolbar_remove_from_group)))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .perform(click());

        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsVisible()
                .verifyToolbarActionButtonDisabled()
                .verifyToolbarActionButtonWithResourceId(
                        R.string.tab_grid_dialog_selection_mode_remove)
                .verifyToolbarSelectionTextWithResourceId(
                        R.string.tab_selection_editor_toolbar_select_tabs)
                .verifyAdapterHasItemCount(count);
    }

    private void openSelectionEditorV2AndVerify(ChromeTabbedActivity cta, int count) {
        // Open tab selection editor by selecting the select tabs item in tab grid dialog menu.
        onView(withId(R.id.toolbar_menu_button)).perform(click());
        onView(withText(cta.getString(R.string.menu_select_tabs)))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .perform(click());

        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsVisible()
                .verifyToolbarSelectionTextWithResourceId(
                        R.string.tab_selection_editor_toolbar_select_tabs)
                .verifyAdapterHasItemCount(count);
    }

    private void checkPosition(ChromeTabbedActivity cta, boolean isDialog, boolean isPortrait) {
        // If isDialog is true, we are checking the position of TabGridDialog; otherwise we are
        // checking the position of TabSelectionEditor.
        int contentViewId = isDialog ? R.id.dialog_container_view : R.id.selectable_list;
        int smallMargin =
                (int) cta.getResources().getDimension(R.dimen.tab_grid_dialog_side_margin);
        int largeMargin = (int) cta.getResources().getDimension(R.dimen.tab_grid_dialog_top_margin);
        int topMargin = isPortrait ? largeMargin : smallMargin;
        int sideMargin = isPortrait ? smallMargin : largeMargin;
        View parentView = cta.getCompositorViewHolderForTesting();
        Rect parentRect = new Rect();
        parentView.getGlobalVisibleRect(parentRect);
        int[] parentLoc = new int[2];
        parentView.getLocationOnScreen(parentLoc);
        onView(withId(contentViewId)).check((v, e) -> {
            int[] location = new int[2];
            v.getLocationOnScreen(location);
            int relLoc0 = location[0] - parentLoc[0];
            int relLoc1 = location[1] - parentLoc[1];
            // Check the position.
            assertEquals(sideMargin, relLoc0);
            assertEquals(topMargin, relLoc1);
            // Check the size.
            assertEquals(parentView.getHeight() - 2 * topMargin, v.getHeight());
            assertEquals(parentView.getWidth() - 2 * sideMargin, v.getWidth());
        });
    }

    private void editDialogTitle(ChromeTabbedActivity cta, String title) {
        onView(allOf(withParent(withId(R.id.main_content)), withId(R.id.title)))
                .perform(click())
                .check((v, e) -> {
                    // Verify all texts in the field are selected.
                    EditText titleView = (EditText) v;
                    assertEquals(titleView.getText().length(),
                            titleView.getSelectionEnd() - titleView.getSelectionStart());
                })
                .perform(replaceText(title))
                .perform(pressImeActionButton());
        // Wait until the keyboard is hidden to make sure the edit has taken effect.
        KeyboardVisibilityDelegate delegate = KeyboardVisibilityDelegate.getInstance();
        CriteriaHelper.pollUiThread(
                () -> !delegate.isKeyboardShowing(cta, cta.getCompositorViewHolderForTesting()));
    }

    private void verifyFirstCardTitle(String title) {
        onView(allOf(withParent(withId(TabUiTestHelper.getTabSwitcherParentId(
                             mActivityTestRule.getActivity()))),
                       withId(R.id.tab_list_view)))
                .check((v, noMatchException) -> {
                    if (noMatchException != null) throw noMatchException;

                    RecyclerView recyclerView = (RecyclerView) v;
                    TextView firstCardTitleTextView =
                            recyclerView.findViewHolderForAdapterPosition(0).itemView.findViewById(
                                    R.id.tab_title);
                    assertEquals(title, firstCardTitleTextView.getText().toString());
                });
    }

    private void clickScrimToExitDialog(ChromeTabbedActivity cta) throws ExecutionException {
        CriteriaHelper.pollUiThread(() -> isDialogShowing(cta));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            View scrimView;
            if (isTablet(cta)) {
                TabGridDialogView dialogView = cta.findViewById(R.id.dialog_parent_view);
                scrimView = dialogView.getScrimCoordinatorForTesting().getViewForTesting();
            } else {
                scrimView = cta.getRootUiCoordinatorForTesting()
                                    .getScrimCoordinator()
                                    .getViewForTesting();
            }
            scrimView.performClick();
        });
    }

    private boolean isTablet(ChromeTabbedActivity cta) {
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(cta);
    }

    private void verifyBackgroundViewAccessibilityImportance(
            ChromeTabbedActivity cta, boolean isDialogShowing) {
        View controlContainer = cta.findViewById(R.id.control_container);
        assertEquals(isDialogShowing,
                IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
                        == controlContainer.getImportantForAccessibility());
        View compositorViewHolder = cta.getCompositorViewHolderForTesting();
        assertEquals(isDialogShowing,
                IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
                        == compositorViewHolder.getImportantForAccessibility());
        View bottomContainer = cta.findViewById(R.id.bottom_container);
        assertEquals(isDialogShowing,
                IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
                        == bottomContainer.getImportantForAccessibility());
        if (isPhone()) {
            View bottomControls = cta.findViewById(R.id.bottom_controls);
            assertEquals(isDialogShowing,
                    IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
                            == bottomControls.getImportantForAccessibility());
        }
        if (isTablet(cta)) {
            View tabSwitcherViewHolder = cta.findViewById(R.id.grid_tab_switcher_view_holder);
            assertEquals(isDialogShowing,
                    IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
                            == tabSwitcherViewHolder.getImportantForAccessibility());
        }
    }

    private void verifyDialogUndoBarAndClick() {
        // Verify that the dialog undo bar is showing and the default undo bar is hidden.
        onViewWaiting(allOf(withId(R.id.snackbar_button),
                isDescendantOfA(withId(R.id.dialog_snack_bar_container_view)), isDisplayed()));
        onView(allOf(withId(R.id.snackbar), isDescendantOfA(withId(R.id.bottom_container))))
                .check(doesNotExist());
        onView(allOf(withId(R.id.snackbar_button),
                       isDescendantOfA(withId(R.id.dialog_snack_bar_container_view)),
                       isDisplayed()))
                .perform(click());
    }

    private void verifyGlobalUndoBarAndClick() {
        // Verify that the dialog undo bar is showing and the default undo bar is hidden.
        int expectedParent = isTablet(mActivityTestRule.getActivity())
                ? R.id.grid_tab_switcher_view_holder
                : R.id.bottom_container;
        onViewWaiting(allOf(
                withId(R.id.snackbar), isDescendantOfA(withId(expectedParent)), isDisplayed()));
        onView(allOf(withId(R.id.snackbar_button),
                       isDescendantOfA(withId(R.id.dialog_snack_bar_container_view))))
                .check(doesNotExist());
        onView(allOf(withId(R.id.snackbar_button), isDescendantOfA(withId(expectedParent)),
                       isDisplayed()))
                .perform(click());
    }

    private void verifyDialogBackButtonContentDescription(ChromeTabbedActivity cta, String s) {
        assertTrue(isDialogShowing(cta));
        onView(allOf(withId(R.id.toolbar_left_button),
                       isDescendantOfA(withId(R.id.dialog_container_view))))
                .check((v, e) -> assertEquals(s, v.getContentDescription()));
    }

    private void testTitleTextFocus(ChromeTabbedActivity cta) throws ExecutionException {
        // Click the text field to grab focus and click back button to lose focus.
        onView(allOf(withParent(withId(R.id.main_content)), withId(R.id.title))).perform(click());
        verifyTitleTextFocus(cta, true);
        Espresso.pressBack();
        verifyTitleTextFocus(cta, false);
        verifyShowingDialog(cta, 2, null);

        // Use toolbar menu to grab focus and click back button to lose focus.
        openDialogToolbarMenuAndVerify(cta);
        selectTabGridDialogToolbarMenuItem(cta, "Edit group name");
        verifyTitleTextFocus(cta, true);
        Espresso.pressBack();
        verifyTitleTextFocus(cta, false);
        verifyShowingDialog(cta, 2, null);

        // Click the text field to grab focus and click scrim to lose focus.
        onView(allOf(withParent(withId(R.id.main_content)), withId(R.id.title))).perform(click());
        verifyTitleTextFocus(cta, true);
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimation(cta);
        verifyTitleTextFocus(cta, false);
    }

    private void verifyTitleTextFocus(ChromeTabbedActivity cta, boolean shouldFocus) {
        CriteriaHelper.pollUiThread(() -> {
            View titleTextView = cta.findViewById(R.id.tab_group_toolbar).findViewById(R.id.title);
            KeyboardVisibilityDelegate delegate = KeyboardVisibilityDelegate.getInstance();
            boolean keyboardVisible =
                    delegate.isKeyboardShowing(cta, cta.getCompositorViewHolderForTesting());
            boolean isFocused = titleTextView.isFocused();
            return (!shouldFocus ^ isFocused) && (!shouldFocus ^ keyboardVisible);
        });
    }

    private void enterTabSelectionEditorV2(ChromeTabbedActivity cta) {
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(), cta, R.id.menu_select_tabs);
    }
}
