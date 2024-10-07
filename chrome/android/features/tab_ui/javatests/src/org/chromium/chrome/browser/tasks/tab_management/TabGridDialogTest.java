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
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.greaterThanOrEqualTo;
import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.lessThanOrEqualTo;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.Matchers.notNullValue;
import static org.hamcrest.core.IsEqual.equalTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.ANDROID_HUB_SEARCH;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.DATA_SHARING;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.NAV_BAR_COLOR_MATCHES_TAB_BACKGROUND;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.TAB_GROUP_PANE_ANDROID;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.TAB_GROUP_PARITY_ANDROID;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.TAB_GROUP_SYNC_ANDROID;
import static org.chromium.chrome.browser.ntp.HomeSurfaceTestUtils.createTabStatesAndMetadataFile;
import static org.chromium.chrome.browser.ntp.HomeSurfaceTestUtils.createThumbnailBitmapAndWriteToFile;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.addBlankTabs;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickFirstCardFromTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickFirstTabInDialog;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickNthTabInDialog;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.closeFirstTabInDialog;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.closeNthTabInDialog;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.createTabGroup;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.createTabs;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.finishActivity;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.getSwipeToDismissAction;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.getTabSwitcherAncestorId;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.leaveTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.mergeAllNormalTabsToAGroup;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.prepareTabsWithThumbnail;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyAllTabsHaveThumbnail;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabStripFaviconCount;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabSwitcherCardCount;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.waitForThumbnailsToFetch;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;
import static org.chromium.ui.test.util.ViewUtils.waitForVisibleView;

import android.content.Intent;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.Rect;
import android.os.Build;
import android.os.Build.VERSION_CODES;
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
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.lifecycle.ActivityLifecycleMonitor;
import androidx.test.runner.lifecycle.ActivityLifecycleMonitorRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.TestAnimations.EnableAnimations;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkEditActivity;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.homepage.HomepagePolicyManager;
import org.chromium.chrome.browser.hub.HubContainerView;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.ViewUtils;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutionException;

