// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import static android.os.Build.VERSION_CODES.N_MR1;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.is;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.chrome.browser.tab.TabCreationState.LIVE_IN_BACKGROUND;
import static org.chromium.ui.test.util.ViewUtils.createMotionEvent;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.MotionEvent;
import android.view.MotionEvent.PointerCoords;
import android.view.MotionEvent.PointerProperties;
import android.widget.FrameLayout;

import androidx.test.annotation.UiThreadTest;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Log;
import org.chromium.base.MathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.compositor.layouts.Layout.LayoutState;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.hub.HubLayoutDependencyHolder;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel.MockTabModelDelegate;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.List;
import java.util.concurrent.TimeoutException;

/** Unit tests for {@link org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome} */
@RunWith(ChromeJUnit4ClassRunner.class)
public class LayoutManagerTest implements MockTabModelDelegate {
    private static final String TAG = "LayoutManagerTest";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Mock private TopUiThemeColorProvider mTopUiThemeColorProvider;
    @Mock private HubLayoutDependencyHolder mHubLayoutDependencyHolder;
    @Mock private TabWindowManager mTabWindowManager;

    private TabModelSelector mTabModelSelector;
    private OneshotSupplierImpl<TabSwitcher> mTabSwitcherSupplier;
    private Supplier<TabModelSelector> mTabModelSelectorSupplier;
    private LayoutManagerChrome mManager;
    private LayoutManagerChromePhone mManagerPhone;

    private final PointerProperties[] mProperties = new PointerProperties[2];
    private final PointerCoords[] mPointerCoords = new PointerCoords[2];

    private float mDpToPx;

