// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import static android.os.Build.VERSION_CODES.N_MR1;

import static org.hamcrest.Matchers.is;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_LOW_END_DEVICE;
import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.chrome.browser.tab.TabCreationState.LIVE_IN_BACKGROUND;
import static org.chromium.ui.test.util.ViewUtils.createMotionEvent;

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.view.ContextThemeWrapper;
import android.view.MotionEvent;
import android.view.MotionEvent.PointerCoords;
import android.view.MotionEvent.PointerProperties;
import android.widget.FrameLayout;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.stubbing.Answer;

import org.chromium.base.Log;
import org.chromium.base.MathUtils;
import org.chromium.base.jank_tracker.DummyJankTracker;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.accessibility_tab_switcher.OverviewListLayout;
import org.chromium.chrome.browser.compositor.layouts.Layout.LayoutState;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.features.start_surface.StartSurface;
import org.chromium.chrome.features.start_surface.StartSurface.TabSwitcherViewObserver;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel.MockTabModelDelegate;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Unit tests for {@link org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class LayoutManagerTest implements MockTabModelDelegate {
    private static final String TAG = "LayoutManagerTest";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Mock
    private TopUiThemeColorProvider mTopUiThemeColorProvider;

    @Mock
    private StartSurface mStartSurface;

    @Mock
    private TabSwitcher mTabSwitcher;

    @Mock
    private TabSwitcher.TabListDelegate mTabListDelegate;
    @Mock
    private TabSwitcher.Controller mTabSwitcherController;

    @Captor
    private ArgumentCaptor<TabSwitcherViewObserver> mTabSwitcherViewObserverArgumentCaptor;
    @Captor
    private ArgumentCaptor<TabSwitcher.TabSwitcherViewObserver>
            mRefactorTabSwitcherViewObserverArgumentCaptor;

    private long mLastDownTime;

    private TabModelSelector mTabModelSelector;
    private Supplier<StartSurface> mStartSurfaceSupplier;
    private Supplier<TabSwitcher> mTabSwitcherSupplier;
    private boolean mIsStartSurfaceRefactorEnabled;
    private LayoutManagerChrome mManager;
    private LayoutManagerChromePhone mManagerPhone;

    private final PointerProperties[] mProperties = new PointerProperties[2];
    private final PointerCoords[] mPointerCoords = new PointerCoords[2];

    private float mDpToPx;

    class LayoutObserverCallbackHelper extends CallbackHelper {
        @LayoutType
        public int layoutType;
    }

    private void initializeMotionEvent() {
        mProperties[0] = new PointerProperties();
        mProperties[0].id = 0;
        mProperties[0].toolType = MotionEvent.TOOL_TYPE_FINGER;
        mProperties[1] = new PointerProperties();
        mProperties[1].id = 1;
        mProperties[1].toolType = MotionEvent.TOOL_TYPE_FINGER;

        mPointerCoords[0] = new PointerCoords();
        mPointerCoords[0].x = 0;
        mPointerCoords[0].y = 0;
        mPointerCoords[0].pressure = 1;
        mPointerCoords[0].size = 1;
        mPointerCoords[1] = new PointerCoords();
        mPointerCoords[1].x = 0;
        mPointerCoords[1].y = 0;
        mPointerCoords[1].pressure = 1;
        mPointerCoords[1].size = 1;
    }

    private void setAccessibilityEnabledForTesting(Boolean value) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(value));
    }

    /**
     * Simulates time so the animation updates.
     * @param layoutManager The {@link LayoutManagerChrome} to update.
     * @param maxFrameCount The maximum number of frames to simulate before the motion ends.
     * @return              Whether the maximum number of frames was enough for the
     *                      {@link LayoutManagerChrome} to reach the end of the animations.
     */
    private static boolean simulateTime(LayoutManagerChrome layoutManager, int maxFrameCount) {
        // Simulating time
        int frame = 0;
        long time = 0;
        final long dt = 16;
        while (layoutManager.onUpdate(time, dt) && frame < maxFrameCount) {
            time += dt;
            frame++;
        }
        Log.w(TAG, "simulateTime frame " + frame);
        return frame < maxFrameCount;
    }

    private void initializeLayoutManagerPhone(int standardTabCount, int incognitoTabCount) {
        initializeLayoutManagerPhone(standardTabCount, incognitoTabCount,
                TabModel.INVALID_TAB_INDEX, TabModel.INVALID_TAB_INDEX, false);
    }

    private void initializeLayoutManagerPhone(int standardTabCount, int incognitoTabCount,
            int standardIndexSelected, int incognitoIndexSelected, boolean incognitoSelected) {
        Context context = new ContextThemeWrapper(
                InstrumentationRegistry.getContext(), R.style.Theme_BrowserUI_DayNight);

        mDpToPx = context.getResources().getDisplayMetrics().density;

        if (mIsStartSurfaceRefactorEnabled) {
            when(mTabSwitcher.getController()).thenReturn(mTabSwitcherController);
            when(mTabSwitcher.getTabListDelegate()).thenReturn(mTabListDelegate);
            when(mTabSwitcher.getTabGridDialogVisibilitySupplier()).thenReturn(() -> false);
        } else {
            when(mStartSurface.getGridTabListDelegate()).thenReturn(mTabListDelegate);
        }
        when(mStartSurface.getTabGridDialogVisibilitySupplier()).thenReturn(() -> false);

        mTabModelSelector = new MockTabModelSelector(standardTabCount, incognitoTabCount, this);
        if (standardIndexSelected != TabModel.INVALID_TAB_INDEX) {
            TabModelUtils.setIndex(mTabModelSelector.getModel(false), standardIndexSelected, false);
        }
        if (incognitoIndexSelected != TabModel.INVALID_TAB_INDEX) {
            TabModelUtils.setIndex(mTabModelSelector.getModel(true), incognitoIndexSelected, false);
        }
        mTabModelSelector.selectModel(incognitoSelected);
        Assert.assertNotNull(
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter());

        LayoutManagerHost layoutManagerHost = new MockLayoutHost(context);
        TabContentManager tabContentManager = new TabContentManager(context, null, false, null);
        tabContentManager.initWithNative();

        // Build a fake content container
        FrameLayout parentContainer = new FrameLayout(context);
        FrameLayout container = new FrameLayout(context);
        parentContainer.addView(container);

        ObservableSupplierImpl<TabContentManager> tabContentManagerSupplier =
                new ObservableSupplierImpl<>();

        mManagerPhone = new LayoutManagerChromePhone(layoutManagerHost, container,
                mStartSurfaceSupplier,
                mIsStartSurfaceRefactorEnabled ? mTabSwitcherSupplier : new OneshotSupplierImpl<>(),
                tabContentManagerSupplier, () -> mTopUiThemeColorProvider, new DummyJankTracker());

        setUpLayouts();

        tabContentManagerSupplier.set(tabContentManager);
        mManager = mManagerPhone;
        CompositorAnimationHandler.setTestingMode(true);
        mManager.init(mTabModelSelector, null, null, null, mTopUiThemeColorProvider);
        initializeMotionEvent();
    }

    private void setUpLayouts() {
        if (mIsStartSurfaceRefactorEnabled) {
            verify(mTabSwitcherController)
                    .addTabSwitcherViewObserver(
                            mRefactorTabSwitcherViewObserverArgumentCaptor.capture());
            doAnswer((Answer<Void>) invocation -> {
                mRefactorTabSwitcherViewObserverArgumentCaptor.getValue().finishedShowing();
                simulateTime(mManager, 1000);
                return null;
            })
                    .when(mTabSwitcherController)
                    .showTabSwitcherView(anyBoolean());

            doAnswer((Answer<Void>) invocation -> {
                mRefactorTabSwitcherViewObserverArgumentCaptor.getValue().finishedHiding();
                return null;
            })
                    .when(mTabSwitcherController)
                    .hideTabSwitcherView(anyBoolean());
        } else {
            verify(mStartSurface)
                    .addTabSwitcherViewObserver(mTabSwitcherViewObserverArgumentCaptor.capture());
            doAnswer((Answer<Void>) invocation -> {
                mTabSwitcherViewObserverArgumentCaptor.getValue().finishedShowing();
                simulateTime(mManager, 1000);
                return null;
            })
                    .when(mStartSurface)
                    .showOverview(anyBoolean());

            doAnswer((Answer<Void>) invocation -> {
                mTabSwitcherViewObserverArgumentCaptor.getValue().finishedHiding();
                return null;
            })
                    .when(mStartSurface)
                    .hideTabSwitcherView(anyBoolean());
        }
    }

    @Test
    @SmallTest
    @Feature({"Android-TabSwitcher"})
    @UiThreadTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    public void testCreation() {
        initializeLayoutManagerPhone(0, 0);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    @UiThreadTest
    public void testToolbarSideSwipeOnlyTab() {
        initializeLayoutManagerPhone(1, 0, 0, TabModel.INVALID_TAB_INDEX, false);
        Assert.assertEquals(mTabModelSelector.getModel(false).index(), 0);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 0);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    @UiThreadTest
    public void testToolbarSideSwipeOnlyTabIncognito() {
        initializeLayoutManagerPhone(0, 1, TabModel.INVALID_TAB_INDEX, 0, true);
        Assert.assertEquals(mTabModelSelector.getModel(true).index(), 0);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 0);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    @UiThreadTest
    public void testToolbarSideSwipeNextTab() {
        initializeLayoutManagerPhone(2, 0, 0, TabModel.INVALID_TAB_INDEX, false);
        Assert.assertEquals(mTabModelSelector.getModel(false).index(), 0);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 1);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    @UiThreadTest
    public void testToolbarSideSwipePrevTab() {
        initializeLayoutManagerPhone(2, 0, 1, TabModel.INVALID_TAB_INDEX, false);
        Assert.assertEquals(mTabModelSelector.getModel(false).index(), 1);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    @UiThreadTest
    public void testToolbarSideSwipeNextTabNone() {
        initializeLayoutManagerPhone(2, 0, 1, TabModel.INVALID_TAB_INDEX, false);
        Assert.assertEquals(mTabModelSelector.getModel(false).index(), 1);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 1);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    @UiThreadTest
    public void testToolbarSideSwipePrevTabNone() {
        initializeLayoutManagerPhone(2, 0, 0, TabModel.INVALID_TAB_INDEX, false);
        Assert.assertEquals(mTabModelSelector.getModel(false).index(), 0);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    @UiThreadTest
    public void testToolbarSideSwipeNextTabIncognito() {
        initializeLayoutManagerPhone(0, 2, TabModel.INVALID_TAB_INDEX, 0, true);
        Assert.assertEquals(mTabModelSelector.getModel(true).index(), 0);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 1);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    @UiThreadTest
    public void testToolbarSideSwipePrevTabIncognito() {
        initializeLayoutManagerPhone(0, 2, TabModel.INVALID_TAB_INDEX, 1, true);
        Assert.assertEquals(mTabModelSelector.getModel(true).index(), 1);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    @UiThreadTest
    public void testToolbarSideSwipeNextTabNoneIncognito() {
        initializeLayoutManagerPhone(0, 2, TabModel.INVALID_TAB_INDEX, 1, true);
        Assert.assertEquals(mTabModelSelector.getModel(true).index(), 1);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.LEFT, 1);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Android-TabSwitcher"})
    @UiThreadTest
    public void testToolbarSideSwipePrevTabNoneIncognito() {
        initializeLayoutManagerPhone(0, 2, TabModel.INVALID_TAB_INDEX, 0, true);
        Assert.assertEquals(mTabModelSelector.getModel(true).index(), 0);
        runToolbarSideSwipeTestOnCurrentModel(ScrollDirection.RIGHT, 0);
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_LOW_END_DEVICE})
    @Feature({"Android-TabSwitcher"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testStartSurfaceLayout_Disabled_LowEndPhone() throws Exception {
        // clang-format on
        ChromeFeatureList.sTabGridLayoutAndroid.setForTesting(true);
        launchAndVerifyOverviewListLayout();

        TabUiTestHelper.finishActivity(mActivityTestRule.getActivity());
        ChromeFeatureList.sTabGroupsAndroid.setForTesting(false);
        launchAndVerifyOverviewListLayout();

        // Test accessibility
        TabUiTestHelper.finishActivity(mActivityTestRule.getActivity());
        setAccessibilityEnabledForTesting(true);
        launchAndVerifyOverviewListLayout();
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @Feature({"Android-TabSwitcher"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
    @Features.DisableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID})
    public void testStartSurfaceLayout_Disabled_AllPhone_Accessibility_WithoutContinuationFlag() {
        // clang-format on
        final List<LayoutStateLayoutType> observationSequence = new ArrayList<>();
        setAccessibilityEnabledForTesting(true);
        launchChromeSimple();
        observeLayoutManager(observationSequence);
        showTabSwitcherLayout();

        verifyOverviewListLayoutShown();
        Assert.assertEquals(4, observationSequence.size());
        Assert.assertEquals(LayoutState.STARTING_TO_HIDE, observationSequence.get(0).layoutState);
        Assert.assertEquals(LayoutState.HIDDEN, observationSequence.get(1).layoutState);
        Assert.assertEquals(LayoutState.STARTING_TO_SHOW, observationSequence.get(2).layoutState);
        Assert.assertEquals(LayoutState.SHOWING, observationSequence.get(3).layoutState);
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"Android-TabSwitcher"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    public void testTabSwitcherLayout_Enabled_HighEndPhone() throws Exception {
        // clang-format on
        ChromeFeatureList.sTabGridLayoutAndroid.setForTesting(true);
        verifyTabSwitcherLayoutEnable(TabListCoordinator.TabListMode.GRID);

        TabUiTestHelper.finishActivity(mActivityTestRule.getActivity());
        ChromeFeatureList.sTabGridLayoutAndroid.setForTesting(false);
        verifyTabSwitcherLayoutEnable(TabListCoordinator.TabListMode.GRID);

        // Verify accessibility
        TabUiTestHelper.finishActivity(mActivityTestRule.getActivity());
        setAccessibilityEnabledForTesting(true);
        verifyTabSwitcherLayoutEnable(TabListCoordinator.TabListMode.GRID);
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_LOW_END_DEVICE})
    @Feature({"Android-TabSwitcher"})
    // clang-format off
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_ANDROID,
            ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID})
    @Features.DisableFeatures(ChromeFeatureList.TAB_TO_GTS_ANIMATION)
    public void testTabSwitcherLayout_Enabled_LowEndPhone() throws Exception {
        // clang-format on
        verifyTabSwitcherLayoutEnable(TabListCoordinator.TabListMode.LIST);

        // Test Accessibility
        TabUiTestHelper.finishActivity(mActivityTestRule.getActivity());
        setAccessibilityEnabledForTesting(true);
        verifyTabSwitcherLayoutEnable(TabListCoordinator.TabListMode.LIST);
    }

    // TODO(crbug.com/1108496): Update the test to use Assert.assertThat for better failure message.
    @Test
    @MediumTest
    public void testLayoutObserverNotification_ShowAndHide_ToolbarSwipe() throws TimeoutException {
        LayoutObserverCallbackHelper startedShowingCallback = new LayoutObserverCallbackHelper();
        LayoutObserverCallbackHelper finishedShowingCallback = new LayoutObserverCallbackHelper();
        LayoutObserverCallbackHelper startedHidingCallback = new LayoutObserverCallbackHelper();
        LayoutObserverCallbackHelper finishedHidingCallback = new LayoutObserverCallbackHelper();

        setUpShowAndHideLayoutObserverNotification(startedShowingCallback, finishedShowingCallback,
                startedHidingCallback, finishedHidingCallback);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            performToolbarSideSwipe(ScrollDirection.RIGHT);
            Assert.assertEquals(
                    LayoutType.TOOLBAR_SWIPE, mManager.getActiveLayout().getLayoutType());
            Assert.assertTrue(mManager.isLayoutVisible(LayoutType.TOOLBAR_SWIPE));
        });

        startedShowingCallback.waitForCallback(0);
        Assert.assertEquals(LayoutType.TOOLBAR_SWIPE, startedShowingCallback.layoutType);

        finishedShowingCallback.waitForCallback(0);
        Assert.assertEquals(LayoutType.TOOLBAR_SWIPE, finishedShowingCallback.layoutType);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            finishToolbarSideSwipe();
            Assert.assertEquals(LayoutType.BROWSING, mManager.getActiveLayout().getLayoutType());
            Assert.assertTrue(mManager.isLayoutVisible(LayoutType.BROWSING));
        });

        startedHidingCallback.waitForCallback(0);
        Assert.assertEquals(LayoutType.TOOLBAR_SWIPE, startedHidingCallback.layoutType);

        finishedHidingCallback.waitForCallback(0);
        Assert.assertEquals(LayoutType.TOOLBAR_SWIPE, finishedHidingCallback.layoutType);

        startedShowingCallback.waitForCallback(1);
        Assert.assertEquals(LayoutType.BROWSING, startedShowingCallback.layoutType);

        finishedShowingCallback.waitForCallback(1);
        Assert.assertEquals(LayoutType.BROWSING, finishedShowingCallback.layoutType);
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_greater_than = N_MR1, message = "crbug.com/1139943")
    @DisabledTest(message = "crbug.com/1216438") // Failures on N.
    @Features.EnableFeatures({ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID})
    public void testLayoutObserverNotification_ShowAndHide_TabSwitcher() throws TimeoutException {
        LayoutObserverCallbackHelper startedShowingCallback = new LayoutObserverCallbackHelper();
        LayoutObserverCallbackHelper finishedShowingCallback = new LayoutObserverCallbackHelper();
        LayoutObserverCallbackHelper startedHidingCallback = new LayoutObserverCallbackHelper();
        LayoutObserverCallbackHelper finishedHidingCallback = new LayoutObserverCallbackHelper();

        setUpShowAndHideLayoutObserverNotification(startedShowingCallback, finishedShowingCallback,
                startedHidingCallback, finishedHidingCallback);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mManager.showLayout(LayoutType.TAB_SWITCHER, true);

            Assert.assertTrue(
                    "layoutManager is way too long to end motion", simulateTime(mManager, 1000));
            Assert.assertEquals(
                    LayoutType.TAB_SWITCHER, mManager.getActiveLayout().getLayoutType());
            Assert.assertTrue(mManager.isLayoutVisible(LayoutType.TAB_SWITCHER));
        });

        // The |startedShowingCallback| callCount 0 is reserved for the default layout during
        // initialization. Because LayoutManager does not explicitly hide the old layout when a new
        // layout is forced to show, the callCount for |finishedShowingCallback|,
        // |startedHidingCallback|, and |finishedHidingCallback| are still 0.
        // TODO(crbug.com/1108496): update the callCount when LayoutManager explicitly hide the old
        // layout.
        startedShowingCallback.waitForCallback(1);
        Assert.assertEquals(LayoutType.TAB_SWITCHER, startedShowingCallback.layoutType);

        finishedShowingCallback.waitForCallback(0);
        Assert.assertEquals(LayoutType.TAB_SWITCHER, finishedShowingCallback.layoutType);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mManager.showLayout(LayoutType.BROWSING, true);
            Assert.assertTrue(
                    "layoutManager is way too long to end motion", simulateTime(mManager, 1000));

            Assert.assertTrue(mManager.isLayoutVisible(LayoutType.BROWSING));
        });

        startedHidingCallback.waitForCallback(0);
        Assert.assertEquals(LayoutType.TAB_SWITCHER, startedHidingCallback.layoutType);

        finishedHidingCallback.waitForCallback(0);
        Assert.assertEquals(LayoutType.TAB_SWITCHER, finishedHidingCallback.layoutType);

        startedShowingCallback.waitForCallback(2);
        Assert.assertEquals(LayoutType.BROWSING, startedShowingCallback.layoutType);

        finishedShowingCallback.waitForCallback(1);
        Assert.assertEquals(LayoutType.BROWSING, finishedShowingCallback.layoutType);
    }

    @Test
    @MediumTest
    public void testLayoutObserverNotification_ShowAndHide_SimpleAnimation()
            throws TimeoutException {
        LayoutObserverCallbackHelper startedShowingCallback = new LayoutObserverCallbackHelper();
        LayoutObserverCallbackHelper finishedShowingCallback = new LayoutObserverCallbackHelper();
        LayoutObserverCallbackHelper startedHidingCallback = new LayoutObserverCallbackHelper();
        LayoutObserverCallbackHelper finishedHidingCallback = new LayoutObserverCallbackHelper();

        setUpShowAndHideLayoutObserverNotification(startedShowingCallback, finishedShowingCallback,
                startedHidingCallback, finishedHidingCallback);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = createTab(123, false);
            mTabModelSelector.getModel(false).addTab(
                    tab, -1, TabLaunchType.FROM_LONGPRESS_BACKGROUND, LIVE_IN_BACKGROUND);
            Assert.assertTrue("LayoutManager took too long to finish the animations",
                    simulateTime(mManager, 1000));
            Assert.assertThat("Incorrect active LayoutType",
                    mManager.getActiveLayout().getLayoutType(), is(LayoutType.SIMPLE_ANIMATION));
            Assert.assertThat("Incorrect active Layout",
                    mManager.isLayoutVisible(LayoutType.SIMPLE_ANIMATION), is(true));
        });

        startedShowingCallback.waitForCallback(0);
        Assert.assertThat("startedShowingCallback with incorrect LayoutType",
                startedShowingCallback.layoutType, is(LayoutType.SIMPLE_ANIMATION));

        finishedShowingCallback.waitForCallback(0);
        Assert.assertThat("finishedShowingCallback with incorrect LayoutType",
                finishedShowingCallback.layoutType, is(LayoutType.SIMPLE_ANIMATION));

        CriteriaHelper.pollUiThread(() -> {
            return mManagerPhone.getActiveLayout().getLayoutType() == LayoutType.SIMPLE_ANIMATION
                    && mManagerPhone.getActiveLayout().isStartingToHide();
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Simulate hiding animation.
            Assert.assertTrue("LayoutManager took too long to finish the animations",
                    simulateTime(mManager, 1000));
        });

        startedHidingCallback.waitForCallback(0);
        Assert.assertThat("startedHidingCallback with incorrect LayoutType",
                startedHidingCallback.layoutType, is(LayoutType.SIMPLE_ANIMATION));

        finishedHidingCallback.waitForCallback(0);
        Assert.assertThat("finishedHidingCallback with incorrectLayoutType",
                finishedHidingCallback.layoutType, is(LayoutType.SIMPLE_ANIMATION));

        startedShowingCallback.waitForCallback(1);
        Assert.assertThat("startedShowingCallback with incorrectLayoutType",
                startedShowingCallback.layoutType, is(LayoutType.BROWSING));

        finishedShowingCallback.waitForCallback(1);
        Assert.assertThat("finishedShowingCallback with incorrectLayoutType",
                finishedShowingCallback.layoutType, is(LayoutType.BROWSING));
    }

    private void setUpShowAndHideLayoutObserverNotification(
            LayoutObserverCallbackHelper startedShowingCallback,
            LayoutObserverCallbackHelper finishedShowingCallback,
            LayoutObserverCallbackHelper startedHidingCallback,
            LayoutObserverCallbackHelper finishedHidingCallback) throws TimeoutException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            initializeLayoutManagerPhone(2, 0);
            mManager.addObserver(new LayoutStateProvider.LayoutStateObserver() {
                @Override
                public void onStartedShowing(int layoutType, boolean showToolbar) {
                    Log.d(TAG, "Started to show: " + layoutType);
                    startedShowingCallback.layoutType = layoutType;
                    startedShowingCallback.notifyCalled();
                }

                @Override
                public void onFinishedShowing(int layoutType) {
                    Log.d(TAG, "Finished showing: " + layoutType);
                    finishedShowingCallback.layoutType = layoutType;
                    finishedShowingCallback.notifyCalled();
                }

                @Override
                public void onStartedHiding(
                        int layoutType, boolean showToolbar, boolean delayAnimation) {
                    Log.d(TAG, "Started to hide: " + layoutType);
                    startedHidingCallback.layoutType = layoutType;
                    startedHidingCallback.notifyCalled();
                }

                @Override
                public void onFinishedHiding(int layoutType) {
                    Log.d(TAG, "Finished hiding: " + layoutType);
                    finishedHidingCallback.layoutType = layoutType;
                    finishedHidingCallback.notifyCalled();
                }
            });

            Assert.assertEquals(LayoutType.BROWSING, mManager.getActiveLayout().getLayoutType());
        });

        if (mManager.isLayoutVisible(LayoutType.BROWSING)) {
            startedShowingCallback.layoutType = LayoutType.BROWSING;
        } else {
            startedShowingCallback.waitForCallback(0);
        }
        Assert.assertEquals(LayoutType.BROWSING, startedShowingCallback.layoutType);
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_greater_than = N_MR1, message = "crbug.com/1139943")
    public void testLayoutObserverNotification_TabSelectionHinted() throws TimeoutException {
        CallbackHelper tabSelectionHintedCallback = new CallbackHelper();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            initializeLayoutManagerPhone(2, 0);
            mManager.addObserver(new LayoutStateProvider.LayoutStateObserver() {
                @Override
                public void onTabSelectionHinted(int tabId) {
                    Log.d(TAG, "onTabSelectionHinted");
                    tabSelectionHintedCallback.notifyCalled();
                }
            });

            mManager.showLayout(LayoutType.TAB_SWITCHER, true);
            Assert.assertTrue(
                    "layoutManager is way too long to end motion", simulateTime(mManager, 1000));
            Assert.assertEquals(
                    LayoutType.TAB_SWITCHER, mManager.getActiveLayout().getLayoutType());

            mManager.showLayout(LayoutType.BROWSING, true);
            Assert.assertTrue(
                    "layoutManager is way too long to end motion", simulateTime(mManager, 1000));

            Assert.assertEquals(LayoutType.BROWSING, mManager.getActiveLayout().getLayoutType());
        });

        tabSelectionHintedCallback.waitForCallback(0);
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        // Load the browser process.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { ChromeBrowserInitializer.getInstance().handleSynchronousStartup(); });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mIsStartSurfaceRefactorEnabled = ReturnToChromeUtil.isStartSurfaceRefactorEnabled(
                    InstrumentationRegistry.getContext());
        });

        mStartSurfaceSupplier = () -> mStartSurface;
        mTabSwitcherSupplier = () -> mTabSwitcher;
    }

    @After
    public void tearDown() {
        ChromeFeatureList.sTabGridLayoutAndroid.setForTesting(null);
        ChromeFeatureList.sTabGroupsAndroid.setForTesting(null);
        setAccessibilityEnabledForTesting(null);
    }

    private void launchAndVerifyOverviewListLayout() {
        launchedChromeAndEnterTabSwitcher();
        verifyOverviewListLayoutShown();
    }

    /**
     * Verify the {@link OverviewListLayout} is in used. The {@link OverviewListLayout} is used when
     * accessibility is turned on. It is also used for low end device.
     */
    private void verifyOverviewListLayoutShown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Layout activeLayout = getActiveLayout();
            Assert.assertTrue(activeLayout instanceof OverviewListLayout);
        });
    }

    private void verifyTabSwitcherLayoutEnable(
            @TabListCoordinator.TabListMode int expectedTabListMode) {
        launchedChromeAndEnterTabSwitcher();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Layout activeLayout = getActiveLayout();
            Assert.assertEquals(LayoutType.TAB_SWITCHER, activeLayout.getLayoutType());
            Assert.assertEquals(expectedTabListMode,
                    mActivityTestRule.getActivity()
                            .getStartSurface()
                            .getGridTabListDelegate()
                            .getListModeForTesting());
        });
    }

    private void launchedChromeAndEnterTabSwitcher() {
        launchChromeSimple();
        showTabSwitcherLayout();
    }

    private void launchChromeSimple() {
        mActivityTestRule.startMainActivityOnBlankPage();
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);
    }

    private void showTabSwitcherLayout() {
        LayoutTestUtils.startShowingAndWaitForLayout(
                getLayoutManagerChrome(), LayoutType.TAB_SWITCHER, false);
    }

    private Layout getActiveLayout() {
        return getLayoutManagerChrome().getActiveLayout();
    }

    private LayoutManagerChrome getLayoutManagerChrome() {
        return mActivityTestRule.getActivity().getLayoutManager();
    }

    private void runToolbarSideSwipeTestOnCurrentModel(
            @ScrollDirection int direction, int finalIndex) {
        final TabModel model = mTabModelSelector.getCurrentModel();
        final int finalId = model.getTabAt(finalIndex).getId();

        performToolbarSideSwipe(direction);
        finishToolbarSideSwipe();

        Assert.assertEquals("Unexpected model change after side swipe", model.isIncognito(),
                mTabModelSelector.isIncognitoSelected());

        Assert.assertEquals("Wrong index after side swipe", finalIndex, model.index());
        Assert.assertEquals(
                "Wrong current tab id", finalId, TabModelUtils.getCurrentTab(model).getId());
        Assert.assertTrue("LayoutManager#getActiveLayout() should be StaticLayout",
                mManager.getActiveLayout() instanceof StaticLayout);
    }

    private void performToolbarSideSwipe(@ScrollDirection int direction) {
        Assert.assertTrue("Unexpected direction for side swipe " + direction,
                direction == ScrollDirection.LEFT || direction == ScrollDirection.RIGHT);

        final Layout layout = mManager.getActiveLayout();
        final SwipeHandler eventHandler = mManager.getToolbarSwipeHandler();

        Assert.assertNotNull("LayoutManager#getToolbarSwipeHandler() returned null", eventHandler);
        Assert.assertNotNull("LayoutManager#getActiveLayout() returned null", layout);

        final float layoutWidth = layout.getWidth();
        final boolean scrollLeft = direction == ScrollDirection.LEFT;
        final float deltaX = MathUtils.flipSignIf(layoutWidth / 2.f, scrollLeft);

        eventHandler.onSwipeStarted(direction, createMotionEvent(layoutWidth * mDpToPx, 0));
        // Call swipeUpdated twice since the handler computes direction in that method.
        // TODO(mdjones): Update implementation of EdgeSwipeHandler to work this way by default.
        MotionEvent ev = createMotionEvent(deltaX * mDpToPx, 0.f);
        eventHandler.onSwipeUpdated(ev, deltaX * mDpToPx, 0.f, deltaX * mDpToPx, 0.f);
        eventHandler.onSwipeUpdated(ev, deltaX * mDpToPx, 0.f, deltaX * mDpToPx, 0.f);
        Assert.assertTrue("LayoutManager#getActiveLayout() should be ToolbarSwipeLayout",
                mManager.getActiveLayout() instanceof ToolbarSwipeLayout);
        Assert.assertTrue("LayoutManager took too long to finish the animations",
                simulateTime(mManager, 1000));
    }

    private void finishToolbarSideSwipe() {
        final SwipeHandler eventHandler = mManager.getToolbarSwipeHandler();
        Assert.assertNotNull("LayoutManager#getToolbarSwipeHandler() returned null", eventHandler);

        eventHandler.onSwipeFinished();
        Assert.assertTrue("LayoutManager took too long to finish the animations",
                simulateTime(mManager, 1000));
    }

    /** Simple tuple for LayoutStateProvider.LayoutStateObserver events. */
    private static class LayoutStateLayoutType {
        public final @LayoutState int layoutState;
        public final @LayoutType int layoutType;
        public LayoutStateLayoutType(@LayoutState int layoutState, @LayoutType int layoutType) {
            this.layoutState = layoutState;
            this.layoutType = layoutType;
        }
    }

    private void observeLayoutManager(List<LayoutStateLayoutType> observationSequence) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            LayoutManagerChrome layoutManagerChrome = getLayoutManagerChrome();
            Assert.assertNotNull("Must be called after initialization", layoutManagerChrome);
            layoutManagerChrome.addObserver(new LayoutStateProvider.LayoutStateObserver() {
                @Override
                public void onStartedShowing(int layoutType, boolean showToolbar) {
                    observationSequence.add(
                            new LayoutStateLayoutType(LayoutState.STARTING_TO_SHOW, layoutType));
                }

                @Override
                public void onFinishedShowing(int layoutType) {
                    observationSequence.add(
                            new LayoutStateLayoutType(LayoutState.SHOWING, layoutType));
                }

                @Override
                public void onStartedHiding(
                        int layoutType, boolean showToolbar, boolean delayAnimation) {
                    observationSequence.add(
                            new LayoutStateLayoutType(LayoutState.STARTING_TO_HIDE, layoutType));
                }

                @Override
                public void onFinishedHiding(int layoutType) {
                    observationSequence.add(
                            new LayoutStateLayoutType(LayoutState.HIDDEN, layoutType));
                }
            });
        });
    }

    @Override
    public Tab createTab(int id, boolean incognito) {
        return MockTab.createAndInitialize(id, incognito);
    }
}
