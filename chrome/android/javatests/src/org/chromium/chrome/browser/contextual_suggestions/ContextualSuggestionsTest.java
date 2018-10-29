// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_suggestions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;
import android.support.test.filters.MediumTest;
import android.support.v7.widget.RecyclerView;
import android.view.View;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.contextual_suggestions.ContextualSuggestionsModel.PropertyKey;
import org.chromium.chrome.browser.dependency_injection.ChromeAppModule;
import org.chromium.chrome.browser.dependency_injection.ModuleFactoryOverrides;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.modelutil.ListObservable;
import org.chromium.chrome.browser.modelutil.ListObservable.ListObserver;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestHelper;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder.PartialBindCallback;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticleViewHolder;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.test.ScreenShooter;
import org.chromium.chrome.browser.toolbar.ToolbarPhone;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.test.BottomSheetTestRule;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;
import org.chromium.chrome.test.util.browser.compositor.layouts.DisableChromeAnimations;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestWebContentsObserver;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.UiRestriction;

import java.util.Locale;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Tests related to displaying contextual suggestions in a bottom sheet.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@EnableFeatures(ChromeFeatureList.CONTEXTUAL_SUGGESTIONS_BUTTON)
public class ContextualSuggestionsTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    @Rule
    public ScreenShooter mScreenShooter = new ScreenShooter();
    @Rule
    public TestRule mDisableChromeAnimations = new DisableChromeAnimations();
    @Rule
    public RenderTestRule mRenderTestRule = new RenderTestRule();

    private static final String TEST_PAGE =
            "/chrome/test/data/android/contextual_suggestions/contextual_suggestions_test.html";

    private final FakeContextualSuggestionsSource mFakeSource =
            new FakeContextualSuggestionsSource();

    private EmbeddedTestServer mTestServer;
    private ContextualSuggestionsCoordinator mCoordinator;
    private ContextualSuggestionsMediator mMediator;
    private ContextualSuggestionsModel mModel;
    private BottomSheet mBottomSheet;
    private FakeTracker mFakeTracker;

    // Used in multi-instance test.
    private ContextualSuggestionsCoordinator mCoordinator2;
    private ContextualSuggestionsMediator mMediator2;
    private ContextualSuggestionsModel mModel2;
    private BottomSheet mBottomSheet2;

    private int mNumberOfSourcesCreated;

    private class ContextualSuggestionsModuleForTest extends ContextualSuggestionsModule {
        @Override
        public ContextualSuggestionsSource provideContextualSuggestionsSource(Profile profile) {
            mNumberOfSourcesCreated++;
            return mFakeSource;
        }
    }

    private class ChromeAppModuleForTest extends ChromeAppModule {
        @Override
        public EnabledStateMonitor provideEnabledStateMonitor() {
            return new EmptyEnabledStateMonitor() {
                @Override
                public boolean getSettingsEnabled() {
                    return true;
                }

                @Override
                public boolean getEnabledState() {
                    return true;
                }
            };
        }
    }

    @Before
    public void setUp() throws Exception {
        ModuleFactoryOverrides.setOverride(ContextualSuggestionsModuleForTest.Factory.class,
                ContextualSuggestionsModuleForTest::new);
        ModuleFactoryOverrides.setOverride(
                ChromeAppModule.Factory.class, ChromeAppModuleForTest::new);

        FetchHelper.setDisableDelayForTesting(true);
        ContextualSuggestionsMediator.setOverrideIPHTimeoutForTesting(true);

        mFakeTracker = new FakeTracker(FeatureConstants.CONTEXTUAL_SUGGESTIONS_FEATURE);
        TrackerFactory.setTrackerForTests(mFakeTracker);

        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(TEST_PAGE));
        final CallbackHelper waitForSuggestionsHelper = new CallbackHelper();

        ThreadUtils.runOnUiThreadBlocking(() -> {
            mCoordinator = mActivityTestRule.getActivity()
                                   .getComponent()
                                   .resolveContextualSuggestionsCoordinator();
            mMediator = mCoordinator.getMediatorForTesting();
            mModel = mCoordinator.getModelForTesting();

            if (mModel.getTitle() != null) {
                waitForSuggestionsHelper.notifyCalled();
            } else {
                mModel.addObserver((source, propertyKey) -> {
                    if (propertyKey == PropertyKey.TITLE && mModel.getTitle() != null) {
                        waitForSuggestionsHelper.notifyCalled();
                    }
                });
            }
        });

        waitForSuggestionsHelper.waitForCallback(0);
        mBottomSheet = mActivityTestRule.getActivity().getBottomSheet();
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
        FetchHelper.setDisableDelayForTesting(false);
        ContextualSuggestionsMediator.setOverrideIPHTimeoutForTesting(false);
        ModuleFactoryOverrides.clearOverrides();
    }

    @Test
    @MediumTest
    @Feature({"ContextualSuggestions"})
    public void testRepeatedOpen() throws Exception {
        View toolbarButton = getToolbarButton();
        assertEquals(
                "Toolbar button should be visible", View.VISIBLE, toolbarButton.getVisibility());

        clickToolbarButton();
        simulateClickOnCloseButton();

        assertEquals(
                "Toolbar button should be visible", View.VISIBLE, toolbarButton.getVisibility());

        clickToolbarButton();
        testOpenFirstSuggestion();

        assertEquals("Toolbar button should be visible", View.GONE, toolbarButton.getVisibility());
    }

    @Test
    @MediumTest
    @Feature({"ContextualSuggestions"})
    public void testOpenSuggestion() throws Exception {
        clickToolbarButton();
        testOpenFirstSuggestion();
    }

    @Test
    @MediumTest
    @Feature({"ContextualSuggestions"})
    public void testOpenArticleInNewTab() throws Exception {
        clickToolbarButton();

        SnippetArticleViewHolder holder = getFirstSuggestionViewHolder();
        String expectedUrl = holder.getUrl();

        ChromeTabUtils.invokeContextMenuAndOpenInANewTab(mActivityTestRule, holder.itemView,
                ContextMenuManager.ContextMenuItemId.OPEN_IN_NEW_TAB, false, expectedUrl);

        assertEquals("Sheet should still be opened.", BottomSheet.SheetState.HALF,
                mBottomSheet.getSheetState());
    }

    @Test
    @MediumTest
    @Feature({"ContextualSuggestions"})
    public void testOpenSuggestionInNewTabIncognito() throws Exception {
        clickToolbarButton();

        SnippetArticleViewHolder holder = getFirstSuggestionViewHolder();
        String expectedUrl = holder.getUrl();

        ChromeTabUtils.invokeContextMenuAndOpenInANewTab(mActivityTestRule, holder.itemView,
                ContextMenuManager.ContextMenuItemId.OPEN_IN_INCOGNITO_TAB, true, expectedUrl);

        ThreadUtils.runOnUiThreadBlocking(() -> mBottomSheet.endAnimations());

        assertFalse("Sheet should be closed.", mBottomSheet.isSheetOpen());
    }

    @Test
    @MediumTest
    @Feature({"ContextualSuggestions"})
    public void testShadowVisibleOnScroll() throws Exception {
        clickToolbarButton();

        assertFalse("Shadow shouldn't be visible.", mModel.getToolbarShadowVisibility());

        CallbackHelper shadowVisibilityCallback = new CallbackHelper();

        mModel.addObserver((source, propertyKey) -> {
            if (propertyKey == PropertyKey.TOOLBAR_SHADOW_VISIBILITY
                    && mModel.getToolbarShadowVisibility()) {
                shadowVisibilityCallback.notifyCalled();
            }
        });

        ThreadUtils.runOnUiThreadBlocking(() -> {
            RecyclerView view =
                    (RecyclerView) mBottomSheet.getCurrentSheetContent().getContentView();
            view.smoothScrollToPosition(5);
        });

        shadowVisibilityCallback.waitForCallback("Shadow should be visible");
    }

    @Test
    @MediumTest
    @Feature({"ContextualSuggestions"})
    @DisabledTest(message = "https://crbug.com/890947")
    public void testInProductHelp() throws InterruptedException, TimeoutException {
        assertTrue(
                "Help bubble should be showing.", mMediator.getHelpBubbleForTesting().isShowing());

        ThreadUtils.runOnUiThreadBlocking(() -> mMediator.getHelpBubbleForTesting().dismiss());

        Assert.assertEquals("Help bubble should be dimissed.", 1,
                mFakeTracker.mDimissedCallbackHelper.getCallCount());
    }

    @Test
    @LargeTest
    @Feature({"ContextualSuggestions"})
    public void testMultiInstanceMode() throws Exception {
        ChromeTabbedActivity activity1 = mActivityTestRule.getActivity();
        clickToolbarButton();

        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        ChromeTabbedActivity activity2 = MultiWindowTestHelper.createSecondChromeTabbedActivity(
                activity1, new LoadUrlParams(mTestServer.getURL(TEST_PAGE)));
        ChromeActivityTestRule.waitForActivityNativeInitializationComplete(activity2);

        CallbackHelper allItemsInsertedCallback = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mCoordinator2 = activity2.getComponent().resolveContextualSuggestionsCoordinator();
            mMediator2 = mCoordinator2.getMediatorForTesting();
            mModel2 = mCoordinator2.getModelForTesting();
            mBottomSheet2 = activity2.getBottomSheet();

            mModel2.getClusterList().addObserver(new ListObserver<PartialBindCallback>() {
                @Override
                public void onItemRangeInserted(ListObservable source, int index, int count) {
                    // There will be two calls to this method, one for each cluster that is added
                    // to the list. Wait for the expected number of items to ensure the model
                    // is finished updating.
                    if (mModel2.getClusterList().getItemCount()
                            == FakeContextualSuggestionsSource.TOTAL_ITEM_COUNT) {
                        allItemsInsertedCallback.notifyCalled();
                    }
                }
            });
        });

        assertNotEquals("There should be two coordinators.", mCoordinator, mCoordinator2);
        assertNotEquals("There should be two mediators.", mMediator, mMediator2);
        assertNotEquals("There should be two models.", mModel, mModel2);
        assertEquals("There should have been two requests to create a ContextualSuggestionsSource",
                2, mNumberOfSourcesCreated);

        allItemsInsertedCallback.waitForCallback(0);

        int itemCount = ThreadUtils.runOnUiThreadBlocking(
                () -> { return mModel.getClusterList().getItemCount(); });
        assertEquals("Second model has incorrect number of items.",
                (int) FakeContextualSuggestionsSource.TOTAL_ITEM_COUNT, itemCount);

        clickToolbarButton(activity2);
        BottomSheetTestRule.waitForWindowUpdates();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            ContextualSuggestionsBottomSheetContent content1 =
                    (ContextualSuggestionsBottomSheetContent) mBottomSheet.getCurrentSheetContent();
            ContextualSuggestionsBottomSheetContent content2 =
                    (ContextualSuggestionsBottomSheetContent)
                            mBottomSheet2.getCurrentSheetContent();
            assertNotEquals("There should be two bottom sheet contents", content1, content2);
        });

        assertEquals("Sheet in the second activity should be peeked.", BottomSheet.SheetState.HALF,
                mBottomSheet2.getSheetState());
        assertEquals("Sheet in the first activity should be open.", BottomSheet.SheetState.HALF,
                mBottomSheet.getSheetState());

        ThreadUtils.runOnUiThreadBlocking(
                () -> mBottomSheet2.setSheetState(BottomSheet.SheetState.FULL, false));

        SnippetArticleViewHolder holder = getFirstSuggestionViewHolder(mBottomSheet2);
        String expectedUrl = holder.getUrl();
        ChromeTabUtils.invokeContextMenuAndOpenInOtherWindow(activity2, activity1, holder.itemView,
                ContextMenuManager.ContextMenuItemId.OPEN_IN_NEW_WINDOW, false, expectedUrl);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            mBottomSheet.endAnimations();
            mBottomSheet2.endAnimations();
        });

        assertTrue("Sheet in second activity should be opened.", mBottomSheet2.isSheetOpen());
        assertFalse("Sheet in first activity should be closed.", mBottomSheet.isSheetOpen());
    }

    @Test
    @MediumTest
    @Feature({"ContextualSuggestions", "UiCatalogue"})
    public void testCaptureContextualSuggestionsBottomSheet() throws Exception {
        dismissHelpBubble();

        mScreenShooter.shoot("Contextual suggestions: toolbar button");

        clickToolbarButton();
        BottomSheetTestRule.waitForWindowUpdates();

        ThreadUtils.runOnUiThreadBlocking(
                () -> mBottomSheet.setSheetState(BottomSheet.SheetState.HALF, false));
        BottomSheetTestRule.waitForWindowUpdates();
        mScreenShooter.shoot("Contextual suggestions: half height, images loading");

        ThreadUtils.runOnUiThreadBlocking(() -> mFakeSource.runImageFetchCallbacks());
        BottomSheetTestRule.waitForWindowUpdates();
        mScreenShooter.shoot("Contextual suggestions: half height, images loaded");

        ThreadUtils.runOnUiThreadBlocking(
                () -> mBottomSheet.setSheetState(BottomSheet.SheetState.FULL, false));
        BottomSheetTestRule.waitForWindowUpdates();
        mScreenShooter.shoot("Contextual suggestions: full height");

        ThreadUtils.runOnUiThreadBlocking(() -> {
            RecyclerView view =
                    (RecyclerView) mBottomSheet.getCurrentSheetContent().getContentView();
            view.scrollToPosition(5);
        });
        BottomSheetTestRule.waitForWindowUpdates();
        mScreenShooter.shoot("Contextual suggestions: scrolled");
    }

    @Test
    @MediumTest
    @Feature({"ContextualSuggestions", "RenderTest"})
    public void testRender() throws Exception {
        dismissHelpBubble();

        // Open the sheet to cause the suggestions to be bound in the RecyclerView, then capture
        // a suggestion with its thumbnail loading.
        clickToolbarButton();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mBottomSheet.setSheetState(BottomSheet.SheetState.FULL, false));

        BottomSheetTestRule.waitForWindowUpdates();
        mRenderTestRule.render(getFirstSuggestionViewHolder().itemView, "suggestion_image_loading");

        // Run the image fetch callback so images load, then capture a suggestion with its
        // thumbnail loaded.
        ThreadUtils.runOnUiThreadBlocking(() -> mFakeSource.runImageFetchCallbacks());
        BottomSheetTestRule.waitForWindowUpdates();
        mRenderTestRule.render(getFirstSuggestionViewHolder().itemView, "suggestion_image_loaded");

        // Render a thumbnail with an offline badge.
        ThreadUtils.runOnUiThreadBlocking(
                () -> getSuggestionViewHolder(2).setOfflineBadgeVisibilityForTesting(true));
        mRenderTestRule.render(getSuggestionViewHolder(2).itemView, "suggestion_offline");

        // Render the full suggestions sheet.
        mRenderTestRule.render(mBottomSheet, "full_height");

        // Scroll the suggestions and render the full suggestions sheet.
        ThreadUtils.runOnUiThreadBlocking(() -> {
            RecyclerView view =
                    (RecyclerView) mBottomSheet.getCurrentSheetContent().getContentView();
            view.scrollToPosition(5);
        });
        BottomSheetTestRule.waitForWindowUpdates();
        mRenderTestRule.render(mBottomSheet, "full_height_scrolled");
    }

    // Re-enable if peek delay condition is hooked up to toolbar button.
    @Test
    @MediumTest
    @Feature({"ContextualSuggestions"})
    @DisabledTest
    public void testPeekDelay() throws Exception {
        // Close the suggestions from setUp().
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mMediator.clearState();
            mBottomSheet.endAnimations();
        });

        // Request suggestions with fetch time baseline set for testing.
        long startTime = SystemClock.uptimeMillis();
        FetchHelper.setFetchTimeBaselineMillisForTesting(startTime);
        ThreadUtils.runOnUiThreadBlocking(
                () -> mMediator.requestSuggestions("http://www.testurl.com"));
        assertEquals("Bottom sheet should be hidden before delay.", BottomSheet.SheetState.HIDDEN,
                mBottomSheet.getSheetState());

        // Simulate user scroll by calling showContentInSheet until the sheet is peeked.
        CriteriaHelper.pollUiThread(() -> {
            mMediator.showContentInSheetForTesting(true);
            mBottomSheet.endAnimations();
            return mBottomSheet.getSheetState() == BottomSheet.SheetState.PEEK;
        });

        // Verify that suggestions is shown after the expected delay.
        long duration = SystemClock.uptimeMillis() - startTime;
        long expected = FakeContextualSuggestionsSource.TEST_PEEK_DELAY_SECONDS * 1000;
        assertTrue(String.format(Locale.US,
                        "The peek delay should be greater than %d ms, but was %d ms.",
                        expected, duration),
                duration >= expected);
    }

    @Test
    @MediumTest
    @Feature({"ContextualSuggestions"})
    public void testToolbarButton_ToggleTabSwitcher() throws Exception {
        View toolbarButton = getToolbarButton();

        assertEquals(
                "Toolbar button should be visible", View.VISIBLE, toolbarButton.getVisibility());

        ThreadUtils.runOnUiThreadBlocking(
                () -> { mActivityTestRule.getActivity().getLayoutManager().showOverview(false); });

        assertEquals("Toolbar button should be invisible", View.INVISIBLE,
                toolbarButton.getVisibility());

        ThreadUtils.runOnUiThreadBlocking(
                () -> { mActivityTestRule.getActivity().getLayoutManager().hideOverview(false); });

        assertEquals(
                "Toolbar button should be visible", View.VISIBLE, toolbarButton.getVisibility());
    }

    @Test
    @MediumTest
    @Feature({"ContextualSuggestions"})
    public void testToolbarButton_SwitchTabs() throws Exception {
        View toolbarButton = getToolbarButton();

        assertEquals(
                "Toolbar button should be visible", View.VISIBLE, toolbarButton.getVisibility());

        final TabModel currentModel =
                mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();
        int currentIndex = currentModel.index();
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        assertEquals("Toolbar button should be gone", View.GONE, toolbarButton.getVisibility());

        ThreadUtils.runOnUiThreadBlocking(
                () -> currentModel.setIndex(currentIndex, TabSelectionType.FROM_USER));

        CriteriaHelper.pollUiThread(
                () -> { return toolbarButton.getVisibility() == View.VISIBLE; });
    }

    @Test
    @MediumTest
    @Feature({"ContextualSuggestions"})
    public void testToolbarButton_ResponseInTabSwitcher() throws Exception {
        View toolbarButton = getToolbarButton();

        assertEquals(
                "Toolbar button should be visible", View.VISIBLE, toolbarButton.getVisibility());

        // Simulate suggestions being cleared.
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mMediator.clearState();
            getToolbarPhone().endExperimentalButtonAnimationForTesting();
        });
        assertEquals("Toolbar button should be gone", View.GONE, toolbarButton.getVisibility());
        assertEquals("Suggestions should be cleared", 0, mModel.getClusterList().getItemCount());

        // Enter tab switcher.
        ThreadUtils.runOnUiThreadBlocking(
                () -> { mActivityTestRule.getActivity().getLayoutManager().showOverview(false); });

        // Simulate a new suggestions request.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mMediator.requestSuggestions("https://www.google.com"));
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mModel.getClusterList().getItemCount()
                        == FakeContextualSuggestionsSource.TOTAL_ITEM_COUNT;
            }
        });

        assertEquals("Toolbar button should be invisible", View.INVISIBLE,
                toolbarButton.getVisibility());

        ThreadUtils.runOnUiThreadBlocking(
                () -> { mActivityTestRule.getActivity().getLayoutManager().hideOverview(false); });

        assertEquals(
                "Toolbar button should be visible", View.VISIBLE, toolbarButton.getVisibility());
    }

    private void simulateClickOnCloseButton() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mBottomSheet.getCurrentSheetContent()
                    .getToolbarView()
                    .findViewById(R.id.close_button)
                    .performClick();
            mBottomSheet.endAnimations();
        });

        assertEquals("Sheet should be hidden.", BottomSheet.SheetState.HIDDEN,
                mBottomSheet.getSheetState());
        assertNull("Bottom sheet contents should be null.", mBottomSheet.getCurrentSheetContent());
    }

    private SnippetArticleViewHolder getFirstSuggestionViewHolder() {
        return getFirstSuggestionViewHolder(mBottomSheet);
    }

    private SnippetArticleViewHolder getFirstSuggestionViewHolder(BottomSheet bottomSheet) {
        return getSuggestionViewHolder(bottomSheet, 0);
    }

    private SnippetArticleViewHolder getSuggestionViewHolder(int index) {
        return getSuggestionViewHolder(mBottomSheet, index);
    }

    private SnippetArticleViewHolder getSuggestionViewHolder(BottomSheet bottomSheet, int index) {
        ContextualSuggestionsBottomSheetContent content =
                (ContextualSuggestionsBottomSheetContent) bottomSheet.getCurrentSheetContent();
        RecyclerView recyclerView = (RecyclerView) content.getContentView();

        RecyclerViewTestUtils.waitForStableRecyclerView(recyclerView);

        return (SnippetArticleViewHolder) recyclerView.findViewHolderForAdapterPosition(index);
    }

    private View getToolbarButton() throws ExecutionException {
        return getToolbarButton(mActivityTestRule.getActivity());
    }

    private View getToolbarButton(ChromeActivity activity) throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> { return getToolbarPhone(activity).getExperimentalButtonForTesting(); });
    }

    private void clickToolbarButton() throws ExecutionException {
        clickToolbarButton(mActivityTestRule.getActivity());
    }

    private void clickToolbarButton(ChromeActivity activity) throws ExecutionException {
        View toolbarButton = getToolbarButton(activity);
        assertEquals(
                "Toolbar button should be visible", View.VISIBLE, toolbarButton.getVisibility());

        ThreadUtils.runOnUiThreadBlocking(() -> {
            toolbarButton.performClick();
            mBottomSheet.endAnimations();
        });
        assertTrue("Sheet should be open.", mBottomSheet.isSheetOpen());
    }

    private void testOpenFirstSuggestion() throws InterruptedException, TimeoutException {
        SnippetArticleViewHolder holder = getFirstSuggestionViewHolder();
        String expectedUrl = holder.getUrl();

        TestWebContentsObserver webContentsObserver = new TestWebContentsObserver(
                mActivityTestRule.getActivity().getActivityTab().getWebContents());

        int callCount = webContentsObserver.getOnPageStartedHelper().getCallCount();

        ThreadUtils.runOnUiThreadBlocking(() -> { holder.itemView.performClick(); });

        webContentsObserver.getOnPageStartedHelper().waitForCallback(callCount);

        ThreadUtils.runOnUiThreadBlocking(() -> mBottomSheet.endAnimations());

        assertFalse("Sheet should be closed.", mBottomSheet.isSheetOpen());

        // URL may not have been updated yet when WebContentsObserver#didStartLoading is called.
        CriteriaHelper.pollUiThread(() -> {
            return mActivityTestRule.getActivity().getActivityTab().getUrl().equals(expectedUrl);
        });

        ThreadUtils.runOnUiThreadBlocking(
                () -> getToolbarPhone().endExperimentalButtonAnimationForTesting());
    }

    private void dismissHelpBubble() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            if (mMediator.getHelpBubbleForTesting() != null) {
                mMediator.getHelpBubbleForTesting().dismiss();
            }
        });
    }

    private ToolbarPhone getToolbarPhone() {
        return getToolbarPhone(mActivityTestRule.getActivity());
    }

    private ToolbarPhone getToolbarPhone(ChromeActivity activity) {
        return (ToolbarPhone) activity.getToolbarManager().getToolbarLayout();
    }
}
