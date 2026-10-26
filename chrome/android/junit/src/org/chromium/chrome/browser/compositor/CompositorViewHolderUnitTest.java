// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.PictureInPictureParams;
import android.content.ClipData;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.drawable.ColorDrawable;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.util.Size;
import android.view.ContextThemeWrapper;
import android.view.DragAndDropPermissions;
import android.view.DragEvent;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnDragListener;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.DragEventBuilder;

import org.chromium.base.BaseFeatures;
import org.chromium.base.InputHintChecker;
import org.chromium.base.InputHintCheckerJni;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.layouts.EventFilter.EventType;
import org.chromium.chrome.browser.layouts.SceneOverlay;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.theme.ToolbarThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.UiUpdateRequest;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.UiUpdateRequest.UpdateReason;
import org.chromium.chrome.browser.ui.side_ui.SideUiStateProvider;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.components.browser_ui.widget.TouchEventObserver;
import org.chromium.components.content_capture.ContentCaptureFeatures;
import org.chromium.components.content_capture.ContentCaptureFeaturesJni;
import org.chromium.components.content_capture.OnscreenContentProvider;
import org.chromium.components.content_capture.OnscreenContentProviderJni;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.prefs.PrefService;
import org.chromium.content_public.browser.ViewFocusChangeSuppression;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.ApplicationViewportInsetTracker;
import org.chromium.ui.base.EventForwarder;
import org.chromium.ui.base.EventOffsetHandler;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.dragdrop.DragStateTracker;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.mojom.VirtualKeyboardMode;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link CompositorViewHolder}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({
    ChromeFeatureList.FULLSCREEN_INSETS_API_MIGRATION,
    ChromeFeatureList.FULLSCREEN_INSETS_API_MIGRATION_ON_AUTOMOTIVE
})
public class CompositorViewHolderUnitTest {
    // Subclass needed to stub getWindowVisibleDisplayFrame(), set initial dimensions without
    // triggering onSizeChanged(), and forward post() to the main looper when detached.
    private static class TestCompositorViewHolder extends CompositorViewHolder {
        @Nullable Rect mWindowVisibleDisplayFrame;
        private boolean mInSetSizeForTesting;

        TestCompositorViewHolder(Context c) {
            super(c, /* attrs= */ null);
        }

        void setWidthForTesting(int width) {
            mInSetSizeForTesting = true;
            setRight(width);
            mInSetSizeForTesting = false;
        }

        void setHeightForTesting(int height) {
            mInSetSizeForTesting = true;
            setBottom(height);
            mInSetSizeForTesting = false;
        }

        @Override
        protected void onSizeChanged(int w, int h, int oldw, int oldh) {
            if (!mInSetSizeForTesting) {
                super.onSizeChanged(w, h, oldw, oldh);
            }
        }

        @Override
        public void getWindowVisibleDisplayFrame(Rect outRect) {
            if (mWindowVisibleDisplayFrame != null) {
                outRect.set(mWindowVisibleDisplayFrame);
            } else {
                super.getWindowVisibleDisplayFrame(outRect);
            }
        }

        @Override
        public boolean post(Runnable action) {
            return new Handler(Looper.getMainLooper()).post(action);
        }
    }

    // Subclass needed to return mock DragAndDropPermissions.
    public static class DropTestActivity extends Activity {
        final List<DragAndDropPermissions> mPermissions = new ArrayList<>();

        @Override
        public DragAndDropPermissions requestDragAndDropPermissions(DragEvent event) {
            DragAndDropPermissions permissions = mock(DragAndDropPermissions.class);
            mPermissions.add(permissions);
            return permissions;
        }
    }

    // Subclass needed to record calls to setHasActiveTouchInterceptors(), which has no getter.
    private static class TestCompositorView extends CompositorView {
        @Nullable Boolean mLastActiveTouchInterceptors;

        TestCompositorView(Context c, LayoutRenderHost host) {
            super(c, host);
        }

        @Override
        public void setHasActiveTouchInterceptors(boolean hasActiveTouchInterceptors) {
            mLastActiveTouchInterceptors = hasActiveTouchInterceptors;
        }
    }

    // Since these tests don't depend on the heights being pixels, we can use these as dpi directly.
    private static final int TOOLBAR_HEIGHT = 56;
    private static final int KEYBOARD_HEIGHT = 741;
    private static final int VIEWPORT_WIDTH = 1080;
    private static final int VIEWPORT_HEIGHT = 941;

    private static final int SIDE_UI_START_WIDTH = 60;
    private static final int SIDE_UI_END_WIDTH = 70;
    private static final SideUiSpecs EMPTY_SIDE_UI_SPECS =
            new SideUiSpecs(/* leftContainerWidth= */ 0, /* rightContainerWidth= */ 0);

    private static final long TOUCH_TIME = 0;
    private static final MotionEvent MOTION_EVENT_DOWN =
            MotionEvent.obtain(TOUCH_TIME, TOUCH_TIME, MotionEvent.ACTION_DOWN, 1, 1, 0);
    private static final MotionEvent MOTION_EVENT_UP =
            MotionEvent.obtain(TOUCH_TIME, TOUCH_TIME, MotionEvent.ACTION_UP, 1, 1, 0);

    private static final MotionEvent MOTION_ACTION_HOVER_ENTER =
            MotionEvent.obtain(TOUCH_TIME, TOUCH_TIME, MotionEvent.ACTION_HOVER_ENTER, 1, 1, 0);

    private static final MotionEvent MOTION_ACTION_BUTTON_RELEASE_MOUSE;

    static {
        MotionEvent.PointerCoords[] coords = new MotionEvent.PointerCoords[1];
        coords[0] = new MotionEvent.PointerCoords();
        coords[0].x = 1f;
        coords[0].y = 1f;

        MotionEvent.PointerProperties[] properties = new MotionEvent.PointerProperties[1];
        properties[0] = new MotionEvent.PointerProperties();
        properties[0].id = 0;
        properties[0].toolType = MotionEvent.TOOL_TYPE_MOUSE;

        MOTION_ACTION_BUTTON_RELEASE_MOUSE =
                MotionEvent.obtain(
                        0,
                        0,
                        MotionEvent.ACTION_BUTTON_RELEASE,
                        1,
                        properties,
                        coords,
                        0,
                        0,
                        1f,
                        1f,
                        0,
                        0,
                        InputDevice.SOURCE_CLASS_POINTER,
                        0);
    }

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;
    @Mock private ControlContainer mControlContainer;
    @Mock private WebContents mWebContents;
    @Mock private CompositorView.Natives mCompositorViewJni;
    @Mock private ResourceManager mResourceManager;
    @Mock private LayoutManagerImpl mLayoutManager;
    @Mock private KeyboardVisibilityDelegate mMockKeyboard;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private DynamicResourceLoader mDynamicResourceLoader;
    @Mock private PrefService mPrefService;
    @Mock private OnscreenContentProvider.Natives mOnscreenContentProviderJni;
    @Mock private ContentCaptureFeatures.Natives mContentCaptureFeaturesJni;
    @Mock private InputHintChecker.Natives mInputHintCheckerJni;
    @Mock private MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    @Mock private InsetObserver mInsetObserver;
    @Mock private ToolbarThemeColorProvider mToolbarThemeColorProvider;
    @Mock private SideUiStateProvider mSideUiStateProvider;
    @Mock private OnDragListener mOnDragListener;
    @Mock private EventForwarder mEventForwarder;

    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    private final ActivityTabProvider mActivityTabProvider = new ActivityTabProvider();
    private Activity mActivity;
    private Context mContext;
    private ContentView mContentView;
    private TestCompositorView mCompositorView;
    private MockTabModelSelector mTabModelSelector;
    private Tab mTab;
    private TestCompositorViewHolder mCompositorViewHolder;
    private BrowserControlsManager mBrowserControlsManager;
    private ApplicationViewportInsetTracker mViewportInsets;
    private SettableNonNullObservableSupplier<Integer> mKeyboardInsetSupplier;
    private SettableNonNullObservableSupplier<Integer> mKeyboardAccessoryInsetSupplier;
    private OneshotSupplierImpl<SideUiStateProvider> mSideUiStateProviderSupplier;
    private final UserDataHost mUserDataHost = new UserDataHost();

    @Before
    public void setUp() {
        OnscreenContentProviderJni.setInstanceForTesting(mOnscreenContentProviderJni);
        ContentCaptureFeaturesJni.setInstanceForTesting(mContentCaptureFeaturesJni);
        InputHintCheckerJni.setInstanceForTesting(mInputHintCheckerJni);
        CompositorViewJni.setInstanceForTesting(mCompositorViewJni);
        when(mCompositorViewJni.init(any(), any(), any())).thenReturn(1L);
        when(mCompositorViewJni.getResourceManager(1L)).thenReturn(mResourceManager);

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mActivity.getWindow().getDecorView().setFitsSystemWindows(true);

        // Setup the mock keyboard.
        KeyboardVisibilityDelegate.setInstanceForTesting(mMockKeyboard);

        mViewportInsets = ApplicationViewportInsetTracker.createForTests();
        when(mInsetObserver.isKeyboardInOverlayMode()).thenReturn(false);
        mViewportInsets.setInsetObserver(mInsetObserver);

        mKeyboardInsetSupplier = ObservableSuppliers.createNonNull(0);
        mViewportInsets.setKeyboardInsetSupplier(mKeyboardInsetSupplier);
        mKeyboardAccessoryInsetSupplier = ObservableSuppliers.createNonNull(0);
        mViewportInsets.setKeyboardAccessoryInsetSupplier(mKeyboardAccessoryInsetSupplier);
        mSideUiStateProviderSupplier = new OneshotSupplierImpl<>();

        when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);

        // Setup the TabModelSelector.
        mTabModelSelector =
                new MockTabModelSelector(
                        mProfile,
                        mIncognitoProfile,
                        0,
                        0,
                        (id, incognito) -> {
                            MockTab tab =
                                    spy(new MockTab(id, incognito ? mIncognitoProfile : mProfile));
                            when(tab.isDetachedFromActivity()).thenReturn(false);
                            return tab;
                        });
        mTab = mTabModelSelector.addMockTab();
        mTabModelSelector.getModel(false).setIndex(0, TabSelectionType.FROM_NEW);

        // Setup for BrowserControlsManager which initiates content/control offset changes
        // for CompositorViewHolder.
        View containerView = new View(mActivity);
        mContentView = ContentView.createContentView(mActivity, /* webContents= */ null);
        mActivity.setContentView(mContentView);

        when(mControlContainer.getView()).thenReturn(containerView);
        when(mTab.isUserInteractable()).thenReturn(true);

        BrowserControlsManager browserControlsManager =
                new BrowserControlsManager(
                        mActivity,
                        BrowserControlsStateProvider.ControlsPosition.TOP,
                        mMultiWindowModeStateDispatcher);
        mBrowserControlsManager = spy(browserControlsManager);
        mBrowserControlsManager.initialize(
                mControlContainer,
                mActivityTabProvider,
                mTabModelSelector,
                R.dimen.control_container_height);
        when(mBrowserControlsManager.getTab()).thenReturn(mTab);

        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        when(mResourceManager.getDynamicResourceLoader()).thenReturn(mDynamicResourceLoader);

        mCompositorViewHolder = new TestCompositorViewHolder(mContext);

        mCompositorView = new TestCompositorView(mActivity, mCompositorViewHolder);
        mCompositorView.initNativeCompositor(mWindowAndroid, /* tabContentManager= */ null);