    static class LayoutObserverCallbackHelper extends CallbackHelper {
        @LayoutType public int layoutType;
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(value));
    }

    /**
     * Simulates time so the animation updates.
     *
     * @param layoutManager The {@link LayoutManagerChrome} to update.
     * @param maxFrameCount The maximum number of frames to simulate before the motion ends.
     * @return Whether the maximum number of frames was enough for the {@link LayoutManagerChrome}
     *     to reach the end of the animations.
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
        initializeLayoutManagerPhone(
                standardTabCount,
                incognitoTabCount,
                TabModel.INVALID_TAB_INDEX,
                TabModel.INVALID_TAB_INDEX,
                false);
    }

    private void initializeLayoutManagerPhone(
            int standardTabCount,
            int incognitoTabCount,
            int standardIndexSelected,
            int incognitoIndexSelected,
            boolean incognitoSelected) {
        Context context =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        mDpToPx = context.getResources().getDisplayMetrics().density;

        mTabModelSelector =
                new MockTabModelSelector(
                        ProfileManager.getLastUsedRegularProfile(),
                        ProfileManager.getLastUsedRegularProfile().getPrimaryOTRProfile(true),
                        standardTabCount,
                        incognitoTabCount,
                        this);
        if (standardIndexSelected != TabModel.INVALID_TAB_INDEX) {
            TabModelUtils.setIndex(mTabModelSelector.getModel(false), standardIndexSelected);
        }
        if (incognitoIndexSelected != TabModel.INVALID_TAB_INDEX) {
            TabModelUtils.setIndex(mTabModelSelector.getModel(true), incognitoIndexSelected);
        }
        mTabModelSelector.selectModel(incognitoSelected);
        Assert.assertNotNull(
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter());

        LayoutManagerHost layoutManagerHost = new MockLayoutHost(context);
        TabContentManager tabContentManager =
                new TabContentManager(context, null, false, null, mTabWindowManager);
        tabContentManager.initWithNative();

        // Build a fake content container
        FrameLayout parentContainer = new FrameLayout(context);
        FrameLayout container = new FrameLayout(context);
        parentContainer.addView(container);

        ObservableSupplierImpl<TabContentManager> tabContentManagerSupplier =
                new ObservableSupplierImpl<>();

        mTabSwitcherSupplier = new OneshotSupplierImpl();
        mManagerPhone =
                new LayoutManagerChromePhone(
                        layoutManagerHost,
                        container,
                        mTabSwitcherSupplier,
                        mTabModelSelectorSupplier,
                        tabContentManagerSupplier,
                        () -> mTopUiThemeColorProvider,
                        mHubLayoutDependencyHolder);

        tabContentManagerSupplier.set(tabContentManager);
        mManager = mManagerPhone;
        CompositorAnimationHandler.setTestingMode(true);
        mManager.init(mTabModelSelector, null, null, null, mTopUiThemeColorProvider);
        initializeMotionEvent();
    }

    @Test
    @SmallTest
    @Feature({"Android-TabSwitcher"})
    @UiThreadTest
    @Restriction({DeviceFormFactor.PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
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
    @Restriction({DeviceFormFactor.PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"Android-TabSwitcher"})
    @CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
    public void testHubTabSwitcherLayout_Enabled() throws Exception {
        launchedChromeAndEnterTabSwitcher();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(LayoutType.TAB_SWITCHER, getActiveLayout().getLayoutType());
                });

        // See https://crbug.com/1522983 this shouldn't crash.
        showTabSwitcherLayout();
    }

    // TODO(crbug.com/40141330): Update the test to use assertThat for better failure message.
    @Test
    @MediumTest
    public void testLayoutObserverNotification_ShowAndHide_ToolbarSwipe() throws TimeoutException {
        LayoutObserverCallbackHelper startedShowingCallback = new LayoutObserverCallbackHelper();
        LayoutObserverCallbackHelper finishedShowingCallback = new LayoutObserverCallbackHelper();
        LayoutObserverCallbackHelper startedHidingCallback = new LayoutObserverCallbackHelper();
        LayoutObserverCallbackHelper finishedHidingCallback = new LayoutObserverCallbackHelper();

        setUpShowAndHideLayoutObserverNotification(
                startedShowingCallback,
                finishedShowingCallback,
                startedHidingCallback,
                finishedHidingCallback);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    performToolbarSideSwipe(ScrollDirection.RIGHT);
                    Assert.assertEquals(
                            LayoutType.TOOLBAR_SWIPE, mManager.getActiveLayout().getLayoutType());
                    Assert.assertTrue(mManager.isLayoutVisible(LayoutType.TOOLBAR_SWIPE));
                });

        startedShowingCallback.waitForCallback(0);
        Assert.assertEquals(LayoutType.TOOLBAR_SWIPE, startedShowingCallback.layoutType);

        finishedShowingCallback.waitForCallback(0);
        Assert.assertEquals(LayoutType.TOOLBAR_SWIPE, finishedShowingCallback.layoutType);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    finishToolbarSideSwipe();
                    Assert.assertEquals(
                            LayoutType.BROWSING, mManager.getActiveLayout().getLayoutType());
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
    public void testLayoutObserverNotification_ShowAndHide_TabSwitcher() throws TimeoutException {
        LayoutObserverCallbackHelper startedShowingCallback = new LayoutObserverCallbackHelper();
        LayoutObserverCallbackHelper finishedShowingCallback = new LayoutObserverCallbackHelper();
        LayoutObserverCallbackHelper startedHidingCallback = new LayoutObserverCallbackHelper();
        LayoutObserverCallbackHelper finishedHidingCallback = new LayoutObserverCallbackHelper();

        setUpShowAndHideLayoutObserverNotification(
                startedShowingCallback,
                finishedShowingCallback,
                startedHidingCallback,
                finishedHidingCallback);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mManager.showLayout(LayoutType.TAB_SWITCHER, true);

                    Assert.assertTrue(
                            "layoutManager is way too long to end motion",
                            simulateTime(mManager, 1000));
                    Assert.assertEquals(
                            LayoutType.TAB_SWITCHER, mManager.getActiveLayout().getLayoutType());
                    Assert.assertTrue(mManager.isLayoutVisible(LayoutType.TAB_SWITCHER));
                });

        // The |startedShowingCallback| callCount 0 is reserved for the default layout during
        // initialization. Because LayoutManager does not explicitly hide the old layout when a new
        // layout is forced to show, the callCount for |finishedShowingCallback|,
        // |startedHidingCallback|, and |finishedHidingCallback| are still 0.
        // TODO(crbug.com/40141330): update the callCount when LayoutManager explicitly hide the old
        // layout.
        startedShowingCallback.waitForCallback(1);
        Assert.assertEquals(LayoutType.TAB_SWITCHER, startedShowingCallback.layoutType);

        finishedShowingCallback.waitForCallback(0);
        Assert.assertEquals(LayoutType.TAB_SWITCHER, finishedShowingCallback.layoutType);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mManager.showLayout(LayoutType.BROWSING, true);
                    Assert.assertTrue(
                            "layoutManager is way too long to end motion",
                            simulateTime(mManager, 1000));

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

        setUpShowAndHideLayoutObserverNotification(
                startedShowingCallback,
                finishedShowingCallback,
                startedHidingCallback,
                finishedHidingCallback);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab tab = createTab(123, false);
                    mTabModelSelector
                            .getModel(false)
                            .addTab(
                                    tab,
                                    -1,
                                    TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                                    LIVE_IN_BACKGROUND);
                    Assert.assertTrue(
                            "LayoutManager took too long to finish the animations",
                            simulateTime(mManager, 1000));
                    assertThat(
                            "Incorrect active LayoutType",
                            mManager.getActiveLayout().getLayoutType(),
                            is(LayoutType.SIMPLE_ANIMATION));
                    assertThat(
                            "Incorrect active Layout",
                            mManager.isLayoutVisible(LayoutType.SIMPLE_ANIMATION),
                            is(true));
                });

        startedShowingCallback.waitForCallback(0);
        assertThat(
                "startedShowingCallback with incorrect LayoutType",
                startedShowingCallback.layoutType,
                is(LayoutType.SIMPLE_ANIMATION));

        finishedShowingCallback.waitForCallback(0);
        assertThat(
                "finishedShowingCallback with incorrect LayoutType",
                finishedShowingCallback.layoutType,
                is(LayoutType.SIMPLE_ANIMATION));

        CriteriaHelper.pollUiThread(
                () -> {
                    return mManagerPhone.getActiveLayout().getLayoutType()
                                    == LayoutType.SIMPLE_ANIMATION
                            && mManagerPhone.getActiveLayout().isStartingToHide();
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Simulate hiding animation.
                    Assert.assertTrue(
                            "LayoutManager took too long to finish the animations",
                            simulateTime(mManager, 1000));
                });

        startedHidingCallback.waitForCallback(0);
        assertThat(
                "startedHidingCallback with incorrect LayoutType",
                startedHidingCallback.layoutType,
                is(LayoutType.SIMPLE_ANIMATION));

        finishedHidingCallback.waitForCallback(0);
        assertThat(
                "finishedHidingCallback with incorrectLayoutType",
                finishedHidingCallback.layoutType,
                is(LayoutType.SIMPLE_ANIMATION));

        startedShowingCallback.waitForCallback(1);
        assertThat(
                "startedShowingCallback with incorrectLayoutType",
                startedShowingCallback.layoutType,
                is(LayoutType.BROWSING));

        finishedShowingCallback.waitForCallback(1);
        assertThat(
                "finishedShowingCallback with incorrectLayoutType",
                finishedShowingCallback.layoutType,
                is(LayoutType.BROWSING));
    }

    private void setUpShowAndHideLayoutObserverNotification(
            LayoutObserverCallbackHelper startedShowingCallback,
            LayoutObserverCallbackHelper finishedShowingCallback,
            LayoutObserverCallbackHelper startedHidingCallback,
            LayoutObserverCallbackHelper finishedHidingCallback)
            throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    initializeLayoutManagerPhone(2, 0);
                    mManager.addObserver(
                            new LayoutStateProvider.LayoutStateObserver() {
                                @Override
                                public void onStartedShowing(int layoutType) {
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
                                public void onStartedHiding(int layoutType) {
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

                    Assert.assertEquals(
                            LayoutType.BROWSING, mManager.getActiveLayout().getLayoutType());
                });

        if (mManager.isLayoutVisible(LayoutType.BROWSING)) {
            startedShowingCallback.layoutType = LayoutType.BROWSING;
        } else {
            startedShowingCallback.waitForCallback(0);
        }
        Assert.assertEquals(LayoutType.BROWSING, startedShowingCallback.layoutType);
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        // Load the browser process.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
                });

        mTabModelSelectorSupplier = () -> mTabModelSelector;
    }

    @After
    public void tearDown() {
        setAccessibilityEnabledForTesting(null);
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

        Assert.assertEquals(
                "Unexpected model change after side swipe",
                model.isIncognito(),
                mTabModelSelector.isIncognitoSelected());

        Assert.assertEquals("Wrong index after side swipe", finalIndex, model.index());
        Assert.assertEquals(
                "Wrong current tab id", finalId, TabModelUtils.getCurrentTab(model).getId());
        Assert.assertTrue(
                "LayoutManager#getActiveLayout() should be StaticLayout",
                mManager.getActiveLayout() instanceof StaticLayout);
    }

    private void performToolbarSideSwipe(@ScrollDirection int direction) {
        Assert.assertTrue(
                "Unexpected direction for side swipe " + direction,
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
        Assert.assertTrue(
                "LayoutManager#getActiveLayout() should be ToolbarSwipeLayout",
                mManager.getActiveLayout() instanceof ToolbarSwipeLayout);
        Assert.assertTrue(
                "LayoutManager took too long to finish the animations",
                simulateTime(mManager, 1000));
    }

    private void finishToolbarSideSwipe() {
        final SwipeHandler eventHandler = mManager.getToolbarSwipeHandler();
        Assert.assertNotNull("LayoutManager#getToolbarSwipeHandler() returned null", eventHandler);

        eventHandler.onSwipeFinished();
        Assert.assertTrue(
                "LayoutManager took too long to finish the animations",
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LayoutManagerChrome layoutManagerChrome = getLayoutManagerChrome();
                    Assert.assertNotNull(
                            "Must be called after initialization", layoutManagerChrome);
                    layoutManagerChrome.addObserver(
                            new LayoutStateProvider.LayoutStateObserver() {
                                @Override
                                public void onStartedShowing(int layoutType) {
                                    observationSequence.add(
                                            new LayoutStateLayoutType(
                                                    LayoutState.STARTING_TO_SHOW, layoutType));
                                }

                                @Override
                                public void onFinishedShowing(int layoutType) {
                                    observationSequence.add(
                                            new LayoutStateLayoutType(
                                                    LayoutState.SHOWING, layoutType));
                                }

                                @Override
                                public void onStartedHiding(int layoutType) {
                                    observationSequence.add(
                                            new LayoutStateLayoutType(
                                                    LayoutState.STARTING_TO_HIDE, layoutType));
                                }

                                @Override
                                public void onFinishedHiding(int layoutType) {
                                    observationSequence.add(
                                            new LayoutStateLayoutType(
                                                    LayoutState.HIDDEN, layoutType));
                                }
                            });
                });
    }

    @Override
    public MockTab createTab(int id, boolean incognito) {
        Profile profile = ProfileManager.getLastUsedRegularProfile();
        return MockTab.createAndInitialize(
                id, incognito ? profile.getPrimaryOTRProfile(true) : profile);
    }
}