/** End-to-end tests for TabGridDialog component. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction({Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@DisableFeatures({
    TAB_GROUP_PARITY_ANDROID,
    NAV_BAR_COLOR_MATCHES_TAB_BACKGROUND,
    ANDROID_HUB_SEARCH
})
@Batch(Batch.PER_CLASS)
public class TabGridDialogTest {
    private static final String CUSTOMIZED_TITLE1 = "wfh tips";
    private static final String CUSTOMIZED_TITLE2 = "wfh funs";
    private static final String PAGE_WITH_HTTPS_CANONICAL_URL =
            "/chrome/test/data/android/share/link_share_https_canonical.html";
    private static final String PAGE_WITH_HTTP_CANONICAL_URL =
            "/chrome/test/data/android/share/link_share_http_canonical.html";

    private static final ActivityLifecycleMonitor sMonitor =
            ActivityLifecycleMonitorRegistry.getInstance();

    private boolean mHasReceivedSourceRect;
    private TabListEditorTestingRobot mSelectionEditorRobot = new TabListEditorTestingRobot();
    private ModalDialogManager mModalDialogManager;
    private PrefService mPrefService;

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(
                            ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_TAB_SWITCHER_GRID)
                    .setRevision(13)
                    .build();

    // Must force tab re-creation to ensure tab group names make sense.
    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, true);

    @Mock private HomepagePolicyManager mHomepagePolicyManager;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        ChromeNightModeTestUtils.setUpNightModeBeforeChromeActivityLaunched();
    }

    @ParameterAnnotations.UseMethodParameterBefore(NightModeTestUtils.NightModeParams.class)
    public void setupNightMode(boolean nightModeEnabled) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
                });
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
        final ChromeTabbedActivity ctaNightMode =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(), ChromeTabbedActivity.class);
        sActivityTestRule.setActivity(ctaNightMode);
        CriteriaHelper.pollUiThread(
                sActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Intents.init();
        // Some of the tests may finish the activity using moveTaskToBack.
        ChromeTabbedActivity.interceptMoveTaskToBackForTesting();
        CriteriaHelper.pollUiThread(
                sActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);
        mModalDialogManager =
                ThreadUtils.runOnUiThreadBlocking(
                        sActivityTestRule.getActivity()::getModalDialogManager);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile =
                            sActivityTestRule
                                    .getActivity()
                                    .getProfileProviderSupplier()
                                    .get()
                                    .getOriginalProfile();
                    mPrefService = UserPrefs.get(profile);
                    ActionConfirmationManager.clearStopShowingPrefsForTesting(mPrefService);
                });
    }

    @After
    public void tearDown() throws Exception {
        try {
            Intents.release();
        } catch (NullPointerException e) {
            // This will fail if the ChromeTabbedActivity is already finished.
            // IntentsTestRule was created to avoid this, but it is deprecated and hard to integrate
            // with batched tests.
        }
        ActivityTestUtils.clearActivityOrientation(sActivityTestRule.getActivity());
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        if (cta == null) return;

        boolean isDestroyed =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return sMonitor.getLifecycleStageOf(cta) == Stage.DESTROYED;
                        });
        if (isDestroyed) return;

        View dialogView = cta.findViewById(R.id.dialog_container_view);
        if (dialogView != null) {
            if (isDialogFullyVisible(cta)) {
                clickScrimToExitDialog(cta);
            }
            waitForDialogHidingAnimation(cta);
        }

        dismissAllModalDialogs();

        if (cta.getLayoutManager().isLayoutVisible(LayoutType.TAB_SWITCHER)
                && !cta.getLayoutManager().isLayoutStartingToHide(LayoutType.TAB_SWITCHER)) {
            if (cta.getTabModelSelectorSupplier().get().getTotalTabCount() == 0) {
                addBlankTabs(cta, false, 1);
                LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);
            } else {
                leaveTabSwitcher(cta);
            }
        } else {
            LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);
        }
    }

    @Test
    @MediumTest
    public void testBackPressCloseDialog() {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
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
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
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
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
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
    public void testAddTabHidesDialog() {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        // Open dialog from tab switcher and verify dialog is showing correct content.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);

        // Add a tab to hide the group. Any foreground launch type will also hide the tab switcher
        // so use a background one to make testing easier.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab parentTab = cta.getTabModelSelector().getModel(false).getTabAt(0);
                    cta.getCurrentTabCreator()
                            .createNewTab(
                                    new LoadUrlParams("about:blank"),
                                    "About title",
                                    TabLaunchType.FROM_LONGPRESS_BACKGROUND_IN_GROUP,
                                    parentTab,
                                    TabModel.INVALID_TAB_INDEX);
                });
        waitForDialogHidingAnimationInTabSwitcher(cta);

        verifyTabSwitcherCardCount(cta, 1);
    }

    @Test
    @MediumTest
    public void testAddTabSkipsHideDialogForSync() {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        // Open dialog from tab switcher and verify dialog is showing correct content.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);

        // Add a tab to hide the group.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelSelector selector = cta.getTabModelSelector();
                    Tab destinationTab = selector.getModel(false).getTabAt(0);
                    Tab tab =
                            cta.getCurrentTabCreator()
                                    .createNewTab(
                                            new LoadUrlParams("about:blank"),
                                            "About title",
                                            TabLaunchType.FROM_SYNC_BACKGROUND,
                                            null,
                                            TabModel.INVALID_TAB_INDEX);
                    ((TabGroupModelFilter)
                                    selector.getTabModelFilterProvider().getTabModelFilter(false))
                            .mergeListOfTabsToGroup(
                                    List.of(tab), destinationTab, /* notify= */ false);
                });
        CriteriaHelper.pollUiThread(() -> isDialogFullyVisible(cta));
        verifyShowingDialog(cta, 3, null);
    }

    @Test
    @MediumTest
    @EnableFeatures(TAB_GROUP_PARITY_ANDROID)
    @RequiresRestart("Group creation modal dialog is sometimes persistent after dismissing")
    public void testTabGroupDialogUi() {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open dialog and verify dialog is showing correct content.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);

        // Verify TabGroupsContinuation related functionality is exposed.
        verifyTabGroupDialogUi(cta);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "Flaky test - see: https://crbug.com/1177149")
    public void testTabGridDialogAnimation() {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Add 400px top margin to the recyclerView.
        RecyclerView recyclerView = cta.findViewById(R.id.tab_list_recycler_view);
        float tabGridCardPadding = TabUiThemeProvider.getTabGridCardMargin(cta);
        int deltaTopMargin = 400;
        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams) recyclerView.getLayoutParams();
        params.topMargin += deltaTopMargin;
        ThreadUtils.runOnUiThreadBlocking(() -> recyclerView.setLayoutParams(params));
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
        TabGridDialogView.setSourceRectCallbackForTesting(
                result -> {
                    mHasReceivedSourceRect = true;
                    assertEquals(expectedTop, result.top, 0.0);
                    assertEquals(expectedHeight, result.height(), 0.0);
                    assertEquals(expectedWidth, result.width(), 0.0);
                });

        TabUiTestHelper.clickFirstCardFromTabSwitcher(cta);
        CriteriaHelper.pollUiThread(() -> mHasReceivedSourceRect);
        CriteriaHelper.pollUiThread(() -> isDialogFullyVisible(cta));
    }

    @Test
    @MediumTest
    public void testUndoClosureInDialog_DialogUndoBar() throws ExecutionException {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
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
    public void testTabGroupDialogRemainsOpenOnSyncUpdate() {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        createTabs(cta, false, 3);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);
        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        leaveTabSwitcher(cta);

        TabModel model = cta.getTabModelSelector().getModel(false);
        addBlankTabs(cta, false, 3);
        enterTabSwitcher(cta);
        createTabGroup(
                cta, false, List.of(model.getTabAt(3), model.getTabAt(4), model.getTabAt(5)));

        // Open dialog and verify dialog is showing correct content.
        openDialogFromTabSwitcherAndVerify(cta, 3, null);

        Callback<Integer> closeTabAt =
                (index) -> {
                    model.closeTabs(TabClosureParams.closeTab(model.getTabAt(index)).build());
                };
        // Close two tabs in the current group
        ThreadUtils.runOnUiThreadBlocking(closeTabAt.bind(0));
        verifyShowingDialog(cta, 2, null);
        ThreadUtils.runOnUiThreadBlocking(closeTabAt.bind(0));
        verifyShowingDialog(cta, 1, null);

        // Close two tabs in the GTS background group, dialog should still show.
        ThreadUtils.runOnUiThreadBlocking(closeTabAt.bind(1));
        verifyShowingDialog(cta, 1, null);
        ThreadUtils.runOnUiThreadBlocking(closeTabAt.bind(1));
        verifyShowingDialog(cta, 1, null);
        ThreadUtils.runOnUiThreadBlocking(closeTabAt.bind(1));
        verifyShowingDialog(cta, 1, null);
    }

    @Test
    @MediumTest
    @EnableFeatures(TAB_GROUP_PARITY_ANDROID)
    @RequiresRestart("Group creation modal dialog is sometimes persistent when dismissing")
    public void testColorPickerOnIconClick() throws ExecutionException {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();

        String blueColor =
                cta.getString(R.string.accessibility_tab_group_color_picker_color_item_blue);
        String notSelectedStringBlue =
                cta.getString(
                        R.string
                                .accessibility_tab_group_color_picker_color_item_not_selected_description,
                        blueColor);

        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Expect the color icon to be clicked.
        var histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecordTimes(
                                "Android.TabGroupParity.TabGroupColorChangeActionType", 0, 3)
                        .build();

        // Open dialog and click the color icon to show the color picker.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        onView(withId(R.id.tab_group_color_icon)).perform(click());
        onView(
                        allOf(
                                instanceOf(TabGroupColorPickerContainer.class),
                                withId(R.id.color_picker_container)))
                .check(matches(isDisplayed()));

        // Select a non default color and assert the pop up closes.
        onView(withContentDescription(notSelectedStringBlue)).perform(click());
        onView(
                        allOf(
                                instanceOf(TabGroupColorPickerContainer.class),
                                withId(R.id.color_picker_container)))
                .check(doesNotExist());

        // Back press should close the color picker pop up.
        onView(withId(R.id.tab_group_color_icon)).perform(click());
        Espresso.pressBack();
        onView(
                        allOf(
                                instanceOf(TabGroupColorPickerContainer.class),
                                withId(R.id.color_picker_container)))
                .check(doesNotExist());

        // Clicking ScrimView should close the color picker pop up.
        onView(withId(R.id.tab_group_color_icon)).perform(click());
        clickScrimToExitDialog(cta);
        histograms.assertExpected();
    }

    @Test
    @MediumTest
    @EnableFeatures(TAB_GROUP_PARITY_ANDROID)
    @RequiresRestart("Group creation modal dialog is sometimes persistent when dismissing")
    public void testColorPickerOnToolbarMenuItemClick() throws ExecutionException {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();

        String blueColor =
                cta.getString(R.string.accessibility_tab_group_color_picker_color_item_blue);
        String notSelectedStringBlue =
                cta.getString(
                        R.string
                                .accessibility_tab_group_color_picker_color_item_not_selected_description,
                        blueColor);

        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Expect the edit color menu item to be clicked.
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabGroupParity.TabGroupColorChangeActionType", 1);

        // Open dialog and click the toolbar menu item to show the color picker.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        openDialogToolbarMenuAndVerify(cta);
        selectTabGridDialogToolbarMenuItem(cta, "Edit group color");
        onView(
                        allOf(
                                instanceOf(TabGroupColorPickerContainer.class),
                                withId(R.id.color_picker_container)))
                .check(matches(isDisplayed()));

        // Select a non default color and assert the pop up closes.
        onView(withContentDescription(notSelectedStringBlue)).perform(click());
        onView(
                        allOf(
                                instanceOf(TabGroupColorPickerContainer.class),
                                withId(R.id.color_picker_container)))
                .check(doesNotExist());
        clickScrimToExitDialog(cta);
        watcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testSelectionEditorShowHide() throws ExecutionException {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open dialog and open selection editor.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        openSelectionEditorAndVerify(cta, 2);

        // Click navigation button should close selection editor but not tab grid dialog.
        mSelectionEditorRobot.actionRobot.clickToolbarNavigationButton();
        mSelectionEditorRobot.resultRobot.verifyTabListEditorIsHidden();
        assertTrue(isDialogFullyVisible(cta));

        // Back press should close both theselectioneditor.
        openSelectionEditorAndVerify(cta, 2);
        Espresso.pressBack();
        mSelectionEditorRobot.resultRobot.verifyTabListEditorIsHidden();
        assertTrue(isDialogFullyVisible(cta));

        // Back press again to exit.
        Espresso.pressBack();
        waitForDialogHidingAnimationInTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Clicking ScrimView should close both the dialog and selection editor.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        openSelectionEditorAndVerify(cta, 2);
        clickScrimToExitDialog(cta);
        mSelectionEditorRobot.resultRobot.verifyTabListEditorIsHidden();
        waitForDialogHidingAnimationInTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);
    }

    @Test
    @MediumTest
    @EnableFeatures(TAB_GROUP_PARITY_ANDROID)
    @RequiresRestart("Group creation modal dialog is sometimes persistent when dismissing")
    public void testDialogToolbarSelectionEditor() throws ExecutionException {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
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
        openSelectionEditorAndVerify(cta, 2);

        // Click navigation button should close selection editor but not tab grid dialog.
        mSelectionEditorRobot.actionRobot.clickToolbarNavigationButton();
        mSelectionEditorRobot.resultRobot.verifyTabListEditorIsHidden();
        assertTrue(isDialogFullyVisible(cta));

        // Back press should close only the selection editor.
        openSelectionEditorAndVerify(cta, 2);
        Espresso.pressBack();
        mSelectionEditorRobot.resultRobot.verifyTabListEditorIsHidden();
        assertTrue(isDialogFullyVisible(cta));

        // Clicking ScrimView should close both the dialog and selection editor.
        openSelectionEditorAndVerify(cta, 2);
        clickScrimToExitDialog(cta);
        mSelectionEditorRobot.resultRobot.verifyTabListEditorIsHidden();
        waitForDialogHidingAnimationInTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);
    }

    @Test
    @MediumTest
    public void testDialogSelectionEditor_LongPressTabAndVerifyNoSelectionOccurs()
            throws ExecutionException {
        TabUiFeatureUtilities.setTabListEditorLongPressEntryEnabledForTesting(true);
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open the selection editor with longpress.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        onView(
                        allOf(
                                withId(R.id.tab_list_recycler_view),
                                withParent(withId(R.id.tab_grid_dialog_recycler_view_container))))
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, longClick()));

        mSelectionEditorRobot.resultRobot.verifyTabListEditorIsVisible();
        // Verify no selection action occurred to switch the selected tab in the tab model
        Criteria.checkThat(
                sActivityTestRule.getActivity().getCurrentTabModel().index(), Matchers.is(1));
    }

    @Test
    @MediumTest
    @DisableIf.Device(DeviceFormFactor.TABLET) // https://crbug.com/338998202
    public void testDialogSelectionEditor_PostLongPressClickNoSelectionEditor()
            throws ExecutionException {
        TabUiFeatureUtilities.setTabListEditorLongPressEntryEnabledForTesting(true);
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open the selection editor with longpress.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        onView(
                        allOf(
                                withId(R.id.tab_list_recycler_view),
                                withParent(withId(R.id.tab_grid_dialog_recycler_view_container))))
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, longClick()));

        mSelectionEditorRobot.resultRobot.verifyTabListEditorIsVisible();
        Espresso.pressBack();

        assertTrue(isDialogFullyVisible(cta));
        clickFirstTabInDialog(cta);
        waitForDialogHidingAnimation(cta);

        // Make sure tab switcher strip (and by extension a tab page) is showing to verify clicking
        // the tab worked.
        CriteriaHelper.pollUiThread(
                () ->
                        sActivityTestRule
                                        .getActivity()
                                        .getBrowserControlsManager()
                                        .getBottomControlOffset()
                                == 0);
        ViewUtils.waitForVisibleView(
                allOf(withId(R.id.toolbar_show_group_dialog_button), isCompletelyDisplayed()));
    }

    @Test
    @MediumTest
    public void testDialogSelectionEditor_BookmarkSingleTabView() {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        SnackbarManager snackbarManager = cta.getSnackbarManager();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        ThreadUtils.runOnUiThreadBlocking(() -> snackbarManager.dismissAllSnackbars());
        verifyTabSwitcherCardCount(cta, 1);

        // Open the selection editor.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        openSelectionEditorAndVerify(cta, 2);

        // Bookmark one tab and verify edit snackbar.
        mSelectionEditorRobot
                .actionRobot
                .clickItemAtAdapterPosition(0)
                .clickToolbarMenuButton()
                .clickToolbarMenuItem("Bookmark tab");

        onViewWaiting(
                allOf(
                        withId(R.id.snackbar_button),
                        isDescendantOfA(withId(R.id.selectable_list)),
                        isDisplayed()));
        onView(allOf(withId(R.id.snackbar), isDescendantOfA(withId(R.id.bottom_container))))
                .check(doesNotExist());
        onView(
                        allOf(
                                withId(R.id.snackbar_button),
                                isDescendantOfA(withId(R.id.selectable_list)),
                                isDisplayed()))
                .perform(click());

        BookmarkEditActivity activity = BookmarkTestUtil.waitForEditActivity();
        activity.finish();

        mSelectionEditorRobot.resultRobot.verifyTabListEditorIsVisible();
    }

    @Test
    @MediumTest
    public void testDialogSelectionEditor_BookmarkTabsView() {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        SnackbarManager snackbarManager = cta.getSnackbarManager();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        ThreadUtils.runOnUiThreadBlocking(() -> snackbarManager.dismissAllSnackbars());
        verifyTabSwitcherCardCount(cta, 1);

        // Open the selection editor.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        openSelectionEditorAndVerify(cta, 2);

        // Bookmark two tabs and verify edit snackbar.
        mSelectionEditorRobot
                .actionRobot
                .clickItemAtAdapterPosition(0)
                .clickItemAtAdapterPosition(1)
                .clickToolbarMenuButton()
                .clickToolbarMenuItem("Bookmark tabs");

        onViewWaiting(
                allOf(
                        withId(R.id.snackbar_button),
                        isDescendantOfA(withId(R.id.selectable_list)),
                        isDisplayed()));
        onView(allOf(withId(R.id.snackbar), isDescendantOfA(withId(R.id.bottom_container))))
                .check(doesNotExist());
        onView(
                        allOf(
                                withId(R.id.snackbar_button),
                                isDescendantOfA(withId(R.id.selectable_list)),
                                isDisplayed()))
                .perform(click());

        BookmarkEditActivity activity = BookmarkTestUtil.waitForEditActivity();
        activity.finish();

        mSelectionEditorRobot.resultRobot.verifyTabListEditorIsVisible();
    }

    @Test
    @MediumTest
    @RequiresRestart("Share sheet is sometimes persistent when calling pressBack to retract")
    public void testDialogSelectionEditor_ShareActionView() {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        createTabs(cta, false, 2);

        final String httpsCanonicalUrl =
                sActivityTestRule.getTestServer().getURL(PAGE_WITH_HTTPS_CANONICAL_URL);
        sActivityTestRule.loadUrl(httpsCanonicalUrl);

        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open dialog and verify dialog is showing correct content.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        openSelectionEditorAndVerify(cta, 2);

        // Share tabs
        mSelectionEditorRobot
                .actionRobot
                .clickItemAtAdapterPosition(1)
                .clickToolbarMenuButton()
                .clickToolbarMenuItem("Share tab");

        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                "Share sheet was not shown.",
                                sActivityTestRule
                                        .getActivity()
                                        .getRootUiCoordinatorForTesting()
                                        .getBottomSheetController(),
                                notNullValue()));

        intended(
                allOf(
                        hasAction(equalTo(Intent.ACTION_CHOOSER)),
                        hasExtras(
                                hasEntry(
                                        equalTo(Intent.EXTRA_INTENT),
                                        allOf(
                                                hasAction(equalTo(Intent.ACTION_SEND)),
                                                hasType("text/plain"))))));
    }

    @Test
    @MediumTest
    @RequiresRestart("Share sheet is sometimes persistent when calling pressBack to retract")
    public void testDialogSelectionEditor_ShareActionTabs() {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();

        final String httpsCanonicalUrl =
                sActivityTestRule.getTestServer().getURL(PAGE_WITH_HTTPS_CANONICAL_URL);
        sActivityTestRule.loadUrlInNewTab(httpsCanonicalUrl);

        final String httpCanonicalUrl =
                sActivityTestRule.getTestServer().getURL(PAGE_WITH_HTTP_CANONICAL_URL);
        sActivityTestRule.loadUrlInNewTab(httpCanonicalUrl);

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
        openSelectionEditorAndVerify(cta, 3);

        TabListEditorShareAction.setIntentCallbackForTesting(
                result -> {
                    assertEquals(Intent.ACTION_SEND, result.getAction());
                    assertEquals(String.join("\n", urls), result.getStringExtra(Intent.EXTRA_TEXT));
                    assertEquals("text/plain", result.getType());
                    assertEquals("2 links from Chrome", result.getStringExtra(Intent.EXTRA_TITLE));
                });

        // Share tabs
        mSelectionEditorRobot
                .actionRobot
                .clickItemAtAdapterPosition(1)
                .clickItemAtAdapterPosition(2)
                .clickToolbarMenuButton()
                .clickToolbarMenuItem("Share tabs");
    }

    @Test
    @MediumTest
    public void testDialogSelectionEditor_ShareActionAllFilterableTabs() {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        createTabs(cta, false, 2);

        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open dialog and verify dialog is showing correct content.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        openSelectionEditorAndVerify(cta, 2);

        mSelectionEditorRobot
                .actionRobot
                .clickItemAtAdapterPosition(0)
                .clickItemAtAdapterPosition(1)
                .clickToolbarMenuButton();

        // Check share tabs disabled
        onView(withText("Share tabs")).check(matches(not(isClickable())));
    }

    @Test
    @MediumTest
    public void testDialogSelectionEditor_UndoClose() throws ExecutionException {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        createTabs(cta, false, 4);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 4);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open the selection editor.
        openDialogFromTabSwitcherAndVerify(cta, 4, null);
        openSelectionEditorAndVerify(cta, 4);

        // Close two tabs and undo.
        mSelectionEditorRobot
                .actionRobot
                .clickItemAtAdapterPosition(0)
                .clickItemAtAdapterPosition(2)
                .clickToolbarMenuButton()
                .clickToolbarMenuItem("Close tabs");
        mSelectionEditorRobot.resultRobot.verifyTabListEditorIsHidden();
        verifyShowingDialog(cta, 2, null);
        verifyDialogUndoBarAndClick();
        verifyShowingDialog(cta, 4, null);

        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimationInTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);
    }

    @Test
    @MediumTest
    public void testDialogSelectionEditor_UndoCloseAll() {
        // This test relies on the undo bar, which is only present when the confirmation dialog is
        // not shown.
        ThreadUtils.runOnUiThreadBlocking(
                () -> ActionConfirmationManager.setAllStopShowingPrefsForTesting(mPrefService));

        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        createTabs(cta, false, 4);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 4);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open the selection editor.
        openDialogFromTabSwitcherAndVerify(cta, 4, null);
        openSelectionEditorAndVerify(cta, 4);

        // Close two tabs and undo.
        mSelectionEditorRobot
                .actionRobot
                .clickItemAtAdapterPosition(0)
                .clickItemAtAdapterPosition(1)
                .clickItemAtAdapterPosition(2)
                .clickItemAtAdapterPosition(3)
                .clickToolbarMenuButton()
                .clickToolbarMenuItem("Close tabs");

        mSelectionEditorRobot.resultRobot.verifyTabListEditorIsHidden();
        waitForDialogHidingAnimationInTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 0);

        verifyGlobalUndoBarAndClick();
        verifyTabSwitcherCardCount(cta, 1);
    }

    @Test
    @MediumTest
    @RequiresRestart("crbug.com/344674734")
    public void testDialogSelectionEditor_UngroupAll() {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        createTabs(cta, false, 4);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 4);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open the selection editor.
        openDialogFromTabSwitcherAndVerify(cta, 4, null);
        openSelectionEditorAndVerify(cta, 4);

        // Ungroup all four tabs.
        mSelectionEditorRobot
                .actionRobot
                .clickItemAtAdapterPosition(0)
                .clickItemAtAdapterPosition(1)
                .clickItemAtAdapterPosition(2)
                .clickItemAtAdapterPosition(3)
                .clickToolbarMenuButton()
                .clickToolbarMenuItem("Ungroup tabs");

        clickThroughConfirmationDialog();

        mSelectionEditorRobot.resultRobot.verifyTabListEditorIsHidden();
        waitForDialogHidingAnimationInTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 4);
    }

    @Test
    @MediumTest
    public void testSwipeToDismiss_Dialog() {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        // Create 2 tabs and merge them into one group.
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        openDialogFromTabSwitcherAndVerify(cta, 2, null);

        // Swipe to dismiss two tabs in dialog.
        onView(
                        allOf(
                                withId(R.id.tab_list_recycler_view),
                                withParent(withId(R.id.tab_grid_dialog_recycler_view_container))))
                .perform(
                        RecyclerViewActions.actionOnItemAtPosition(
                                1, getSwipeToDismissAction(true)));
        verifyShowingDialog(cta, 1, null);
        onView(
                        allOf(
                                withId(R.id.tab_list_recycler_view),
                                withParent(withId(R.id.tab_grid_dialog_recycler_view_container))))
                .perform(
                        RecyclerViewActions.actionOnItemAtPosition(
                                0, getSwipeToDismissAction(false)));

        clickThroughConfirmationDialog();

        waitForDialogHidingAnimation(cta);
        verifyTabSwitcherCardCount(cta, 0);
    }

    @Test
    @MediumTest
    @Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
    public void testSelectionEditorPosition() {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();

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

        // Verify the size and position of TabListEditor in portrait mode.
        openSelectionEditorAndVerify(cta, 3);
        checkPosition(cta, false, true);

        // Verify the size and position of TabListEditor in landscape mode.
        ActivityTestUtils.rotateActivityToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);
        CriteriaHelper.pollUiThread(() -> parentView.getHeight() < parentView.getWidth());
        checkPosition(cta, false, false);

        // Verify the size and position of TabGridDialog in landscape mode.
        mSelectionEditorRobot.actionRobot.clickToolbarNavigationButton();
        mSelectionEditorRobot.resultRobot.verifyTabListEditorIsHidden();
        assertTrue(isDialogFullyVisible(cta));
        checkPosition(cta, true, false);

        // Verify the positioning in multi-window mode. Adjusting the height of the root view to
        // mock entering/exiting multi-window mode.
        ActivityTestUtils.rotateActivityToOrientation(cta, Configuration.ORIENTATION_PORTRAIT);
        CriteriaHelper.pollUiThread(() -> parentView.getHeight() > parentView.getWidth());
        View rootView = cta.findViewById(R.id.coordinator);
        int rootViewHeight = rootView.getHeight();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ViewGroup.LayoutParams params = rootView.getLayoutParams();
                    params.height = rootViewHeight / 2;
                    rootView.setLayoutParams(params);
                });
        checkPosition(cta, true, true);
        openSelectionEditorAndVerify(cta, 3);
        checkPosition(cta, false, true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ViewGroup.LayoutParams params = rootView.getLayoutParams();
                    params.height = rootViewHeight;
                    rootView.setLayoutParams(params);
                });
        checkPosition(cta, false, true);
        checkPosition(cta, true, true);
    }

    @Test
    @MediumTest
    public void testTabGroupNaming() throws ExecutionException {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open dialog and modify group title.
        openDialogFromTabSwitcherAndVerify(
                cta,
                2,
                cta.getResources()
                        .getQuantityString(R.plurals.bottom_tab_grid_title_placeholder, 2, 2));
        editDialogTitle(cta, CUSTOMIZED_TITLE1);

        // Verify the title is updated in both tab switcher and dialog.
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimation(cta);
        verifyFirstCardTitle(CUSTOMIZED_TITLE1);
        openDialogFromTabSwitcherAndVerify(cta, 2, CUSTOMIZED_TITLE1);

        if (isPhone()) {
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
    @EnableFeatures(TAB_GROUP_PARITY_ANDROID)
    @RequiresRestart("Group creation modal dialog is sometimes persistent when dismissing")
    public void testTabGroupNaming_KeyboardVisibility() throws ExecutionException {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        openDialogFromTabSwitcherAndVerify(
                cta,
                2,
                cta.getResources()
                        .getQuantityString(R.plurals.bottom_tab_grid_title_placeholder, 2, 2));

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
    @DisabledTest(message = "TODO(crbug.com/359632348): Fix flakiness.")
    public void testTabGroupNaming_afterFocusNoTitleSaved() throws ExecutionException {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        createTabs(cta, false, 3);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        openDialogFromTabSwitcherAndVerify(
                cta,
                3,
                cta.getResources()
                        .getQuantityString(R.plurals.bottom_tab_grid_title_placeholder, 3, 3));

        // Click on the title this should not save the title.
        onView(allOf(isDescendantOfA(withId(R.id.main_content)), withId(R.id.title)))
                .perform(click());
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
        String twoTabsString =
                cta.getResources()
                        .getQuantityString(R.plurals.bottom_tab_grid_title_placeholder, 2, 2);
        verifyFirstCardTitle(twoTabsString);
        openDialogFromTabSwitcherAndVerify(cta, 2, twoTabsString);

        // Click on the title.
        onView(allOf(isDescendantOfA(withId(R.id.main_content)), withId(R.id.title)))
                .perform(click());
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
    public void testTabGroupNaming_afterMergeWithSelectionEditor() throws ExecutionException {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        createTabs(cta, false, 4);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 4);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open dialog and modify group title.
        openDialogFromTabSwitcherAndVerify(
                cta,
                4,
                cta.getResources()
                        .getQuantityString(R.plurals.bottom_tab_grid_title_placeholder, 4, 4));
        editDialogTitle(cta, CUSTOMIZED_TITLE1);

        // Verify the title is updated in both tab switcher and dialog.
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimation(cta);
        verifyFirstCardTitle(CUSTOMIZED_TITLE1);
        openDialogFromTabSwitcherAndVerify(cta, 4, CUSTOMIZED_TITLE1);
        openSelectionEditorAndVerify(cta, 4);

        // Ungroup tab.
        mSelectionEditorRobot
                .actionRobot
                .clickItemAtAdapterPosition(1)
                .clickItemAtAdapterPosition(2)
                .clickToolbarMenuButton()
                .clickToolbarMenuItem("Ungroup tabs");
        mSelectionEditorRobot.resultRobot.verifyTabListEditorIsHidden();

        // Verify the ungroup occurred.
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimation(cta);
        verifyFirstCardTitle(CUSTOMIZED_TITLE1);
        verifyTabSwitcherCardCount(cta, 3);

        enterTabListEditor(cta);
        mSelectionEditorRobot.resultRobot.verifyTabListEditorIsVisible();
        mSelectionEditorRobot
                .actionRobot
                .clickItemAtAdapterPosition(0)
                .clickItemAtAdapterPosition(1)
                .clickItemAtAdapterPosition(2)
                .clickToolbarMenuButton()
                .clickToolbarMenuItem("Group tabs");
        mSelectionEditorRobot.resultRobot.verifyTabListEditorIsHidden();

        // Verify the group worked and the title remained.
        verifyFirstCardTitle(CUSTOMIZED_TITLE1);
        openDialogFromTabSwitcherAndVerify(cta, 4, CUSTOMIZED_TITLE1);
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimation(cta);
    }

    @Test
    @MediumTest
    @DisableIf.Build(
            sdk_is_greater_than = VERSION_CODES.N_MR1,
            message = "https://crbug.com/1124336")
    @DisableIf.Build(supported_abis_includes = "x86", message = "https://crbug.com/1124336")
    @DisableIf.Device(DeviceFormFactor.TABLET)
    public void testDialogInitialShowFromStrip() throws Exception {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        prepareTabsWithThumbnail(sActivityTestRule, 2, 0, "about:blank");
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Restart the activity and open the dialog from strip to check the initial setup of dialog.
        finishActivity(cta);
        sActivityTestRule.startMainActivityFromLauncher();
        CriteriaHelper.pollUiThread(
                sActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);
        openDialogFromStripAndVerify(sActivityTestRule.getActivity(), 2, null);
        closeNthTabInDialog(0);
        verifyShowingDialog(cta, 1, null);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures({TAB_GROUP_PARITY_ANDROID})
    @RequiresRestart("Group creation modal dialog is sometimes persistent when dismissing")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testRenderDialog_3Tabs_Portrait(boolean nightModeEnabled) throws Exception {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        prepareTabsWithThumbnail(sActivityTestRule, 3, 0, "about:blank");
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);
        waitForThumbnailsToFetch(getRecyclerView(cta));
        verifyAllTabsHaveThumbnail(cta.getCurrentTabModel());

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        openDialogFromTabSwitcherAndVerify(cta, 3, null);

        View dialogView = cta.findViewById(R.id.dialog_parent_view);
        waitForThumbnailsToFetch(
                (RecyclerView) dialogView.findViewById(R.id.tab_list_recycler_view));
        mRenderTestRule.render(dialogView, "3_tabs_portrait");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures({TAB_GROUP_PARITY_ANDROID})
    @RequiresRestart("Group creation modal dialog is sometimes persistent when dismissing")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testRenderDialog_3Tabs_Landscape_NewAspectRatio(boolean nightModeEnabled)
            throws Exception {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        prepareTabsWithThumbnail(sActivityTestRule, 3, 0, "about:blank");
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);
        waitForThumbnailsToFetch(getRecyclerView(cta));
        verifyAllTabsHaveThumbnail(cta.getCurrentTabModel());

        // Rotate to landscape mode and create a tab group.
        ActivityTestUtils.rotateActivityToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        openDialogFromTabSwitcherAndVerify(cta, 3, null);

        View dialogView = cta.findViewById(R.id.dialog_parent_view);
        waitForThumbnailsToFetch(
                (RecyclerView) dialogView.findViewById(R.id.tab_list_recycler_view));
        mRenderTestRule.render(dialogView, "3_tabs_landscape_new_aspect_ratio");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures({TAB_GROUP_PARITY_ANDROID})
    @RequiresRestart("Group creation modal dialog is sometimes persistent when dismissing")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testRenderDialog_5Tabs_InitialScroll(boolean nightModeEnabled) throws Exception {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        prepareTabsWithThumbnail(sActivityTestRule, 5, 0, "about:blank");
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 5);
        waitForThumbnailsToFetch(getRecyclerView(cta));
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
        waitForThumbnailsToFetch(
                (RecyclerView) dialogView.findViewById(R.id.tab_list_recycler_view));
        mRenderTestRule.render(dialogView, "5_tabs_select_last");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures(TAB_GROUP_PARITY_ANDROID)
    @DisableFeatures({ChromeFeatureList.LOGO_POLISH})
    @RequiresRestart("Group creation modal dialog is sometimes persistent when dismissing")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testRenderDialog_TabGroupColorChange(boolean nightModeEnabled) throws Exception {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();

        String blueColor =
                cta.getString(R.string.accessibility_tab_group_color_picker_color_item_blue);
        String notSelectedStringBlue =
                cta.getString(
                        R.string
                                .accessibility_tab_group_color_picker_color_item_not_selected_description,
                        blueColor);

        String redColor =
                cta.getString(R.string.accessibility_tab_group_color_picker_color_item_red);
        String notSelectedStringRed =
                cta.getString(
                        R.string
                                .accessibility_tab_group_color_picker_color_item_not_selected_description,
                        redColor);

        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open dialog and click the color icon to show the color picker.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        onView(withId(R.id.tab_group_color_icon)).perform(click());
        onView(
                        allOf(
                                instanceOf(TabGroupColorPickerContainer.class),
                                withId(R.id.color_picker_container)))
                .check(matches(isDisplayed()));

        // Select a non default color and assert the pop up closes.
        onView(withContentDescription(notSelectedStringBlue)).perform(click());
        onView(
                        allOf(
                                instanceOf(TabGroupColorPickerContainer.class),
                                withId(R.id.color_picker_container)))
                .check(doesNotExist());

        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimationInTabSwitcher(cta);
        View dialogView = cta.findViewById(R.id.dialog_parent_view);
        waitForThumbnailsToFetch(getRecyclerView(cta));
        // Take the GTS first snapshot, which should have the second color (blue) shown.
        mRenderTestRule.render(getRecyclerView(cta), "GTS_tab_group_color_initial");

        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        waitForThumbnailsToFetch(
                (RecyclerView) dialogView.findViewById(R.id.tab_list_recycler_view));
        // Take the dialog first snapshot, which should have the second color (blue) shown.
        mRenderTestRule.render(dialogView, "dialog_tab_group_color_initial");

        onView(withId(R.id.tab_group_color_icon)).perform(click());
        onView(
                        allOf(
                                instanceOf(TabGroupColorPickerContainer.class),
                                withId(R.id.color_picker_container)))
                .check(matches(isDisplayed()));

        // Select a non default color and assert the pop up closes.
        onView(withContentDescription(notSelectedStringRed)).perform(click());
        onView(
                        allOf(
                                instanceOf(TabGroupColorPickerContainer.class),
                                withId(R.id.color_picker_container)))
                .check(doesNotExist());

        waitForThumbnailsToFetch(
                (RecyclerView) dialogView.findViewById(R.id.tab_list_recycler_view));
        // Take the dialog second snapshot, which should have the third color (red) shown.
        mRenderTestRule.render(dialogView, "dialog_tab_group_color_changed");

        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimationInTabSwitcher(cta);
        waitForThumbnailsToFetch(getRecyclerView(cta));
        // Take the GTS second snapshot, which should have the third color (red) shown.
        mRenderTestRule.render(getRecyclerView(cta), "GTS_tab_group_color_changed");
    }

    @Test
    @MediumTest
    public void testAdjustBackGroundViewAccessibilityImportance() {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
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
    @DisabledTest(message = "TODO(crbug.com/40148943): Fix flakiness.")
    public void testAccessibilityString() throws ExecutionException {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        createTabs(cta, false, 3);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Verify the initial group card content description.
        RecyclerView recyclerView = cta.findViewById(R.id.tab_list_recycler_view);
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
    @DisableIf.Device(DeviceFormFactor.TABLET)
    public void testStripDialog_TabListEditorCloseAll_NoCustomHomepage() {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
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
        CriteriaHelper.pollUiThread(
                () ->
                        sActivityTestRule
                                        .getActivity()
                                        .getBrowserControlsManager()
                                        .getBottomControlOffset()
                                == 0);
        ViewUtils.waitForVisibleView(
                allOf(withId(R.id.toolbar_show_group_dialog_button), isCompletelyDisplayed()));

        // Test opening dialog from strip and from tab switcher.
        openDialogFromStripAndVerify(cta, 2, null);
        openSelectionEditorAndVerify(cta, 2);

        // Close two tabs.
        mSelectionEditorRobot
                .actionRobot
                .clickItemAtAdapterPosition(0)
                .clickItemAtAdapterPosition(1)
                .clickToolbarMenuButton()
                .clickToolbarMenuItem("Close tabs");

        clickThroughConfirmationDialog();

        // Rather than destroying the activity the GTS should be showing.
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.TAB_SWITCHER);
        verifyTabSwitcherCardCount(cta, 0);
    }

    @Test
    @MediumTest
    @DisableIf.Device(DeviceFormFactor.TABLET)
    public void testStripDialog_TabListEditorCloseAll_CustomHomepage() {
        GURL url =
                new GURL(
                        sActivityTestRule
                                .getEmbeddedTestServerRule()
                                .getServer()
                                .getURL("/chrome/test/data/android/google.html"));
        when(mHomepagePolicyManager.isHomepageLocationPolicyEnabled()).thenReturn(true);
        when(mHomepagePolicyManager.getHomepagePreference()).thenReturn(url);

        HomepagePolicyManager.setInstanceForTests(mHomepagePolicyManager);
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
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
        CriteriaHelper.pollUiThread(
                () ->
                        sActivityTestRule
                                        .getActivity()
                                        .getBrowserControlsManager()
                                        .getBottomControlOffset()
                                == 0);
        ViewUtils.waitForVisibleView(
                allOf(withId(R.id.toolbar_show_group_dialog_button), isCompletelyDisplayed()));

        // Test opening dialog from strip and from tab switcher.
        openDialogFromStripAndVerify(cta, 2, null);
        openSelectionEditorAndVerify(cta, 2);

        // Close two tabs.
        mSelectionEditorRobot
                .actionRobot
                .clickItemAtAdapterPosition(0)
                .clickItemAtAdapterPosition(1)
                .clickToolbarMenuButton()
                .clickToolbarMenuItem("Close tabs");

        clickThroughConfirmationDialog();

        // With a custom homepage exit the app.
        CriteriaHelper.pollUiThread(() -> cta.isDestroyed());
    }

    @Test
    @MediumTest
    @DisableIf.Device(DeviceFormFactor.TABLET)
    @RequiresRestart
    public void testDialogSetup_WithStartSurface() throws Exception {
        // Create a tab group with 2 tabs.
        finishActivity(sActivityTestRule.getActivity());
        createThumbnailBitmapAndWriteToFile(0, mBrowserControlsStateProvider);
        createThumbnailBitmapAndWriteToFile(1, mBrowserControlsStateProvider);
        createTabStatesAndMetadataFile(new int[] {0, 1}, new int[] {0, 0});

        // Restart Chrome and make sure tab strip is showing.
        sActivityTestRule.startMainActivityFromLauncher();
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(cta.getTabModelSelector()::isTabStateInitialized);
        CriteriaHelper.pollUiThread(
                () ->
                        sActivityTestRule
                                        .getActivity()
                                        .getBrowserControlsManager()
                                        .getBottomControlOffset()
                                == 0);
        ViewUtils.waitForVisibleView(
                allOf(withId(R.id.toolbar_show_group_dialog_button), isCompletelyDisplayed()));

        // Test opening dialog from strip and from tab switcher.
        openDialogFromStripAndVerify(cta, 2, null);
        Espresso.pressBack();

        // Tab switcher is created, and a fake signal to hide dialog is sent. This line would
        // crash if the fake signal is not properly handled. See crbug.com/1096358.
        enterTabSwitcher(cta);
        onView(
                        allOf(
                                isDescendantOfA(
                                        withId(
                                                getTabSwitcherAncestorId(
                                                        sActivityTestRule.getActivity()))),
                                withId(R.id.tab_list_recycler_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, click()));
        CriteriaHelper.pollUiThread(() -> isDialogFullyVisible(sActivityTestRule.getActivity()));
        verifyShowingDialog(cta, 2, null);
    }

    @Test
    @MediumTest
    public void testCreateTabInDialog() {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        // Open dialog from tab switcher and verify dialog is showing correct content.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);

        // Create a tab by tapping "+" on the dialog.
        onView(
                        allOf(
                                withId(R.id.toolbar_new_tab_button),
                                isDescendantOfA(withId(R.id.tab_grid_dialog_toolbar_container))))
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
            onView(
                            allOf(
                                    withId(R.id.toolbar_new_tab_button),
                                    isDescendantOfA(
                                            withId(R.id.tab_grid_dialog_toolbar_container))))
                    .perform(click());
            waitForDialogHidingAnimation(cta);
            LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);

            openDialogFromStripAndVerify(cta, 4, null);
        }
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures({
        DATA_SHARING,
        TAB_GROUP_PARITY_ANDROID,
        TAB_GROUP_SYNC_ANDROID,
        TAB_GROUP_PANE_ANDROID
    })
    @RequiresRestart("Group creation modal dialog is sometimes persistent when dismissing")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    @DisabledTest(message = "crbug.com/362762206, see also crbug.com/360072870")
    public void testRenderDialog_TwoRows_Portrait(boolean nightModeEnabled) throws Exception {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        prepareTabsWithThumbnail(sActivityTestRule, 3, 0, "about:blank");
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);
        waitForThumbnailsToFetch(getRecyclerView(cta));
        verifyAllTabsHaveThumbnail(cta.getCurrentTabModel());

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        openDialogFromTabSwitcherAndVerify(cta, 3, null);

        View dialogView = cta.findViewById(R.id.dialog_parent_view);
        waitForThumbnailsToFetch(
                (RecyclerView) dialogView.findViewById(R.id.tab_list_recycler_view));
        mRenderTestRule.render(dialogView, "3_tabs_portrait_2_row_toolbar_share_button");

        onView(allOf(isDescendantOfA(withId(R.id.dialog_parent_view)), withId(R.id.share_button)))
                .perform(click());
        // Dismiss the bottom sheet.
        Espresso.pressBack();

        mRenderTestRule.render(dialogView, "3_tabs_portrait_2_row_toolbar_image_tiles");
    }

    @Test
    @MediumTest
    @EnableAnimations
    public void testCreateIncognitoGroupAndCloseAllTabsInDialogTwice_Bug354745444() {
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        boolean incognito = true;
        int tabCount = 2;
        TabModel incognitoTabModel = cta.getTabModelSelectorSupplier().get().getModel(incognito);
        createTabs(cta, incognito, tabCount);
        enterTabSwitcher(cta);
        List<Tab> tabGroup = List.of(incognitoTabModel.getTabAt(0), incognitoTabModel.getTabAt(1));
        createTabGroup(cta, incognito, tabGroup);
        openDialogFromTabSwitcherAndVerify(cta, tabCount, /* customizedTitle= */ null);
        closeFirstTabInDialog();
        closeFirstTabInDialog();
        waitForDialogHidingAnimation(cta);

        leaveTabSwitcher(cta);
        createTabs(cta, incognito, tabCount);
        enterTabSwitcher(cta);
        tabGroup = List.of(incognitoTabModel.getTabAt(0), incognitoTabModel.getTabAt(1));
        createTabGroup(cta, incognito, tabGroup);
        openDialogFromTabSwitcherAndVerify(cta, tabCount, /* customizedTitle= */ null);
        closeFirstTabInDialog();
        closeFirstTabInDialog();
        waitForDialogHidingAnimation(cta);
    }

    private void openDialogFromTabSwitcherAndVerify(
            ChromeTabbedActivity cta, int tabCount, String customizedTitle) {
        clickFirstCardFromTabSwitcher(cta);
        CriteriaHelper.pollUiThread(() -> isDialogFullyVisible(cta));
        verifyShowingDialog(cta, tabCount, customizedTitle);
    }

    private void openDialogFromStripAndVerify(
            ChromeTabbedActivity cta, int tabCount, String customizedTitle) {
        showDialogFromStrip(cta);
        CriteriaHelper.pollUiThread(() -> isDialogFullyVisible(cta));
        verifyShowingDialog(cta, tabCount, customizedTitle);
    }

    private void verifyShowingDialog(
            ChromeTabbedActivity cta, int tabCount, String customizedTitle) {
        onView(
                        allOf(
                                withId(R.id.tab_list_recycler_view),
                                withParent(withId(R.id.tab_grid_dialog_recycler_view_container))))
                .check(matches(isDisplayed()))
                .check(TabUiTestHelper.ChildrenCountAssertion.havingTabCount(tabCount));

        // Check contents within dialog.
        onView(allOf(isDescendantOfA(withId(R.id.main_content)), withId(R.id.title)))
                .check(
                        (v, noMatchException) -> {
                            if (noMatchException != null) throw noMatchException;

                            Assert.assertTrue(v instanceof EditText);
                            EditText titleText = (EditText) v;
                            String title =
                                    customizedTitle == null
                                            ? cta.getResources()
                                                    .getQuantityString(
                                                            R.plurals
                                                                    .bottom_tab_grid_title_placeholder,
                                                            tabCount,
                                                            tabCount)
                                            : customizedTitle;
                            Assert.assertEquals(title, titleText.getText().toString());
                            assertFalse(v.isFocused());
                        });

        // Check views used for animations are not visible.
        onView(allOf(withParent(withId(R.id.dialog_parent_view)), withId(R.id.dialog_frame)))
                .check((v, e) -> assertEquals(0f, v.getAlpha(), 0.0));
        onView(
                        allOf(
                                isDescendantOfA(withId(R.id.dialog_parent_view)),
                                withId(R.id.dialog_animation_card_view)))
                .check((v, e) -> assertEquals(0f, v.getAlpha(), 0.0));

        // For devices with version higher or equal to O_MR1 and use light color navigation bar,
        // make sure that the color of navigation bar is changed by dialog scrim.
        // Skip if Chrome is drawing edge to edge as navigation bar will stay transparent.
        Resources resources = cta.getResources();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O_MR1
                || !resources.getBoolean(R.bool.window_light_navigation_bar)
                || isTablet(cta)
                || cta.getTabModelSelectorSupplier().get().isIncognitoBrandedModelSelected()) {
            return;
        }

        if (!EdgeToEdgeUtils.isEnabled()) {
            @ColorInt int scrimDefaultColor = cta.getColor(R.color.default_scrim_color);
            @ColorInt int navigationBarColor = SemanticColorUtils.getBottomSystemNavColor(cta);
            @ColorInt
            int navigationBarColorWithScrimOverlay =
                    ColorUtils.overlayColor(navigationBarColor, scrimDefaultColor);
            assertEquals(
                    navigationBarColorWithScrimOverlay, cta.getWindow().getNavigationBarColor());
            assertNotEquals(navigationBarColor, navigationBarColorWithScrimOverlay);
        } else if (cta.getEdgeToEdgeSupplier().get() != null
                && cta.getEdgeToEdgeSupplier().get().isDrawingToEdge()) {
            assertEquals(Color.TRANSPARENT, cta.getWindow().getNavigationBarColor());
        }
    }

    private boolean isPhone() {
        return !isTablet(sActivityTestRule.getActivity());
    }

    private boolean isDialogFullyVisible(ChromeTabbedActivity cta) {
        View dialogView = cta.findViewById(R.id.dialog_parent_view);
        View dialogContainerView = cta.findViewById(R.id.dialog_container_view);
        return dialogView.getVisibility() == View.VISIBLE && dialogContainerView.getAlpha() == 1f;
    }

    private boolean isDialogHidden(ChromeTabbedActivity cta) {
        View dialogView = cta.findViewById(R.id.dialog_parent_view);
        return dialogView.getVisibility() == View.GONE;
    }

    private void showDialogFromStrip(ChromeTabbedActivity cta) {
        assertFalse(cta.getLayoutManager().isLayoutVisible(LayoutType.TAB_SWITCHER));
        waitForVisibleView(
                allOf(
                        withId(R.id.tab_list_recycler_view),
                        isDescendantOfA(withId(R.id.bottom_controls)),
                        isCompletelyDisplayed()));
        onViewWaiting(
                        allOf(
                                withId(R.id.toolbar_show_group_dialog_button),
                                isDescendantOfA(withId(R.id.bottom_controls))))
                .perform(click());
    }

    private void verifyTabGroupDialogUi(ChromeTabbedActivity cta) {

        // Verify the menu button exists.
        onView(withId(R.id.toolbar_menu_button)).check(matches(isDisplayed()));

        // Verify the color icon exists.
        onView(withId(R.id.tab_group_color_icon)).check(matches(isDisplayed()));

        // Try to grab focus of the title text field by clicking on it.
        onView(allOf(isDescendantOfA(withId(R.id.main_content)), withId(R.id.title)))
                .perform(click());
        onView(allOf(isDescendantOfA(withId(R.id.main_content)), withId(R.id.title)))
                .check(
                        (v, noMatchException) -> {
                            if (noMatchException != null) throw noMatchException;

                            // Verify if we can grab focus on the editText or not.
                            assertTrue(v.isFocused());
                        });
        // Verify if the keyboard shows or not.
        CriteriaHelper.pollUiThread(
                () ->
                        cta.getWindowAndroid()
                                .getKeyboardDelegate()
                                .isKeyboardShowing(cta, cta.getCompositorViewHolderForTesting()));
    }

    private void openDialogToolbarMenuAndVerify(ChromeTabbedActivity cta) {
        onView(withId(R.id.toolbar_menu_button)).perform(click());
        onView(withId(R.id.tab_group_action_menu_list))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .check(
                        (v, noMatchException) -> {
                            if (noMatchException != null) throw noMatchException;
                            Assert.assertTrue(v instanceof ListView);
                            ListView listView = (ListView) v;
                            verifyTabGridDialogToolbarMenuItem(
                                    listView, 0, cta.getString(R.string.menu_select_tabs));
                            verifyTabGridDialogToolbarMenuItem(
                                    listView,
                                    1,
                                    cta.getString(
                                            R.string.tab_grid_dialog_toolbar_edit_group_name));
                            verifyTabGridDialogToolbarMenuItem(
                                    listView,
                                    2,
                                    cta.getString(
                                            R.string.tab_grid_dialog_toolbar_edit_group_color));
                            verifyTabGridDialogToolbarMenuItem(
                                    listView,
                                    3,
                                    cta.getString(R.string.tab_grid_dialog_toolbar_close_group));
                            int itemCount = 4;
                            boolean shouldShowDelete = isTabGroupSyncEnabled(cta);
                            if (shouldShowDelete) {
                                verifyTabGridDialogToolbarMenuItem(
                                        listView,
                                        4,
                                        cta.getString(
                                                R.string.tab_grid_dialog_toolbar_delete_group));
                                itemCount++;
                            }
                            assertEquals(itemCount, listView.getCount());
                        });
    }

    private boolean isTabGroupSyncEnabled(ChromeTabbedActivity cta) {
        Profile profile = cta.getTabModelSelectorSupplier().get().getCurrentModel().getProfile();
        if (profile.isOffTheRecord()) return false;
        return TabGroupSyncFeatures.isTabGroupSyncEnabled(profile);
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

    private void waitForDialogHidingAnimation(ChromeTabbedActivity cta) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(isDialogHidden(cta), Matchers.is(true));
                });
    }

    private void waitForDialogHidingAnimationInTabSwitcher(ChromeTabbedActivity cta) {
        waitForDialogHidingAnimation(cta);
        // Animation source card becomes alpha = 0f when dialog is showing and animates back to 1f
        // when dialog hides. Make sure the source card has restored its alpha change.
        CriteriaHelper.pollUiThread(
                () -> {
                    RecyclerView recyclerView = cta.findViewById(R.id.tab_list_recycler_view);
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
        // Open tab selection editor by selecting the select tabs item in tab grid dialog menu.
        onView(withId(R.id.toolbar_menu_button)).perform(click());
        onView(withText(cta.getString(R.string.menu_select_tabs)))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .perform(click());

        mSelectionEditorRobot
                .resultRobot
                .verifyTabListEditorIsVisible()
                .verifyToolbarSelectionTextWithResourceId(
                        R.string.tab_selection_editor_toolbar_select_tabs)
                .verifyAdapterHasItemCount(count);
    }

    private void dismissAllModalDialogs() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModalDialogManager.dismissAllDialogs(DialogDismissalCause.UNKNOWN);
                });
    }

    private void checkPosition(ChromeTabbedActivity cta, boolean isDialog, boolean isPortrait) {
        // If isDialog is true, we are checking the position of TabGridDialog; otherwise we are
        // checking the position of TabListEditor.
        int contentViewId = isDialog ? R.id.dialog_container_view : R.id.selectable_list;
        int minMargin =
                cta.getResources().getDimensionPixelSize(R.dimen.tab_grid_dialog_min_margin);
        int maxMargin =
                cta.getResources().getDimensionPixelSize(R.dimen.tab_grid_dialog_max_margin);
        View parentView = cta.getCompositorViewHolderForTesting();
        Rect parentRect = new Rect();
        parentView.getGlobalVisibleRect(parentRect);
        int[] parentLoc = new int[2];
        parentView.getLocationOnScreen(parentLoc);
        onView(withId(contentViewId))
                .check(
                        (v, e) -> {
                            int[] location = new int[2];
                            v.getLocationOnScreen(location);
                            int side = location[0] - parentLoc[0];
                            int top = location[1] - parentLoc[1];
                            // Check the position.
                            if (isPortrait) {
                                assertEquals(side, minMargin);
                                assertThat(
                                        top,
                                        allOf(
                                                greaterThanOrEqualTo(minMargin),
                                                lessThanOrEqualTo(maxMargin)));
                            } else {
                                assertThat(
                                        side,
                                        allOf(
                                                greaterThanOrEqualTo(minMargin),
                                                lessThanOrEqualTo(maxMargin)));
                                assertEquals(top, minMargin);
                            }
                            // Check the size.
                            assertEquals(parentView.getHeight() - 2 * top, v.getHeight());
                            assertEquals(parentView.getWidth() - 2 * side, v.getWidth());
                        });
    }

    private void editDialogTitle(ChromeTabbedActivity cta, String title) {
        onView(allOf(isDescendantOfA(withId(R.id.main_content)), withId(R.id.title)))
                .perform(click())
                .check(
                        (v, e) -> {
                            // Verify all texts in the field are selected.
                            EditText titleView = (EditText) v;
                            assertEquals(
                                    titleView.getText().length(),
                                    titleView.getSelectionEnd() - titleView.getSelectionStart());
                        })
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
                                                        sActivityTestRule.getActivity()))),
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

    private void clickScrimToExitDialog(ChromeTabbedActivity cta) throws ExecutionException {
        CriteriaHelper.pollUiThread(() -> isDialogFullyVisible(cta));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    View scrimView;
                    if (isTablet(cta)) {
                        TabGridDialogView dialogView = cta.findViewById(R.id.dialog_parent_view);
                        scrimView = dialogView.getScrimCoordinatorForTesting().getViewForTesting();
                    } else {
                        scrimView =
                                cta.getRootUiCoordinatorForTesting()
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
            ChromeTabbedActivity cta, boolean isDialogFullyVisible) {
        View controlContainer = cta.findViewById(R.id.control_container);
        assertEquals(
                isDialogFullyVisible,
                IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
                        == controlContainer.getImportantForAccessibility());
        View compositorViewHolder = cta.getCompositorViewHolderForTesting();
        assertEquals(
                isDialogFullyVisible,
                IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
                        == compositorViewHolder.getImportantForAccessibility());
        View bottomContainer = cta.findViewById(R.id.bottom_container);
        assertEquals(
                isDialogFullyVisible,
                IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
                        == bottomContainer.getImportantForAccessibility());
        if (isPhone()) {
            View bottomControls = cta.findViewById(R.id.bottom_controls);
            assertEquals(
                    isDialogFullyVisible,
                    IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
                            == bottomControls.getImportantForAccessibility());
        }
        if (isTablet(cta)) {
            View tabSwitcherViewHolder = cta.findViewById(R.id.tab_switcher_view_holder);
            assertEquals(
                    isDialogFullyVisible,
                    IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
                            == tabSwitcherViewHolder.getImportantForAccessibility());
        }
    }

    private void verifyDialogUndoBarAndClick() {
        // Verify that the dialog undo bar is showing and the default undo bar is hidden.
        onViewWaiting(
                allOf(
                        withId(R.id.snackbar_button),
                        isDescendantOfA(withId(R.id.dialog_snack_bar_container_view)),
                        isDisplayed()));
        onView(allOf(withId(R.id.snackbar), isDescendantOfA(withId(R.id.bottom_container))))
                .check(doesNotExist());
        onView(
                        allOf(
                                withId(R.id.snackbar_button),
                                isDescendantOfA(withId(R.id.dialog_snack_bar_container_view)),
                                isDisplayed()))
                .perform(click());
    }

    private void verifyGlobalUndoBarAndClick() {
        // Verify that the dialog undo bar is showing and the default undo bar is hidden.
        Matcher<View> expectedAncestor = instanceOf(HubContainerView.class);
        onViewWaiting(
                allOf(withId(R.id.snackbar), isDescendantOfA(expectedAncestor), isDisplayed()));
        onView(
                        allOf(
                                withId(R.id.snackbar_button),
                                isDescendantOfA(withId(R.id.dialog_snack_bar_container_view))))
                .check(doesNotExist());
        onView(
                        allOf(
                                withId(R.id.snackbar_button),
                                isDescendantOfA(expectedAncestor),
                                isDisplayed()))
                .perform(click());
    }

    private void verifyDialogBackButtonContentDescription(ChromeTabbedActivity cta, String s) {
        assertTrue(isDialogFullyVisible(cta));
        onViewWaiting(
                        allOf(
                                withId(R.id.toolbar_back_button),
                                isDescendantOfA(withId(R.id.tab_grid_dialog_toolbar_container))))
                .check((v, e) -> assertEquals(s, v.getContentDescription()));
    }

    private void testTitleTextFocus(ChromeTabbedActivity cta) throws ExecutionException {
        // Click the text field to grab focus and click back button to lose focus.
        onView(allOf(isDescendantOfA(withId(R.id.main_content)), withId(R.id.title)))
                .perform(click());
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
        onView(allOf(isDescendantOfA(withId(R.id.main_content)), withId(R.id.title)))
                .perform(click());
        verifyTitleTextFocus(cta, true);
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimation(cta);
        verifyTitleTextFocus(cta, false);
    }

    private void verifyTitleTextFocus(ChromeTabbedActivity cta, boolean shouldFocus) {
        CriteriaHelper.pollUiThread(
                () -> {
                    View titleTextView =
                            cta.findViewById(R.id.tab_group_toolbar).findViewById(R.id.title);
                    KeyboardVisibilityDelegate delegate =
                            cta.getWindowAndroid().getKeyboardDelegate();
                    boolean keyboardVisible =
                            delegate.isKeyboardShowing(
                                    cta, cta.getCompositorViewHolderForTesting());
                    boolean isFocused = titleTextView.isFocused();
                    return (!shouldFocus ^ isFocused) && (!shouldFocus ^ keyboardVisible);
                });
    }

    private RecyclerView getRecyclerView(ChromeTabbedActivity cta) {
        ViewGroup group = (ViewGroup) cta.findViewById(getTabSwitcherAncestorId(cta));
        return group.findViewById(R.id.tab_list_recycler_view);
    }

    private void enterTabListEditor(ChromeTabbedActivity cta) {
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(), cta, R.id.menu_select_tabs);
    }

    private void clickThroughConfirmationDialog() {
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(mModalDialogManager.isShowing(), Matchers.is(true)));
        onViewWaiting(withText("Delete group"), /* checkRootDialog= */ true).perform(click());
    }
}