        mCompositorViewHolder.setToolbarThemeColorProvider(mToolbarThemeColorProvider);
        mCompositorViewHolder.setLayoutManager(mLayoutManager);
        mCompositorViewHolder.setControlContainer(mControlContainer);
        mCompositorViewHolder.setCompositorViewForTesting(mCompositorView);
        mCompositorViewHolder.setBrowserControlsManager(mBrowserControlsManager);
        mCompositorViewHolder.setApplicationViewportInsetSupplier(mViewportInsets);
        mCompositorViewHolder.onFinishNativeInitialization(
                mTabModelSelector, null, ObservableSuppliers.alwaysZero());
        mCompositorViewHolder.setSideUiStateProviderSupplier(mSideUiStateProviderSupplier);
        when(mSideUiStateProvider.getCurrentSideUiSpecs()).thenReturn(EMPTY_SIDE_UI_SPECS);
        when(mSideUiStateProvider.getExpectedSideUiSpecsForTab(any()))
                .thenReturn(EMPTY_SIDE_UI_SPECS);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getContentView()).thenReturn(mContentView);
        when(mTab.getView()).thenReturn(mContentView);
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);

        ViewAndroidDelegate viewDelegate = ViewAndroidDelegate.createBasicDelegate(mContentView);
        when(mWebContents.getViewAndroidDelegate()).thenReturn(viewDelegate);
        runCurrentTasks();
        clearInvocations(mCompositorViewJni);
    }

    @After
    public void tearDown() {
        LocalizationUtils.setRtlForTesting(false);
    }

    // controlsResizeView tests ---
    // For these tests, we will simulate the scrolls assuming we either completely show or hide (or
    // scroll until the min-height) the controls and don't leave at in-between positions. The reason
    // is that CompositorViewHolder only flips the mControlsResizeView bit if the controls are
    // idle, meaning they're at the min-height or fully shown. Making sure the controls snap to
    // these two positions is not CVH's responsibility as it's handled in native code by compositor
    // or blink.

    @Test
    public void testControlsResizeViewChanges() {
        // Let's use simpler numbers for this test.
        final int topHeight = 100;
        final int topMinHeight = 0;

        TabModelSelectorTabObserver tabControlsObserver =
                mBrowserControlsManager.getTabControlsObserverForTesting();

        mBrowserControlsManager.setTopControlsHeight(topHeight, topMinHeight);

        // Send initial offsets.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ 0,
                /* bottomControlsOffsetY= */ 0,
                /* contentOffsetY= */ 100,
                /* topControlsMinHeightOffsetY= */ 0,
                /* bottomControlsMinHeightOffsetY= */ 0);
        // Initially, the controls should be fully visible.
        assertTrue(
                "Browser controls aren't fully visible.",
                BrowserControlsUtils.areBrowserControlsFullyVisible(mBrowserControlsManager));
        // ControlsResizeView is false, but it should be true when the controls are fully visible.
        verify(mCompositorViewJni).onControlsResizeViewChanged(anyLong(), any(), eq(true));
        clearInvocations(mCompositorViewJni);

        // Scroll to fully hidden.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ -100,
                /* bottomControlsOffsetY= */ 0,
                /* contentOffsetY= */ 0,
                /* topControlsMinHeightOffsetY= */ 0,
                /* bottomControlsMinHeightOffsetY= */ 0);
        assertTrue(
                "Browser controls aren't at min-height.",
                mBrowserControlsManager.areBrowserControlsAtMinHeight());
        // ControlsResizeView is true, but it should be false when the controls are hidden.
        verify(mCompositorViewJni).onControlsResizeViewChanged(anyLong(), any(), eq(false));
        clearInvocations(mCompositorViewJni);

        // Now, scroll back to fully visible.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ 0,
                /* bottomControlsOffsetY= */ 0,
                /* contentOffsetY= */ 100,
                /* topControlsMinHeightOffsetY= */ 0,
                /* bottomControlsMinHeightOffsetY= */ 0);
        assertFalse(
                "Browser controls are hidden when they should be fully visible.",
                mBrowserControlsManager.areBrowserControlsAtMinHeight());
        assertTrue(
                "Browser controls aren't fully visible.",
                BrowserControlsUtils.areBrowserControlsFullyVisible(mBrowserControlsManager));
        // #controlsResizeView should be flipped back to true.
        // ControlsResizeView is false, but it should be true when the controls are fully visible.
        verify(mCompositorViewJni).onControlsResizeViewChanged(anyLong(), any(), eq(true));
        clearInvocations(mCompositorViewJni);
    }

    // Test that a page opted in to view transitions gets an early resize event
    // on the controls starting to show.
    @Test
    @DisableFeatures(ChromeFeatureList.BROWSER_CONTROLS_EARLY_RESIZE)
    public void testResizeViewOnWillShowControlsWithViewTransition() {
        final int topHeight = 100;
        final int topMinHeight = 0;

        TabModelSelectorTabObserver tabControlsObserver =
                mBrowserControlsManager.getTabControlsObserverForTesting();

        mBrowserControlsManager.setTopControlsHeight(topHeight, topMinHeight);

        // Send initial offsets.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ -topHeight,
                /* bottomControlsOffsetY= */ 0,
                /* contentOffsetY= */ 0,
                /* topControlsMinHeightOffsetY= */ 0,
                /* bottomControlsMinHeightOffsetY= */ 0);
        // Initially, the controls should be hidden.
        assertTrue(
                "Browser controls aren't fully hidden.",
                BrowserControlsUtils.areBrowserControlsOffScreen(mBrowserControlsManager));

        // Simulate the browser issuing a "show browser controls" signal to the renderer.
        mCompositorViewHolder.onWillShowBrowserControls(/* viewTransitionOptIn= */ false);

        // This should must not cause the controls to start resizing the view yet.
        verify(mCompositorViewJni, never())
                .onControlsResizeViewChanged(anyLong(), any(), anyBoolean());
        clearInvocations(mCompositorViewJni);

        // Do the same but this time with the page having the view transition opt in.
        mCompositorViewHolder.onWillShowBrowserControls(/* viewTransitionOptIn= */ true);

        // This should cause the controls to start resizing the view.
        verify(mCompositorViewJni).onControlsResizeViewChanged(anyLong(), any(), eq(true));
        clearInvocations(mCompositorViewJni);
    }

    // Test for the browser controls early resize flagged behavior.
    // https://crbug.com/332331777
    @Test
    @EnableFeatures(ChromeFeatureList.BROWSER_CONTROLS_EARLY_RESIZE)
    public void testResizeViewOnWillShowControls() {
        final int topHeight = 100;
        final int topMinHeight = 0;

        TabModelSelectorTabObserver tabControlsObserver =
                mBrowserControlsManager.getTabControlsObserverForTesting();

        mBrowserControlsManager.setTopControlsHeight(topHeight, topMinHeight);

        // Send initial offsets.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ -topHeight,
                /* bottomControlsOffsetY= */ 0,
                /* contentOffsetY= */ 0,
                /* topControlsMinHeightOffsetY= */ 0,
                /* bottomControlsMinHeightOffsetY= */ 0);
        // Initially, the controls should be hidden.
        assertTrue(
                "Browser controls aren't fully hidden.",
                BrowserControlsUtils.areBrowserControlsOffScreen(mBrowserControlsManager));

        // Simulate the browser issuing a "show browser controls" signal to the renderer.
        mCompositorViewHolder.onWillShowBrowserControls(/* viewTransitionOptIn= */ false);

        // This should cause the controls to start resizing the view.
        verify(mCompositorViewJni).onControlsResizeViewChanged(anyLong(), any(), eq(true));
        clearInvocations(mCompositorViewJni);

        // Simulating a show-animation partially updating the controls, this shouldn't cause another
        // resize.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ -topHeight / 2,
                /* bottomControlsOffsetY= */ 0,
                /* contentOffsetY= */ topHeight / 2,
                /* topControlsMinHeightOffsetY= */ 0,
                /* bottomControlsMinHeightOffsetY= */ 0);
        verify(mCompositorViewJni, never())
                .onControlsResizeViewChanged(anyLong(), any(), anyBoolean());
        clearInvocations(mCompositorViewJni);

        // The controls finished animating in. Since they already resized the view, this should also
        // be a no-op.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ 0,
                /* bottomControlsOffsetY= */ 0,
                /* contentOffsetY= */ topHeight,
                /* topControlsMinHeightOffsetY= */ 0,
                /* bottomControlsMinHeightOffsetY= */ 0);

        verify(mCompositorViewJni, never())
                .onControlsResizeViewChanged(anyLong(), any(), anyBoolean());
        clearInvocations(mCompositorViewJni);

        // The controls going back to hidden should resize the view as usual.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ -topHeight,
                /* bottomControlsOffsetY= */ 0,
                /* contentOffsetY= */ 0,
                /* topControlsMinHeightOffsetY= */ 0,
                /* bottomControlsMinHeightOffsetY= */ 0);
        verify(mCompositorViewJni).onControlsResizeViewChanged(anyLong(), any(), eq(false));
        clearInvocations(mCompositorViewJni);
    }

    // TODO(bokan): Ensure disabling the flag-guard reverts to old behavior.
    // https://crbug.com/332331777
    @Test
    @DisableFeatures(ChromeFeatureList.BROWSER_CONTROLS_EARLY_RESIZE)
    public void testResizeViewOnWillShowControlsFlagGuarded() {
        final int topHeight = 100;
        final int topMinHeight = 0;

        TabModelSelectorTabObserver tabControlsObserver =
                mBrowserControlsManager.getTabControlsObserverForTesting();

        mBrowserControlsManager.setTopControlsHeight(topHeight, topMinHeight);

        // Send initial offsets.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ -topHeight,
                /* bottomControlsOffsetY= */ 0,
                /* contentOffsetY= */ 0,
                /* topControlsMinHeightOffsetY= */ 0,
                /* bottomControlsMinHeightOffsetY= */ 0);
        // Initially, the controls should be hidden.
        assertTrue(
                "Browser controls aren't fully hidden.",
                BrowserControlsUtils.areBrowserControlsOffScreen(mBrowserControlsManager));

        // Simulate the browser issuing a "show browser controls" signal to the renderer.
        mCompositorViewHolder.onWillShowBrowserControls(/* viewTransitionOptIn= */ false);

        // This should must not cause the controls to start resizing the view yet.
        verify(mCompositorViewJni, never())
                .onControlsResizeViewChanged(anyLong(), any(), anyBoolean());
        clearInvocations(mCompositorViewJni);

        // The controls finished animating in. Since they already resized the view, this should
        // cause the resize the occur.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ 0,
                /* bottomControlsOffsetY= */ 0,
                /* contentOffsetY= */ topHeight,
                /* topControlsMinHeightOffsetY= */ 0,
                /* bottomControlsMinHeightOffsetY= */ 0);

        verify(mCompositorViewJni).onControlsResizeViewChanged(anyLong(), any(), eq(true));
        clearInvocations(mCompositorViewJni);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.FULLSCREEN_INSETS_API_MIGRATION,
        ChromeFeatureList.FULLSCREEN_INSETS_API_MIGRATION_ON_AUTOMOTIVE
    })
    public void testHandleSystemUiVisibilityChangesWithUpdatedFullscreenApis() {
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));

        mCompositorViewHolder.onNativeLibraryReady(mWindowAndroid, null, null);
        mCompositorViewHolder.handleSystemUiVisibilityChange();
    }

    @Test
    public void testControlsResizeViewChangesWithMinHeight() {
        // Let's use simpler numbers for this test. We'll simulate the scrolling logic in the
        // compositor. Which means the top and bottom controls will have the same normalized ratio.
        // E.g. if the top content offset is 25 (at min-height so the normalized ratio is 0), the
        // bottom content offset will be 0 (min-height-0 + normalized-ratio-0 * rest-of-height-60).
        final int topHeight = 100;
        final int topMinHeight = 25;
        final int bottomHeight = 60;
        final int bottomMinHeight = 0;

        TabModelSelectorTabObserver tabControlsObserver =
                mBrowserControlsManager.getTabControlsObserverForTesting();

        mBrowserControlsManager.setTopControlsHeight(topHeight, topMinHeight);
        mBrowserControlsManager.setBottomControlsHeight(bottomHeight, bottomMinHeight);

        // Send initial offsets.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ 0,
                /* bottomControlsOffsetY= */ 0,
                /* contentOffsetY= */ 100,
                /* topControlsMinHeightOffsetY= */ 25,
                /* bottomControlsMinHeightOffsetY= */ 0);
        // Initially, the controls should be fully visible.
        assertTrue(
                "Browser controls aren't fully visible.",
                BrowserControlsUtils.areBrowserControlsFullyVisible(mBrowserControlsManager));
        // ControlsResizeView is false, but it should be true when the controls are fully visible.
        verify(mCompositorViewJni).onControlsResizeViewChanged(anyLong(), any(), eq(true));
        clearInvocations(mCompositorViewJni);

        // Scroll all the way to the min-height.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ -75,
                /* bottomControlsOffsetY= */ 60,
                /* contentOffsetY= */ 25,
                /* topControlsMinHeightOffsetY= */ 25,
                /* bottomControlsMinHeightOffsetY= */ 0);
        assertTrue(
                "Browser controls aren't at min-height.",
                mBrowserControlsManager.areBrowserControlsAtMinHeight());
        // ControlsResizeView is true but it should be false when the controls are at min-height.
        verify(mCompositorViewJni).onControlsResizeViewChanged(anyLong(), any(), eq(false));
        clearInvocations(mCompositorViewJni);

        // Now, scroll back to fully visible.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ 0,
                /* bottomControlsOffsetY= */ 0,
                /* contentOffsetY= */ 100,
                /* topControlsMinHeightOffsetY= */ 25,
                /* bottomControlsMinHeightOffsetY= */ 0);
        assertFalse(
                "Browser controls are at min-height when they should be fully visible.",
                mBrowserControlsManager.areBrowserControlsAtMinHeight());
        assertTrue(
                "Browser controls aren't fully visible.",
                BrowserControlsUtils.areBrowserControlsFullyVisible(mBrowserControlsManager));
        // #controlsResizeView should be flipped back to true.
        verify(mCompositorViewJni).onControlsResizeViewChanged(anyLong(), any(), eq(true));
        clearInvocations(mCompositorViewJni);
    }

    @Test
    public void testControlsResizeViewWhenControlsAreNotIdle() {
        // Let's use simpler numbers for this test. We'll simulate the scrolling logic in the
        // compositor. Which means the top and bottom controls will have the same normalized ratio.
        // E.g. if the top content offset is 25 (at min-height so the normalized ratio is 0), the
        // bottom content offset will be 0 (min-height-0 + normalized-ratio-0 * rest-of-height-60).
        final int topHeight = 100;
        final int topMinHeight = 25;
        final int bottomHeight = 60;
        final int bottomMinHeight = 0;

        TabModelSelectorTabObserver tabControlsObserver =
                mBrowserControlsManager.getTabControlsObserverForTesting();

        mBrowserControlsManager.setTopControlsHeight(topHeight, topMinHeight);
        mBrowserControlsManager.setBottomControlsHeight(bottomHeight, bottomMinHeight);

        // Send initial offsets.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ 0,
                /* bottomControlsOffsetY= */ 0,
                /* contentOffsetY= */ 100,
                /* topControlsMinHeightOffsetY= */ 25,
                /* bottomControlsMinHeightOffsetY= */ 0);
        // ControlsResizeView is false but it should be true when the controls are fully visible.
        verify(mCompositorViewJni).onControlsResizeViewChanged(anyLong(), any(), eq(true));
        clearInvocations(mCompositorViewJni);

        // Scroll a little hide the controls partially.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ -25,
                /* bottomControlsOffsetY= */ 20,
                /* contentOffsetY= */ 75,
                /* topControlsMinHeightOffsetY= */ 25,
                /* bottomControlsMinHeightOffsetY= */ 0);
        // ControlsResizeView is false, but it should still be true. No-op updates won't trigger a
        // changed event.
        verify(mCompositorViewJni, times(0))
                .onControlsResizeViewChanged(anyLong(), any(), eq(true));
        clearInvocations(mCompositorViewJni);

        // Scroll controls all the way to the min-height.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ -75,
                /* bottomControlsOffsetY= */ 60,
                /* contentOffsetY= */ 25,
                /* topControlsMinHeightOffsetY= */ 25,
                /* bottomControlsMinHeightOffsetY= */ 0);
        // ControlsResizeView is true but it should've flipped to false since the controls are idle
        // now.
        verify(mCompositorViewJni).onControlsResizeViewChanged(anyLong(), any(), eq(false));
        clearInvocations(mCompositorViewJni);

        // Scroll controls to show a little more.
        tabControlsObserver.onBrowserControlsOffsetChanged(
                mTab,
                /* topControlsOffsetY= */ -50,
                /* bottomControlsOffsetY= */ 40,
                /* contentOffsetY= */ 50,
                /* topControlsMinHeightOffsetY= */ 25,
                /* bottomControlsMinHeightOffsetY= */ 0);
        // ControlsResizeView is true, but it should still be false. No-op updates won't trigger a
        // changed event.
        verify(mCompositorViewJni, times(0))
                .onControlsResizeViewChanged(anyLong(), any(), eq(false));
    }

    // --- controlsResizeView tests

    // Keyboard resize tests for geometrychange event fired to JS.
    @Test
    public void testResizeNotificationsDueToKeyboardShow_OSKeyboardResize() {
        mCompositorViewHolder.updateVirtualKeyboardMode(VirtualKeyboardMode.OVERLAYS_CONTENT);
        reset(mWebContents);

        // adjustedHeight is the height of the CompositorViewHolder from Android View layout
        // after showing the keyboard. This simulates a reduced layout height from the keyboard
        // taking up the bottom space.
        int adjustedHeight = VIEWPORT_HEIGHT - KEYBOARD_HEIGHT;

        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(true);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(KEYBOARD_HEIGHT);
        mCompositorViewHolder.setWidthForTesting(VIEWPORT_WIDTH);
        mCompositorViewHolder.setHeightForTesting(adjustedHeight);

        // This triggers handleWindowInsetChanged automatically.
        mKeyboardInsetSupplier.set(KEYBOARD_HEIGHT);

        // The webcontents weren't resized, so no `setSize` should come through.
        verify(mWebContents, never()).setSize(anyInt(), anyInt());
        verify(mCompositorViewJni, times(1))
                .notifyVirtualKeyboardOverlayRect(
                        eq(1L),
                        eq(mWebContents),
                        eq(0),
                        eq(0), // Expected y is 0 when View is resized to 200 and keyboard is 741
                        eq(VIEWPORT_WIDTH),
                        eq(KEYBOARD_HEIGHT));

        reset(mWebContents);
        clearInvocations(mCompositorViewJni);

        // Hide the keyboard.
        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(false);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(0);
        mCompositorViewHolder.setWidthForTesting(VIEWPORT_WIDTH);
        mCompositorViewHolder.setHeightForTesting(VIEWPORT_HEIGHT);
        // This triggers handleWindowInsetChanged automatically.
        mKeyboardInsetSupplier.set(0);

        verify(mWebContents, never()).setSize(anyInt(), anyInt());
        verify(mCompositorViewJni, times(1))
                .notifyVirtualKeyboardOverlayRect(1L, mWebContents, 0, 0, 0, 0);
    }

    @Test
    public void testWebContentResizeTriggeredDueToKeyboardTransition_hasNoTransientOvershoot() {
        mCompositorViewHolder.updateVirtualKeyboardMode(VirtualKeyboardMode.OVERLAYS_CONTENT);
        reset(mWebContents);

        int adjustedHeight = VIEWPORT_HEIGHT - KEYBOARD_HEIGHT;

        mCompositorViewHolder.setWidthForTesting(VIEWPORT_WIDTH);

        // Establish the baseline viewport size before keyboard insets change.
        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(false);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(0);
        mCompositorViewHolder.setHeightForTesting(VIEWPORT_HEIGHT);
        mCompositorViewHolder.updateWebContentsSize(mTab);
        reset(mWebContents);

        // Keyboard show: inset is updated before layout applies the reduced view height.
        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(true);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(KEYBOARD_HEIGHT);
        mCompositorViewHolder.setHeightForTesting(VIEWPORT_HEIGHT);
        mKeyboardInsetSupplier.set(KEYBOARD_HEIGHT);
        mCompositorViewHolder.updateWebContentsSize(mTab);

        // After layout, view height is reduced and compensation should still keep size stable.
        mCompositorViewHolder.setHeightForTesting(adjustedHeight);
        mCompositorViewHolder.updateWebContentsSize(mTab);

        // Keyboard hide: inset clears before layout restores the full view height.
        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(false);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(0);
        mCompositorViewHolder.setHeightForTesting(adjustedHeight);
        mKeyboardInsetSupplier.set(0);
        mCompositorViewHolder.updateWebContentsSize(mTab);

        // After layout restoration, size should remain stable.
        mCompositorViewHolder.setHeightForTesting(VIEWPORT_HEIGHT);
        mCompositorViewHolder.updateWebContentsSize(mTab);

        verify(mWebContents, atLeast(1)).setSize(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
        verify(mWebContents, never()).setSize(VIEWPORT_WIDTH, VIEWPORT_HEIGHT + KEYBOARD_HEIGHT);
        verify(mWebContents, never()).setSize(VIEWPORT_WIDTH, adjustedHeight);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.VIRTUAL_KEYBOARD_RESIZES_CONTENT_TRANSIENT_OVERSHOOT_FIX)
    public void
            testWebContentResizeTriggeredDueToKeyboardTransition_resizesContent_hasNoTransientOvershoot() {
        mCompositorViewHolder.updateVirtualKeyboardMode(VirtualKeyboardMode.RESIZES_CONTENT);
        reset(mWebContents);

        int adjustedHeight = VIEWPORT_HEIGHT - KEYBOARD_HEIGHT;

        mCompositorViewHolder.setWidthForTesting(VIEWPORT_WIDTH);

        // Establish the baseline viewport size before keyboard insets change.
        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(false);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(0);
        mCompositorViewHolder.setHeightForTesting(VIEWPORT_HEIGHT);
        mCompositorViewHolder.updateWebContentsSize(mTab);
        reset(mWebContents);

        // Keyboard show: browser controls hide (simulating transient height increase) before
        // layout applies the reduced view height.
        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(true);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(KEYBOARD_HEIGHT);
        mCompositorViewHolder.setHeightForTesting(VIEWPORT_HEIGHT + 100);
        mKeyboardInsetSupplier.set(0);
        mCompositorViewHolder.updateWebContentsSize(mTab);

        // Verify that transient height increases caused by browser controls hiding before the
        // WindowAndroid layout completes are clamped to the stable closed WebContents height.
        verify(mWebContents, never())
                .setSize(VIEWPORT_WIDTH, VIEWPORT_HEIGHT + 100 - TOOLBAR_HEIGHT);

        // Once the keyboard inset arrives, it should resize to the adjusted height.
        mCompositorViewHolder.setHeightForTesting(adjustedHeight);
        mKeyboardInsetSupplier.set(KEYBOARD_HEIGHT);
        mCompositorViewHolder.updateWebContentsSize(mTab);
        verify(mWebContents, atLeast(1)).setSize(VIEWPORT_WIDTH, adjustedHeight - TOOLBAR_HEIGHT);

        // Keyboard hide: browser controls show (simulating transient height decrease) before
        // layout applies the restored view height.
        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(false);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(0);
        mCompositorViewHolder.setHeightForTesting(adjustedHeight - 100);
        mKeyboardInsetSupplier.set(0);
        mCompositorViewHolder.updateWebContentsSize(mTab);

        // Verify that transient height decreases caused by browser controls showing before the
        // WindowAndroid layout completes are clamped to the stable open WebContents height.
        verify(mWebContents, never())
                .setSize(VIEWPORT_WIDTH, adjustedHeight - 100 - TOOLBAR_HEIGHT);

        // After layout restoration, size should restore cleanly.
        mCompositorViewHolder.setHeightForTesting(VIEWPORT_HEIGHT);
        mCompositorViewHolder.updateWebContentsSize(mTab);
        verify(mWebContents, atLeast(1)).setSize(VIEWPORT_WIDTH, VIEWPORT_HEIGHT - TOOLBAR_HEIGHT);
    }

    @Test
    public void
            testWebContentResizeTriggeredDueToKeyboardTransition_hasNoIntermediateInsetOvershoot() {
        mCompositorViewHolder.updateVirtualKeyboardMode(VirtualKeyboardMode.OVERLAYS_CONTENT);
        reset(mWebContents);

        int intermediateViewportHeight = VIEWPORT_HEIGHT - 16;

        mCompositorViewHolder.setWidthForTesting(VIEWPORT_WIDTH);

        // Establish baseline before keyboard transition starts.
        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(false);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(0);
        mCompositorViewHolder.setHeightForTesting(VIEWPORT_HEIGHT);
        mCompositorViewHolder.updateWebContentsSize(mTab);
        reset(mWebContents);

        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(true);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(KEYBOARD_HEIGHT);

        // Small keyboard inset step arrives before the view height update.
        mCompositorViewHolder.setHeightForTesting(VIEWPORT_HEIGHT);
        mKeyboardInsetSupplier.set(16);
        mCompositorViewHolder.updateWebContentsSize(mTab);

        // Next inset step arrives after the view has partially resized but before the previous
        // viewport-height update was observed by updateWebContentsSize().
        mCompositorViewHolder.setHeightForTesting(intermediateViewportHeight);
        mKeyboardInsetSupplier.set(32);
        mCompositorViewHolder.updateWebContentsSize(mTab);

        ArgumentCaptor<Integer> resizedHeightCaptor = ArgumentCaptor.forClass(Integer.class);
        verify(mWebContents, atLeast(1)).setSize(eq(VIEWPORT_WIDTH), resizedHeightCaptor.capture());
        for (int observedHeight : resizedHeightCaptor.getAllValues()) {
            Assert.assertTrue(
                    "Unexpected transient overshoot: " + observedHeight + " > " + VIEWPORT_HEIGHT,
                    observedHeight <= VIEWPORT_HEIGHT);
        }
    }

    @Test
    public void testWebContentResizeTriggeredDueToKeyboardDismiss_hasNoTransientUndershoot() {
        mCompositorViewHolder.updateVirtualKeyboardMode(VirtualKeyboardMode.OVERLAYS_CONTENT);
        reset(mWebContents);

        int fullViewportHeight = 785;
        int intermediateViewportHeight = fullViewportHeight - 24;

        mCompositorViewHolder.setWidthForTesting(VIEWPORT_WIDTH);

        // Establish baseline before keyboard transition.
        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(false);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(0);
        mCompositorViewHolder.setHeightForTesting(fullViewportHeight);
        mCompositorViewHolder.updateWebContentsSize(mTab);
        reset(mWebContents);

        // Layout race during dismiss: view height shrinks while the inset supplier still reads 0
        // and the IME animation is in progress.
        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(true);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(24);
        mCompositorViewHolder.setHeightForTesting(intermediateViewportHeight);
        mCompositorViewHolder.updateWebContentsSize(mTab);

        ArgumentCaptor<Integer> resizedHeightCaptor = ArgumentCaptor.forClass(Integer.class);
        verify(mWebContents, atLeast(1)).setSize(eq(VIEWPORT_WIDTH), resizedHeightCaptor.capture());
        for (int observedHeight : resizedHeightCaptor.getAllValues()) {
            Assert.assertTrue(
                    "Unexpected transient undershoot: "
                            + observedHeight
                            + " < "
                            + fullViewportHeight,
                    observedHeight >= fullViewportHeight);
        }
    }

    @Test
    @DisableFeatures(ChromeFeatureList.VIRTUAL_KEYBOARD_TRANSIENT_INNER_HEIGHT_FIX)
    public void
            testWebContentResizeTriggeredDueToKeyboardTransition_withKillSwitch_usesLegacySizing() {
        mCompositorViewHolder.updateVirtualKeyboardMode(VirtualKeyboardMode.OVERLAYS_CONTENT);
        reset(mWebContents);

        int adjustedHeight = VIEWPORT_HEIGHT - KEYBOARD_HEIGHT;

        mCompositorViewHolder.setWidthForTesting(VIEWPORT_WIDTH);

        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(true);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(KEYBOARD_HEIGHT);
        mCompositorViewHolder.setHeightForTesting(VIEWPORT_HEIGHT);
        mKeyboardInsetSupplier.set(KEYBOARD_HEIGHT);
        mCompositorViewHolder.updateWebContentsSize(mTab);

        mCompositorViewHolder.setHeightForTesting(adjustedHeight);
        mCompositorViewHolder.updateWebContentsSize(mTab);

        verify(mWebContents, atLeast(1)).setSize(VIEWPORT_WIDTH, VIEWPORT_HEIGHT + KEYBOARD_HEIGHT);
    }

    @Test
    @DisableFeatures(BaseFeatures.VIRTUAL_KEYBOARD_GEOMETRY_AND_INSET_FIXES)
    public void testWebContentResizeTriggeredDueToKeyboardShow_withKillSwitch_usesLegacyOrigin() {
        mCompositorViewHolder.updateVirtualKeyboardMode(VirtualKeyboardMode.OVERLAYS_CONTENT);
        reset(mWebContents);

        int adjustedHeight = VIEWPORT_HEIGHT - KEYBOARD_HEIGHT;

        mCompositorViewHolder.mWindowVisibleDisplayFrame =
                new Rect(17, 23, 17 + VIEWPORT_WIDTH, 23 + VIEWPORT_HEIGHT);

        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(true);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(KEYBOARD_HEIGHT);
        mCompositorViewHolder.setWidthForTesting(VIEWPORT_WIDTH);
        mCompositorViewHolder.setHeightForTesting(adjustedHeight);

        mKeyboardInsetSupplier.set(KEYBOARD_HEIGHT);
        mCompositorViewHolder.updateWebContentsSize(mTab);

        verify(mCompositorViewJni, times(1))
                .notifyVirtualKeyboardOverlayRect(
                        1L, mWebContents, 17, 23, VIEWPORT_WIDTH, KEYBOARD_HEIGHT);
    }

    // Keyboard resize tests for geometrychange event fired to JS.
    @Test
    public void testResizeNotificationsDueToKeyboardShow_OSKeyboardOverlay() {
        mCompositorViewHolder.updateVirtualKeyboardMode(VirtualKeyboardMode.OVERLAYS_CONTENT);
        reset(mWebContents);

        // adjustedHeight is the height of the CompositorViewHolder from Android View layout
        // after showing the keyboard. This simulates a reduced layout height from the keyboard
        // taking up the bottom space.
        int adjustedHeight = VIEWPORT_HEIGHT - KEYBOARD_HEIGHT;

        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(true);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(KEYBOARD_HEIGHT);
        mCompositorViewHolder.setWidthForTesting(VIEWPORT_WIDTH);
        // The CompositorViewHolder does not account for the keyboard since the keyboard inset has
        // been consumed by an inset consumer, which deliberately did not update the CVH.
        mCompositorViewHolder.setHeightForTesting(VIEWPORT_HEIGHT);
        when(mInsetObserver.isKeyboardInOverlayMode()).thenReturn(true);

        // This triggers handleWindowInsetChanged automatically.
        mKeyboardInsetSupplier.set(KEYBOARD_HEIGHT);

        // The webcontents weren't resized, so no `setSize` should come through.
        verify(mWebContents, never()).setSize(anyInt(), anyInt());
        verify(mCompositorViewJni, times(1))
                .notifyVirtualKeyboardOverlayRect(
                        1L, mWebContents, 0, adjustedHeight, VIEWPORT_WIDTH, KEYBOARD_HEIGHT);

        reset(mWebContents);
        clearInvocations(mCompositorViewJni);

        // Hide the keyboard.
        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(false);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(0);
        mCompositorViewHolder.setWidthForTesting(VIEWPORT_WIDTH);
        mCompositorViewHolder.setHeightForTesting(VIEWPORT_HEIGHT);
        when(mInsetObserver.isKeyboardInOverlayMode()).thenReturn(true);
        // This triggers handleWindowInsetChanged automatically.
        mKeyboardInsetSupplier.set(0);

        verify(mWebContents, never()).setSize(anyInt(), anyInt());
        verify(mCompositorViewJni, times(1))
                .notifyVirtualKeyboardOverlayRect(1L, mWebContents, 0, 0, 0, 0);
    }

    @Test
    public void testOverlayGeometryNotTriggeredDueToNoKeyboard() {
        mCompositorViewHolder.updateVirtualKeyboardMode(VirtualKeyboardMode.OVERLAYS_CONTENT);
        reset(mWebContents);

        // Simulate the keyboard being hidden
        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(false);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(0);
        mCompositorViewHolder.setWidthForTesting(VIEWPORT_WIDTH);
        mCompositorViewHolder.setHeightForTesting(VIEWPORT_HEIGHT);
        mKeyboardInsetSupplier.set(0);

        // Ensure updating the WebContents size doesn't dispatch a keyboard geometry event to
        // web content. The updateWebContentsSize call simulates the Views layout that happens as a
        // result of the keyboard showing, which happens after the inset is set.
        mCompositorViewHolder.updateWebContentsSize(mTab);
        verify(mWebContents, times(1)).setSize(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
        verify(mCompositorViewJni, times(0))
                .notifyVirtualKeyboardOverlayRect(1L, mWebContents, 0, 0, 0, 0);
    }

    @Test
    public void testWebContentResizeWhenInOSKResizesVisualMode() {
        mCompositorViewHolder.updateVirtualKeyboardMode(VirtualKeyboardMode.RESIZES_VISUAL);
        reset(mWebContents);

        // adjustedHeight is height of the CompositorViewHolder from Android View layout. This
        // simulates a reduced layout height from the keyboard taking up the bottom space.
        int adjustedHeight = VIEWPORT_HEIGHT - KEYBOARD_HEIGHT;

        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(true);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(KEYBOARD_HEIGHT);
        mKeyboardInsetSupplier.set(KEYBOARD_HEIGHT);
        mCompositorViewHolder.setWidthForTesting(VIEWPORT_WIDTH);
        mCompositorViewHolder.setHeightForTesting(adjustedHeight);

        mCompositorViewHolder.updateWebContentsSize(mTab);

        // In RESIZES_VISUAL mode, CompositorViewHolder ensures that size changes from the virtual
        // keyboard don't affect the WebContents' size.
        verify(mWebContents, times(1)).setSize(VIEWPORT_WIDTH, VIEWPORT_HEIGHT);
        verify(mCompositorViewJni, times(0))
                .notifyVirtualKeyboardOverlayRect(1L, mWebContents, 0, 0, 0, 0);
    }

    @Test
    public void testWebContentResizeWhenInOSKResizesContentMode() {
        mCompositorViewHolder.updateVirtualKeyboardMode(VirtualKeyboardMode.RESIZES_CONTENT);
        reset(mWebContents);

        // adjustedHeight is height of the CompositorViewHolder from Android View layout. This
        // simulates a reduced layout height from the keyboard taking up the bottom space.
        int adjustedHeight = VIEWPORT_HEIGHT - KEYBOARD_HEIGHT;

        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(true);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(KEYBOARD_HEIGHT);
        mCompositorViewHolder.setWidthForTesting(VIEWPORT_WIDTH);
        mCompositorViewHolder.setHeightForTesting(adjustedHeight);
        mKeyboardInsetSupplier.set(KEYBOARD_HEIGHT);

        // In RESIZES_CONTENT mode, CompositorViewHolder resizes the WebContents by the keyboard
        // height.
        verify(mWebContents, times(1)).setSize(VIEWPORT_WIDTH, adjustedHeight - TOOLBAR_HEIGHT);
        verify(mCompositorViewJni, times(0))
                .notifyVirtualKeyboardOverlayRect(1L, mWebContents, 0, 0, 0, 0);
    }

    @Test
    public void testWebContentResizeByBottomSheetInset() {
        SettableMonotonicObservableSupplier<Integer> bottomSheetInsetSupplier =
                ObservableSuppliers.createMonotonic();
        mViewportInsets.setBottomSheetInsetSupplier(bottomSheetInsetSupplier);
        reset(mWebContents);

        int bottomSheetOffset = 420;

        mCompositorViewHolder.setWidthForTesting(VIEWPORT_WIDTH);
        mCompositorViewHolder.setHeightForTesting(VIEWPORT_HEIGHT);
        bottomSheetInsetSupplier.set(bottomSheetOffset);

        // adjustedHeight is height of the CompositorViewHolder from Android View layout. This
        // simulates a reduced layout height from bottom sheet taking up the space at the bottom.
        int adjustedHeight = VIEWPORT_HEIGHT - bottomSheetOffset;
        verify(mWebContents, times(1)).setSize(VIEWPORT_WIDTH, adjustedHeight - TOOLBAR_HEIGHT);
    }

    @Test
    public void testOverlayGeometryWhenViewNotAttachedToWindow() {
        mCompositorViewHolder.updateVirtualKeyboardMode(VirtualKeyboardMode.OVERLAYS_CONTENT);
        reset(mWebContents);

        UiUtils.removeViewFromParent(mContentView);

        // adjustedHeight is height of the CompositorViewHolder from Android View layout. This
        // simulates a reduced layout height from the keyboard taking up the bottom space.
        int adjustedHeight = VIEWPORT_HEIGHT - KEYBOARD_HEIGHT;

        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(true);
        when(mMockKeyboard.calculateTotalKeyboardHeight(any())).thenReturn(KEYBOARD_HEIGHT);
        mKeyboardInsetSupplier.set(KEYBOARD_HEIGHT);
        mCompositorViewHolder.setWidthForTesting(VIEWPORT_WIDTH);
        mCompositorViewHolder.setHeightForTesting(adjustedHeight);

        // Ensure updateWebContentsSize in OVERLAYS_CONTENT mode doesn't send keyboard geometry
        // events to content if the view is detached.
        mCompositorViewHolder.updateWebContentsSize(mTab);
        verify(mCompositorViewJni, times(0))
                .notifyVirtualKeyboardOverlayRect(1L, mWebContents, 0, 0, 0, 0);
    }

    @Test
    public void testAccessoryInsetsResizeWebContents() {
        int viewportHeight = 800;
        int viewportWidth = 300;
        int accessoryHeight = 500;

        mCompositorViewHolder.setWidthForTesting(viewportWidth);
        mCompositorViewHolder.setHeightForTesting(viewportHeight);

        // This is only relevant for RESIZES_CONTENT mode since in RESIZES_VISUAL or
        // OVERLAYS_CONTENT the WebContents does not need to be resized by keyboard-related insets.
        mCompositorViewHolder.updateVirtualKeyboardMode(VirtualKeyboardMode.RESIZES_CONTENT);

        // Updating the VirtualKeyboardMode will update the viewport size. The test is setup so the
        // browser controls are showing so they'll be subtracted from the viewport height.
        verify(mWebContents, times(1)).setSize(viewportWidth, viewportHeight - TOOLBAR_HEIGHT);

        reset(mWebContents);

        // Simulate showing a keyboard accessory of some kind. This should cause the WebContents to
        // be resized without any other action.
        mKeyboardAccessoryInsetSupplier.set(accessoryHeight);

        verify(mWebContents, times(1))
                .setSize(viewportWidth, viewportHeight - accessoryHeight - TOOLBAR_HEIGHT);
    }

    @Test
    public void testInMotionSupplier_OnTouch() {
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        mCompositorViewHolder.onNativeLibraryReady(
                mWindowAndroid, /* tabContentManager= */ null, mPrefService);
        mCompositorViewHolder.onContentChanged();
        verify(mTab, atLeast(1)).addObserver(mTabObserverCaptor.capture());

        mTabObserverCaptor.getAllValues().forEach((obs) -> obs.onTouchDown());
        Assert.assertTrue(mCompositorViewHolder.getInMotionSupplier().get());

        mTabObserverCaptor.getAllValues().forEach((obs) -> obs.onTouchUp());
        Assert.assertFalse(mCompositorViewHolder.getInMotionSupplier().get());
    }

    @Test
    public void testOnInterceptHoverEvent() {
        when(mMockKeyboard.isKeyboardShowing(any())).thenReturn(false);
        when(mLayoutManager.onInterceptMotionEvent(
                        MOTION_ACTION_HOVER_ENTER, false, EventType.HOVER))
                .thenReturn(true);
        boolean intercepted =
                mCompositorViewHolder.onInterceptHoverEvent(MOTION_ACTION_HOVER_ENTER);
        verify(mLayoutManager)
                .onInterceptMotionEvent(MOTION_ACTION_HOVER_ENTER, false, EventType.HOVER);
        Assert.assertTrue(
                "#onInterceptHoverEvent should return true if the LayoutManager intercepts the"
                        + " event.",
                intercepted);
    }

    @Test
    public void testOnHoverEvent() {
        when(mLayoutManager.onHoverEvent(MOTION_ACTION_HOVER_ENTER)).thenReturn(true);
        boolean consumed = mCompositorViewHolder.onHoverEvent(MOTION_ACTION_HOVER_ENTER);
        verify(mLayoutManager).onHoverEvent(MOTION_ACTION_HOVER_ENTER);
        Assert.assertTrue(
                "#onHoverEvent should return true if the LayoutManager consumes the event.",
                consumed);
    }

    @Test
    public void testDispatchGenericMotionEvent() {
        when(mLayoutManager.dispatchGenericMotionEvent(MOTION_ACTION_BUTTON_RELEASE_MOUSE))
                .thenReturn(true);
        boolean consumed =
                mCompositorViewHolder.dispatchGenericMotionEvent(
                        MOTION_ACTION_BUTTON_RELEASE_MOUSE);
        verify(mLayoutManager).dispatchGenericMotionEvent(MOTION_ACTION_BUTTON_RELEASE_MOUSE);
        Assert.assertTrue(
                "#dispatchGenericMotionEvent should return true if the LayoutManager consumes the"
                        + " event.",
                consumed);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testSetBackgroundRunnable() {
        // Trigger a compositor layout. Verify the background has not yet been removed.
        mCompositorViewHolder.onCompositorLayout();
        verifyBackgroundNotRemoved();

        // Mark that a frame has swapped, but the buffer has not yet swapped. Verify the background
        // has not yet been removed.
        int pendingFrameCount = 0;
        mCompositorViewHolder.didSwapFrame(pendingFrameCount);
        verifyBackgroundNotRemoved();

        // Mark that the buffer has swapped. Verify the background has now been removed
        boolean swappedCurrentSize = true;
        int framesUntilHideBackground = 1;
        mCompositorViewHolder.didSwapBuffers(swappedCurrentSize, framesUntilHideBackground);
        verifyBackgroundRemoved();
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testSetBackgroundRunnable_Timeout() {
        // Run delayed tasks (timing out the background runnable), then verify the background has
        // been removed.
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verifyBackgroundRemoved();
    }

    @Test
    public void testPrepareForTabReparenting_AfterTimeout_RemovesBackgroundOnFirstSwap() {
        // The activity is stopped before its first draw, so the initial removal times out.
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertNull(mCompositorView.getBackground());

        // Restarting with a tab to reparent sets the white background again, which must be
        // removed on the first swap.
        mCompositorViewHolder.prepareForTabReparenting();
        assertEquals(Color.WHITE, ((ColorDrawable) mCompositorView.getBackground()).getColor());
        verifyBackgroundNotRemoved();

        mCompositorViewHolder.onCompositorLayout();
        mCompositorViewHolder.didSwapFrame(/* pendingFrameCount= */ 0);
        verifyBackgroundNotRemoved();

        mCompositorViewHolder.didSwapBuffers(
                /* swappedCurrentSize= */ true, /* framesUntilHideBackground= */ 0);
        verifyBackgroundRemoved();
    }

    @Test
    public void testPrepareForTabReparenting_AfterTimeout_RemovesBackgroundOnTimeout() {
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        assertNull(mCompositorView.getBackground());

        mCompositorViewHolder.prepareForTabReparenting();
        assertEquals(Color.WHITE, ((ColorDrawable) mCompositorView.getBackground()).getColor());

        // With no frame swapped, the fallback timeout must still remove the background.
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();
        verifyBackgroundRemoved();
    }

    @Test
    public void testFocusOnWebContent_resetsKeyboardFocus() {
        SceneOverlay sceneOverlay = mock(SceneOverlay.class);
        VirtualView virtualView = mock(VirtualView.class);
        doAnswer(
                        invocation -> {
                            List<VirtualView> views = invocation.getArgument(0);
                            views.add(virtualView);
                            return null;
                        })
                .when(sceneOverlay)
                .getVirtualViews(anyList());
        mCompositorViewHolder.requestKeyboardFocus(sceneOverlay);
        verify(virtualView).setKeyboardFocused(true);

        mCompositorViewHolder.setFocusOnFirstContentViewItem();
        verify(virtualView).setKeyboardFocused(false);
        assertFalse(mCompositorViewHolder.containsKeyboardFocus(sceneOverlay));
    }

    @Test
    public void testOnControlsOffsetChanged_NoRequestRenderIfScrolling() {
        mCompositorViewHolder.dispatchTouchEvent(MOTION_EVENT_DOWN);
        mCompositorViewHolder.onControlsOffsetChanged(0, 0, false, 0, 0, false, true, false);
        verify(mCompositorViewJni, never()).setNeedsComposite(anyLong());
        mCompositorViewHolder.dispatchTouchEvent(MOTION_EVENT_UP);

        mCompositorViewHolder.setContentViewScrollingStateForTesting(true);
        mCompositorViewHolder.onControlsOffsetChanged(0, 0, false, 0, 0, false, true, false);
        verify(mCompositorViewJni, never()).setNeedsComposite(anyLong());
        mCompositorViewHolder.setContentViewScrollingStateForTesting(false);
    }

    @Test
    public void testOnControlsOffsetChanged_RequestRender() {
        mCompositorViewHolder.onControlsOffsetChanged(0, 0, false, 0, 0, false, true, false);
        verify(mCompositorViewJni, times(1)).setNeedsComposite(anyLong());
    }

    @Test
    public void testActiveTouchInterceptors() {
        TouchEventObserver observer = mock(TouchEventObserver.class);
        when(observer.mayInterceptTouchSequenceInWebContents()).thenReturn(true);

        mCompositorViewHolder.addTouchEventObserver(observer);
        assertEquals(true, mCompositorView.mLastActiveTouchInterceptors);
        mCompositorView.mLastActiveTouchInterceptors = null;

        mCompositorViewHolder.removeTouchEventObserver(observer);
        assertEquals(false, mCompositorView.mLastActiveTouchInterceptors);
    }

    @Test
    public void testAsymmetricTouchInterceptors() {
        TouchEventObserver observer = mock(TouchEventObserver.class);
        when(observer.mayInterceptTouchSequenceInWebContents()).thenReturn(true);

        mCompositorViewHolder.addTouchEventObserver(null);
        assertNull(mCompositorView.mLastActiveTouchInterceptors);

        mCompositorViewHolder.addTouchEventObserver(observer);
        assertEquals(true, mCompositorView.mLastActiveTouchInterceptors);
        mCompositorView.mLastActiveTouchInterceptors = null;

        mCompositorViewHolder.addTouchEventObserver(observer);
        assertNull(mCompositorView.mLastActiveTouchInterceptors);

        mCompositorViewHolder.removeTouchEventObserver(null);
        assertNull(mCompositorView.mLastActiveTouchInterceptors);

        mCompositorViewHolder.removeTouchEventObserver(observer);
        assertEquals(false, mCompositorView.mLastActiveTouchInterceptors);
        mCompositorView.mLastActiveTouchInterceptors = null;

        mCompositorViewHolder.removeTouchEventObserver(observer);
        assertNull(mCompositorView.mLastActiveTouchInterceptors);
    }

    @Test
    public void testMultipleActiveTouchInterceptors() {
        TouchEventObserver observer1 = mock(TouchEventObserver.class);
        when(observer1.mayInterceptTouchSequenceInWebContents()).thenReturn(true);
        TouchEventObserver observer2 = mock(TouchEventObserver.class);
        when(observer2.mayInterceptTouchSequenceInWebContents()).thenReturn(true);
        TouchEventObserver observer3 = mock(TouchEventObserver.class);
        when(observer3.mayInterceptTouchSequenceInWebContents()).thenReturn(false);

        mCompositorViewHolder.addTouchEventObserver(observer1);
        assertEquals(true, mCompositorView.mLastActiveTouchInterceptors);
        mCompositorView.mLastActiveTouchInterceptors = null;

        mCompositorViewHolder.addTouchEventObserver(observer2);
        assertNull(mCompositorView.mLastActiveTouchInterceptors);

        mCompositorViewHolder.addTouchEventObserver(observer3);
        assertNull(mCompositorView.mLastActiveTouchInterceptors);

        mCompositorViewHolder.removeTouchEventObserver(observer3);
        assertNull(mCompositorView.mLastActiveTouchInterceptors);

        mCompositorViewHolder.removeTouchEventObserver(observer2);
        assertNull(mCompositorView.mLastActiveTouchInterceptors);

        mCompositorViewHolder.removeTouchEventObserver(observer1);
        assertEquals(false, mCompositorView.mLastActiveTouchInterceptors);
    }

    private void runCurrentTasks() {
        RobolectricUtil.runAllBackgroundAndUi();
    }

    private void verifyBackgroundNotRemoved() {
        runCurrentTasks();
        assertNotNull(mCompositorView.getBackground());
    }

    private void verifyBackgroundRemoved() {
        runCurrentTasks();
        assertNull(mCompositorView.getBackground());
    }

    @Test
    public void testAccessibilityNode_boundsAreCorrect() {
        mContext.getResources().getDisplayMetrics().density = 1.375f;

        var virtualView = mock(VirtualView.class);
        // Values in this test case are real numbers captured from clank running
        // in a maximized window.
        RectF dpRect = new RectF(100.36364f, 2.18182f, 337.36365f, 42.18182f);
        doAnswer(
                        invocation -> {
                            ((RectF) invocation.getArgument(0)).set(dpRect);
                            return null;
                        })
                .when(virtualView)
                .getTouchTarget(any(RectF.class));
        when(virtualView.getAccessibilityDescription()).thenReturn("test-node");
        doAnswer(
                        invocation -> {
                            List<VirtualView> list = invocation.getArgument(0);
                            list.add(virtualView);
                            return null;
                        })
                .when(mLayoutManager)
                .getVirtualViews(anyList());

        mCompositorViewHolder.onAccessibilityModeChanged(true);
        assertNotNull(mCompositorViewHolder.mAccessibilityView);

        mCompositorViewHolder.mAccessibilityView.createAccessibilityNodeInfo();
        var accessibilityProvider =
                mCompositorViewHolder.mAccessibilityView.getAccessibilityNodeProvider();
        assertNotNull(accessibilityProvider);
        var nodeInfo = accessibilityProvider.createAccessibilityNodeInfo(0);
        Rect actualRect = new Rect();
        nodeInfo.getBoundsInScreen(actualRect);
        // Top coordinate: 2.18182 * 1.375 = 3.0000025. Should be floored or
        // rounded to 3.
        Rect expectedRect = new Rect(138, 3, 464, 59);
        assertEquals(expectedRect, actualRect);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL)
    public void testSetSideUiStateProviderSupplier() {
        mSideUiStateProviderSupplier.set(mSideUiStateProvider);
        runCurrentTasks();

        verify(mSideUiStateProvider).addObserver(mCompositorViewHolder);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL)
    public void testOnSideUiSpecsChangeCompleted_updateWebContentsSize() {
        // Setup.
        reset(mWebContents);

        mCompositorViewHolder.setWidthForTesting(VIEWPORT_WIDTH);
        mCompositorViewHolder.setHeightForTesting(VIEWPORT_HEIGHT);

        // Arbitrary Side UI width.
        int startContainerWidth = 100;
        int endContainerWidth = 200;
        SideUiSpecs currentSideUiSpecs = new SideUiSpecs(startContainerWidth, endContainerWidth);
        when(mSideUiStateProvider.getExpectedSideUiSpecsForTab(mTab))
                .thenReturn(currentSideUiSpecs);

        // Act: Make SideUiStateProvider available.
        //
        // Note that updateForSideUiSpecs() is expected to be called as soon as SideUiStateProvider
        // is available, so we don't need to explicitly call onSideUiSpecsChangeCompleted() here.
        mSideUiStateProviderSupplier.set(mSideUiStateProvider);
        runCurrentTasks();

        // Verify.
        verify(mWebContents, atLeastOnce())
                .setSize(
                        VIEWPORT_WIDTH - (startContainerWidth + endContainerWidth),
                        VIEWPORT_HEIGHT);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL)
    @EnableFeatures(ChromeFeatureList.ANDROID_VERTICAL_TABS)
    @Config(qualifiers = "sw600dp")
    public void testOnSideUiSpecsChangeCompleted_updateWebContentsSize_verticalTabs() {
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        mCompositorViewHolder.onNativeLibraryReady(
                mWindowAndroid, /* tabContentManager= */ null, mPrefService);
        reset(mWebContents);

        mCompositorViewHolder.setWidthForTesting(VIEWPORT_WIDTH);
        mCompositorViewHolder.setHeightForTesting(VIEWPORT_HEIGHT);

        int startContainerWidth = 100;
        int endContainerWidth = 200;
        SideUiSpecs currentSideUiSpecs = new SideUiSpecs(startContainerWidth, endContainerWidth);
        when(mSideUiStateProvider.getExpectedSideUiSpecsForTab(mTab))
                .thenReturn(currentSideUiSpecs);

        // Make SideUiStateProvider available.
        mSideUiStateProviderSupplier.set(mSideUiStateProvider);
        runCurrentTasks();

        // Verify that web contents width is updated when vertical tabs is enabled on tablet.
        verify(mWebContents, atLeastOnce())
                .setSize(
                        VIEWPORT_WIDTH - (startContainerWidth + endContainerWidth),
                        VIEWPORT_HEIGHT);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL)
    public void testOnSideUiSpecsChangeCompleted_updateContentOffsetX() {
        doTestOnSideUiSpecsChangeCompleted_updateContentOffsetX(/* shouldBeRtl= */ false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL)
    public void testOnSideUiSpecsChangeCompleted_updateContentOffsetX_rtl() {
        doTestOnSideUiSpecsChangeCompleted_updateContentOffsetX(/* shouldBeRtl= */ true);
    }

    private void doTestOnSideUiSpecsChangeCompleted_updateContentOffsetX(boolean shouldBeRtl) {
        // Setup.
        LocalizationUtils.setRtlForTesting(shouldBeRtl);
        reset(mWebContents);
        ViewAndroidDelegate viewDelegate = ViewAndroidDelegate.createBasicDelegate(mContentView);
        when(mWebContents.getViewAndroidDelegate()).thenReturn(viewDelegate);

        // Arbitrary Side UI width.
        int leftContainerWidth = 50;
        int rightContainerWidth = 150;
        SideUiSpecs currentSideUiSpecs = new SideUiSpecs(leftContainerWidth, rightContainerWidth);
        when(mSideUiStateProvider.getCurrentSideUiSpecs()).thenReturn(currentSideUiSpecs);
        when(mSideUiStateProvider.getExpectedSideUiSpecsForTab(mTab))
                .thenReturn(currentSideUiSpecs);

        // Act: Make SideUiStateProvider available.
        //
        // Note that updateForSideUiSpecs() is expected to be called as soon as SideUiStateProvider
        // is available, so we don't need to explicitly call onSideUiSpecsChangeCompleted() here.
        mSideUiStateProviderSupplier.set(mSideUiStateProvider);
        runCurrentTasks();

        // Verify that RTL does not affect the offset (i.e. always contentOffsetx == left)
        int expectedContentOffsetX = leftContainerWidth;
        verify(mLayoutManager).setContentOffsetX(expectedContentOffsetX);
        assertEquals(expectedContentOffsetX, mContentView.getContentOffsetXPix());
    }

    @Test
    public void testOnSideUiSpecsChangeCompleted_manualResize() {
        mCompositorViewHolder.setWidthForTesting(VIEWPORT_WIDTH);
        mCompositorViewHolder.setHeightForTesting(VIEWPORT_HEIGHT);
        mSideUiStateProviderSupplier.set(mSideUiStateProvider);
        runCurrentTasks();
        reset(mWebContents);
        when(mWebContents.getViewAndroidDelegate())
                .thenReturn(ViewAndroidDelegate.createBasicDelegate(mContentView));

        SideUiSpecs specs = new SideUiSpecs(SIDE_UI_START_WIDTH, /* rightContainerWidth= */ 0);
        when(mSideUiStateProvider.getExpectedSideUiSpecsForTab(mTab)).thenReturn(specs);
        mCompositorViewHolder.onSideUiSpecsChangeCompleted(
                specs,
                UiUpdateRequest.getRequestForTesting(
                        /* suppressAnimations= */ true, UpdateReason.RESIZE_LIVE));
        runCurrentTasks();

        verify(mLayoutManager).setContentOffsetX(SIDE_UI_START_WIDTH);
        assertEquals(SIDE_UI_START_WIDTH, mContentView.getContentOffsetXPix());
        verify(mWebContents, never()).setSize(anyInt(), anyInt());

        mCompositorViewHolder.onSideUiSpecsChangeCompleted(
                specs,
                UiUpdateRequest.getRequestForTesting(
                        /* suppressAnimations= */ true, UpdateReason.RESIZE_COMMITTED));
        runCurrentTasks();

        verify(mWebContents).setSize(VIEWPORT_WIDTH - SIDE_UI_START_WIDTH, VIEWPORT_HEIGHT);
    }

    @Test
    public void testOnSideUiSpecsChangeCompleted_contentViewMarginsNotUpdated() {
        // Setup content view.
        mContentView.setLayoutParams(new FrameLayout.LayoutParams(0, 0));

        doTestSideUiSpecsChangeCompleted_updateMargins(
                /* expectedStartMargin= */ 0, /* expectedEndMargin= */ 0);
    }

    @Test
    public void testOnSideUiSpecsChangeCompleted_customViewMarginsUpdated() {
        // Setup custom view.
        View customView = new View(mContext);
        when(mTab.isShowingCustomView()).thenReturn(true);
        when(mTab.getView()).thenReturn(customView);

        doTestSideUiSpecsChangeCompleted_updateMargins(SIDE_UI_START_WIDTH, SIDE_UI_END_WIDTH);
    }

    private void doTestSideUiSpecsChangeCompleted_updateMargins(
            int expectedStartMargin, int expectedEndMargin) {
        // Notify content changed.
        reset(mWebContents);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        mCompositorViewHolder.onNativeLibraryReady(
                mWindowAndroid, /* tabContentManager= */ null, mPrefService);
        mCompositorViewHolder.onContentChanged();

        // Arbitrary Side UI width.
        SideUiSpecs currentSideUiSpecs = new SideUiSpecs(SIDE_UI_START_WIDTH, SIDE_UI_END_WIDTH);
        when(mSideUiStateProvider.getCurrentSideUiSpecs()).thenReturn(currentSideUiSpecs);
        when(mSideUiStateProvider.getExpectedSideUiSpecsForTab(mTab))
                .thenReturn(currentSideUiSpecs);
        mSideUiStateProviderSupplier.set(mSideUiStateProvider);
        runCurrentTasks();
        mCompositorViewHolder.onSideUiSpecsChangeCompleted(
                currentSideUiSpecs,
                UiUpdateRequest.getRequestForTesting(/* suppressAnimations= */ true));

        // Verify layout params.
        MarginLayoutParams layoutParams = (MarginLayoutParams) mTab.getView().getLayoutParams();
        assertEquals(
                "Unexpected start margin.", expectedStartMargin, layoutParams.getMarginStart());
        assertEquals("Unexpected end margin.", expectedEndMargin, layoutParams.getMarginEnd());
    }

    @Test
    public void
            testOnSideUiSpecsChangeCompleted_liveResize_customViewTranslatedWithoutMarginUpdate() {
        View customView = setUpCustomViewWithCommittedSideUiSpecs();

        int liveStartWidth = SIDE_UI_START_WIDTH + 50;
        mCompositorViewHolder.onSideUiSpecsChangeCompleted(
                new SideUiSpecs(liveStartWidth, SIDE_UI_END_WIDTH),
                UiUpdateRequest.getRequestForTesting(
                        /* suppressAnimations= */ true, UpdateReason.RESIZE_LIVE));

        // The view follows the rail by translation only, keeping its committed margins and width.
        assertEquals(
                "Unexpected translation.",
                liveStartWidth - SIDE_UI_START_WIDTH,
                customView.getTranslationX(),
                /* delta= */ 0f);
        MarginLayoutParams layoutParams = (MarginLayoutParams) customView.getLayoutParams();
        assertEquals("Unexpected left margin.", SIDE_UI_START_WIDTH, layoutParams.leftMargin);
        assertEquals("Unexpected right margin.", SIDE_UI_END_WIDTH, layoutParams.rightMargin);
    }

    @Test
    public void testOnSideUiSpecsChangeCompleted_resizeCommitted_customViewTranslationReset() {
        View customView = setUpCustomViewWithCommittedSideUiSpecs();

        int newStartWidth = SIDE_UI_START_WIDTH + 50;
        SideUiSpecs newSpecs = new SideUiSpecs(newStartWidth, SIDE_UI_END_WIDTH);
        mCompositorViewHolder.onSideUiSpecsChangeCompleted(
                newSpecs,
                UiUpdateRequest.getRequestForTesting(
                        /* suppressAnimations= */ true, UpdateReason.RESIZE_LIVE));
        mCompositorViewHolder.onSideUiSpecsChangeCompleted(
                newSpecs,
                UiUpdateRequest.getRequestForTesting(
                        /* suppressAnimations= */ true, UpdateReason.RESIZE_COMMITTED));

        // Committing drops the translation and applies the new margins.
        assertEquals("Unexpected translation.", 0f, customView.getTranslationX(), /* delta= */ 0f);
        MarginLayoutParams layoutParams = (MarginLayoutParams) customView.getLayoutParams();
        assertEquals("Unexpected left margin.", newStartWidth, layoutParams.leftMargin);
        assertEquals("Unexpected right margin.", SIDE_UI_END_WIDTH, layoutParams.rightMargin);
    }

    @Test
    public void testOnContentChanged_customViewMarginsUpdatedUsingExpectedSpecs() {
        // Setup custom view.
        View customView = new View(mContext);
        when(mTab.isShowingCustomView()).thenReturn(true);
        when(mTab.getView()).thenReturn(customView);

        reset(mWebContents);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        mCompositorViewHolder.onNativeLibraryReady(
                mWindowAndroid, /* tabContentManager= */ null, mPrefService);

        // Make SideUiStateProvider available with expected specs.
        SideUiSpecs expectedSideUiSpecs = new SideUiSpecs(SIDE_UI_START_WIDTH, SIDE_UI_END_WIDTH);
        when(mSideUiStateProvider.getExpectedSideUiSpecsForTab(mTab))
                .thenReturn(expectedSideUiSpecs);
        mSideUiStateProviderSupplier.set(mSideUiStateProvider);
        runCurrentTasks();

        // Trigger onContentChanged while provider is available.
        mCompositorViewHolder.onContentChanged();

        // Verify that getExpectedSideUiSpecsForTab was called for mTab and margins are updated.
        verify(mSideUiStateProvider, atLeastOnce()).getExpectedSideUiSpecsForTab(mTab);
        MarginLayoutParams layoutParams = (MarginLayoutParams) mTab.getView().getLayoutParams();
        assertEquals(
                "Unexpected start margin.", SIDE_UI_START_WIDTH, layoutParams.getMarginStart());
        assertEquals("Unexpected end margin.", SIDE_UI_END_WIDTH, layoutParams.getMarginEnd());
    }

    @Test
    public void testSetSideUiStateProviderSupplier_nullTab_doesNotQueryExpectedSpecs() {
        reset(mWebContents);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        mCompositorViewHolder.onNativeLibraryReady(
                mWindowAndroid, /* tabContentManager= */ null, mPrefService);

        // Set current tab to null.
        mTabModelSelector
                .getModel(false)
                .setIndex(TabModel.INVALID_TAB_INDEX, TabSelectionType.FROM_USER);
        mCompositorViewHolder.mTabVisible = null;

        clearInvocations(mSideUiStateProvider);

        // Make SideUiStateProvider available.
        mSideUiStateProviderSupplier.set(mSideUiStateProvider);
        runCurrentTasks();

        // Verify getExpectedSideUiSpecsForTab was never called.
        verify(mSideUiStateProvider, never()).getExpectedSideUiSpecsForTab(any());
    }

    @Test
    public void testOnContentChanged_nullTab_doesNotQueryExpectedSpecs() {
        reset(mWebContents);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        mCompositorViewHolder.onNativeLibraryReady(
                mWindowAndroid, /* tabContentManager= */ null, mPrefService);

        // Make SideUiStateProvider available.
        mSideUiStateProviderSupplier.set(mSideUiStateProvider);
        runCurrentTasks();

        // Set current tab to null.
        mTabModelSelector
                .getModel(false)
                .setIndex(TabModel.INVALID_TAB_INDEX, TabSelectionType.FROM_USER);
        mCompositorViewHolder.mTabVisible = null;

        // Clear invocations from initial provider availability.
        clearInvocations(mSideUiStateProvider);

        // Trigger onContentChanged with null tab.
        mCompositorViewHolder.onContentChanged();

        // Verify getExpectedSideUiSpecsForTab was never called.
        verify(mSideUiStateProvider, never()).getExpectedSideUiSpecsForTab(any());
    }

    @Test
    public void testGetLastNormalSize() {
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        mCompositorViewHolder.onNativeLibraryReady(
                mWindowAndroid, /* tabContentManager= */ null, mPrefService);

        assertEquals(new Size(0, 0), mCompositorViewHolder.getLastNormalSize());

        mCompositorViewHolder.layout(0, 0, 100, 200);
        mCompositorViewHolder.updateWebContentsSize(mTab);
        assertEquals(new Size(100, 200), mCompositorViewHolder.getLastNormalSize());

        mActivity.enterPictureInPictureMode(new PictureInPictureParams.Builder().build());
        mCompositorViewHolder.layout(0, 0, 50, 50);
        mCompositorViewHolder.updateWebContentsSize(mTab);
        assertEquals(new Size(100, 200), mCompositorViewHolder.getLastNormalSize());

        // ShadowActivity#moveTaskToBack() exits picture-in-picture mode.
        mActivity.moveTaskToBack(/* nonRoot= */ false);
        mCompositorViewHolder.layout(0, 0, 300, 400);
        mCompositorViewHolder.updateWebContentsSize(mTab);
        assertEquals(new Size(300, 400), mCompositorViewHolder.getLastNormalSize());
    }

    @Test
    public void testOnSurfaceResized_BackgroundTabCaptured_SyncsPhysicalSize() {
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        mCompositorViewHolder.onNativeLibraryReady(
                mWindowAndroid, /* tabContentManager= */ null, mPrefService);
        when(mPrefService.getBoolean(any())).thenReturn(false);

        // Active foreground tab
        when(mWebContents.isBeingCaptured()).thenReturn(false);

        // Add a background captured tab
        MockTab bgTab = mTabModelSelector.addMockTab();
        WebContents bgWebContents = mock(WebContents.class);
        when(bgTab.getWebContents()).thenReturn(bgWebContents);
        when(bgWebContents.isBeingCaptured()).thenReturn(true);

        int width = 1080;
        int height = 1920;

        mCompositorViewHolder.onSurfaceResized(width, height);

        // Active foreground tab is updated
        verify(mCompositorViewJni, times(1))
                .onPhysicalBackingSizeChanged(
                        anyLong(), eq(mWebContents), eq(width), eq(height), anyBoolean());
        // Background captured tab is ALSO updated
        verify(mCompositorViewJni, times(1))
                .onPhysicalBackingSizeChanged(
                        anyLong(), eq(bgWebContents), eq(width), eq(height), anyBoolean());
    }

    @Test
    public void testOnSurfaceResized_BackgroundTabNotCaptured_DoesNotSyncPhysicalSize() {
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        mCompositorViewHolder.onNativeLibraryReady(
                mWindowAndroid, /* tabContentManager= */ null, mPrefService);
        when(mPrefService.getBoolean(any())).thenReturn(false);

        // Active foreground tab
        when(mWebContents.isBeingCaptured()).thenReturn(false);

        // Add a background non-captured tab
        MockTab bgTab = mTabModelSelector.addMockTab();
        WebContents bgWebContents = mock(WebContents.class);
        when(bgTab.getWebContents()).thenReturn(bgWebContents);
        when(bgWebContents.isBeingCaptured()).thenReturn(false);

        int width = 1080;
        int height = 1920;

        mCompositorViewHolder.onSurfaceResized(width, height);

        // Active foreground tab is updated
        verify(mCompositorViewJni, times(1))
                .onPhysicalBackingSizeChanged(
                        anyLong(), eq(mWebContents), eq(width), eq(height), anyBoolean());
        // Background non-captured tab is NOT updated
        verify(mCompositorViewJni, never())
                .onPhysicalBackingSizeChanged(
                        anyLong(), eq(bgWebContents), anyInt(), anyInt(), anyBoolean());
    }

    @Test
    public void testKeepScreenOnDeferred_TabSwitchResetsState() throws Exception {
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        mCompositorViewHolder.onNativeLibraryReady(
                mWindowAndroid, /* tabContentManager= */ null, mPrefService);
        mCompositorViewHolder.onContentChanged();

        // Capture the tab observer on the first tab
        verify(mTab, atLeast(1)).addObserver(mTabObserverCaptor.capture());
        TabObserver initialTabObserver = mTabObserverCaptor.getValue();

        // Simulate scroll start on the first tab
        initialTabObserver.onContentViewScrollingStateChanged(true);
        assertTrue(mCompositorViewHolder.getInMotionSupplier().get());

        // 2. Add a new tab and switch to it.
        Tab newTab = mTabModelSelector.addMockTab();
        ContentView newContentView =
                ContentView.createContentView(mActivity, /* webContents= */ null);
        WebContents newWebContents = mock(WebContents.class);
        when(newTab.getWebContents()).thenReturn(newWebContents);
        when(newTab.getContentView()).thenReturn(newContentView);
        when(newTab.getView()).thenReturn(newContentView);

        // Reset the captor before trigger tab switch to capture the new observer
        mTabObserverCaptor = ArgumentCaptor.forClass(TabObserver.class);

        // Trigger tab switch in the model selector.
        mTabModelSelector.getModel(false).setIndex(1, TabSelectionType.FROM_USER);
        mCompositorViewHolder.onContentChanged();

        // Verify that active scroll motion state is cleared immediately upon tab switch.
        assertFalse(mCompositorViewHolder.getInMotionSupplier().get());

        // Capture the observer on the new tab
        verify(newTab, atLeast(1)).addObserver(mTabObserverCaptor.capture());
        TabObserver newTabObserver = mTabObserverCaptor.getValue();

        // Now, the new tab is active.
        // 3. Simulate user interaction on the new tab (touch down).
        newTabObserver.onTouchDown();

        // It should call setDeferKeepScreenOnChanges(true) for the new touch sequence.
        newContentView.setKeepScreenOn(true);
        assertFalse(newContentView.getKeepScreenOn());

        // 4. Simulate user interaction ending on the new tab (touch up).
        newTabObserver.onTouchUp();

        // It should correctly release the deferral now.
        assertTrue(newContentView.getKeepScreenOn());
        assertFalse(mCompositorViewHolder.getInMotionSupplier().get());
    }

    @Test
    public void testKeepScreenOnDeferred_TabHideResetsState() throws Exception {
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        mCompositorViewHolder.onNativeLibraryReady(
                mWindowAndroid, /* tabContentManager= */ null, mPrefService);
        mCompositorViewHolder.onContentChanged();

        // Capture the tab observer on the active tab
        verify(mTab, atLeast(1)).addObserver(mTabObserverCaptor.capture());
        TabObserver tabObserver = mTabObserverCaptor.getValue();

        // 1. Simulate scroll and touch on the active tab
        tabObserver.onContentViewScrollingStateChanged(true);
        tabObserver.onTouchDown();

        assertTrue(mCompositorViewHolder.getInMotionSupplier().get());

        // 2. Call onHidden on the observer.
        tabObserver.onHidden(mTab, org.chromium.chrome.browser.tab.TabHidingType.ACTIVITY_HIDDEN);

        // 3. Verify that the compositor is no longer in motion.
        assertFalse(mCompositorViewHolder.getInMotionSupplier().get());
    }

    @Test
    public void testDetachFocusedOverlayViewReclaimsFocus() {
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        mCompositorViewHolder.onNativeLibraryReady(
                mWindowAndroid, /* tabContentManager= */ null, mPrefService);

        mCompositorViewHolder.overrideTab(null);
        View nativePageView = new View(mContext);
        nativePageView.setFocusable(true);
        nativePageView.setFocusableInTouchMode(true);
        when(mTab.getContentView()).thenReturn(mContentView);
        when(mTab.getView()).thenReturn(nativePageView);
        mCompositorViewHolder.onContentChanged();
        assertEquals(mCompositorViewHolder, nativePageView.getParent());
        assertFalse(mCompositorViewHolder.isFocusable());

        assertTrue(nativePageView.hasFocus());
        clearInvocations(mTab);

        // Simulate NativePage detachment via onContentChanged() when TabImpl.getView() returns
        // null because mIsDetachedFromActivity or mNativePage.isFrozen() is true.
        when(mTab.getView()).thenReturn(null);
        mCompositorViewHolder.onContentChanged();

        assertTrue(mCompositorViewHolder.isFocusable());
        assertTrue(mCompositorViewHolder.hasFocus());
        assertNull(nativePageView.getParent());
        verify(mTab, never()).removeObserver(any());

        mCompositorViewHolder.shutDown();
        // ContentView asserts that no drag-drop handler is set before accepting a new one, so this
        // verifies that shutDown() cleared it.
        mContentView.setEventOffsetHandlerForDragDrop(mock(EventOffsetHandler.class));
        verify(mTab).removeObserver(any());
    }

    @Test
    public void testDragEnd_forwardedToSourceThatMissedIt() {
        setUpDragStateTracker(mWebContents, mContentView, /* isDragStarted= */ true);
        mCompositorViewHolder.dispatchDragEvent(createDragEvent(DragEvent.ACTION_DRAG_STARTED));

        // The source's drag state stays started, as if it was detached and missed the drag end.
        DragEvent dragEndEvent = createDragEvent(DragEvent.ACTION_DRAG_ENDED);
        mCompositorViewHolder.dispatchDragEvent(dragEndEvent);
        verify(mOnDragListener).onDrag(mContentView, dragEndEvent);
        verify(mEventForwarder).onDragEvent(dragEndEvent, mContentView);
    }

    @Test
    public void testDragEnd_notForwardedToSourceThatHandledIt() {
        DragStateTracker tracker =
                setUpDragStateTracker(mWebContents, mContentView, /* isDragStarted= */ true);
        mCompositorViewHolder.dispatchDragEvent(createDragEvent(DragEvent.ACTION_DRAG_STARTED));
        // The source handles the drag end, which resets its drag state.
        when(tracker.isDragStarted()).thenReturn(false);

        mCompositorViewHolder.dispatchDragEvent(createDragEvent(DragEvent.ACTION_DRAG_ENDED));
        verifyDragEndNotForwarded(mContentView);
    }

    @Test
    public void testDragEnd_notForwardedToDestroyedSource() {
        setUpDragStateTracker(mWebContents, mContentView, /* isDragStarted= */ true);
        mCompositorViewHolder.dispatchDragEvent(createDragEvent(DragEvent.ACTION_DRAG_STARTED));
        // The source tab is closed during the drag.
        when(mWebContents.isDestroyed()).thenReturn(true);

        mCompositorViewHolder.dispatchDragEvent(createDragEvent(DragEvent.ACTION_DRAG_ENDED));
        verifyDragEndNotForwarded(mContentView);
    }

    @Test
    public void testDragEnd_notForwardedForDragFromElsewhere() {
        DragStateTracker tracker =
                setUpDragStateTracker(mWebContents, mContentView, /* isDragStarted= */ false);
        mCompositorViewHolder.dispatchDragEvent(createDragEvent(DragEvent.ACTION_DRAG_STARTED));
        when(tracker.isDragStarted()).thenReturn(true);

        mCompositorViewHolder.dispatchDragEvent(createDragEvent(DragEvent.ACTION_DRAG_ENDED));
        verifyDragEndNotForwarded(mContentView);
    }

    @Test
    public void testDropPermissions_heldUntilShutDown() {
        DropTestActivity activity = Robolectric.buildActivity(DropTestActivity.class).get();
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(activity));
        mCompositorViewHolder.onNativeLibraryReady(
                mWindowAndroid, /* tabContentManager= */ null, mPrefService);
        mCompositorViewHolder.onContentChanged();
        verify(mTab, atLeast(1)).addObserver(mTabObserverCaptor.capture());
        TabObserver tabObserver = mTabObserverCaptor.getValue();

        // Drop multiple files, then start another drag and drop another file.
        ClipData clipData =
                new ClipData(
                        "files",
                        new String[] {"image/png"},
                        new ClipData.Item(Uri.parse("content://media/image1.png")));
        clipData.addItem(new ClipData.Item(Uri.parse("content://media/image2.png")));
        dispatchDrop(clipData);
        mCompositorViewHolder.dispatchDragEvent(createDragEvent(DragEvent.ACTION_DRAG_STARTED));
        dispatchDrop(
                new ClipData(
                        "file",
                        new String[] {"image/png"},
                        new ClipData.Item(Uri.parse("content://media/image3.png"))));
        assertEquals(2, activity.mPermissions.size());

        // Loading other URLs and switching tabs keeps permissions.
        tabObserver.onPageLoadStarted(mTab, new GURL("https://www.example.com/"));
        Tab newTab = mTabModelSelector.addMockTab();
        ContentView newContentView =
                ContentView.createContentView(mActivity, /* webContents= */ null);
        when(newTab.getWebContents()).thenReturn(mock(WebContents.class));
        when(newTab.getContentView()).thenReturn(newContentView);
        when(newTab.getView()).thenReturn(newContentView);
        mTabModelSelector.getModel(false).setIndex(1, TabSelectionType.FROM_USER);
        mCompositorViewHolder.onContentChanged();
        for (DragAndDropPermissions permissions : activity.mPermissions) {
            verify(permissions, never()).release();
        }

        // Shutting down releases all permissions.
        mCompositorViewHolder.shutDown();
        for (DragAndDropPermissions permissions : activity.mPermissions) {
            verify(permissions).release();
        }
    }

    private void dispatchDrop(ClipData clipData) {
        mCompositorViewHolder.dispatchDragEvent(
                DragEventBuilder.newBuilder()
                        .setAction(DragEvent.ACTION_DROP)
                        .setClipData(clipData)
                        .build());
    }

    private static DragEvent createDragEvent(int action) {
        return DragEventBuilder.newBuilder().setAction(action).build();
    }

    private DragStateTracker setUpDragStateTracker(
            WebContents webContents, ContentView contentView, boolean isDragStarted) {
        DragStateTracker tracker = mock(DragStateTracker.class);
        when(tracker.isDragStarted()).thenReturn(isDragStarted);
        ViewAndroidDelegate delegate = mock(ViewAndroidDelegate.class);
        when(delegate.getDragStateTracker()).thenReturn(tracker);
        when(webContents.getViewAndroidDelegate()).thenReturn(delegate);
        when(webContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        when(webContents.getEventForwarder()).thenReturn(mEventForwarder);
        when(webContents.getOrSetUserData(eq(ViewFocusChangeSuppression.class), any()))
                .thenAnswer(
                        inv -> {
                            WebContents.UserDataFactory<ViewFocusChangeSuppression> factory =
                                    inv.getArgument(1);
                            return factory.create(webContents);
                        });
        UiUtils.removeViewFromParent(contentView);
        contentView.setWebContents(webContents);
        contentView.addOnDragListener(mOnDragListener);
        return tracker;
    }

    private void verifyDragEndNotForwarded(ContentView contentView) {
        verify(mOnDragListener, never()).onDrag(eq(contentView), any());
        verify(mEventForwarder, never()).onDragEvent(any(), eq(contentView));
    }

    /**
     * Shows a custom view for {@link #mTab} and lays it out with committed {@link SideUiSpecs} of
     * {@link #SIDE_UI_START_WIDTH} and {@link #SIDE_UI_END_WIDTH}.
     *
     * @return The custom view.
     */
    private View setUpCustomViewWithCommittedSideUiSpecs() {
        View customView = new View(mContext);
        when(mTab.isShowingCustomView()).thenReturn(true);
        when(mTab.getView()).thenReturn(customView);

        reset(mWebContents);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        mCompositorViewHolder.onNativeLibraryReady(
                mWindowAndroid, /* tabContentManager= */ null, mPrefService);
        mCompositorViewHolder.onContentChanged();

        SideUiSpecs committedSpecs = new SideUiSpecs(SIDE_UI_START_WIDTH, SIDE_UI_END_WIDTH);
        when(mSideUiStateProvider.getCurrentSideUiSpecs()).thenReturn(committedSpecs);
        when(mSideUiStateProvider.getExpectedSideUiSpecsForTab(mTab)).thenReturn(committedSpecs);
        mSideUiStateProviderSupplier.set(mSideUiStateProvider);
        runCurrentTasks();
        mCompositorViewHolder.onSideUiSpecsChangeCompleted(
                committedSpecs,
                UiUpdateRequest.getRequestForTesting(/* suppressAnimations= */ true));

        MarginLayoutParams layoutParams = (MarginLayoutParams) customView.getLayoutParams();
        assertEquals(SIDE_UI_START_WIDTH, layoutParams.leftMargin);
        assertEquals(SIDE_UI_END_WIDTH, layoutParams.rightMargin);
        assertEquals(0f, customView.getTranslationX(), /* delta= */ 0f);
        return customView;
    }
}
