// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import static android.view.Display.INVALID_DISPLAY;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.description;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.graphics.Rect;
import android.os.Build;
import android.os.Looper;
import android.os.OutcomeReceiver;
import android.util.Pair;
import android.view.Display;
import android.view.WindowInsets;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowDisplay;
import org.robolectric.shadows.ShadowDisplayManager;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ActivityState;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.build.annotations.Nullable;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager.FullscreenManagerDelegate;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcherImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAttributeKeys;
import org.chromium.chrome.browser.tab.TabAttributes;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.JUnitTestGURLs;

import java.util.Collections;
import java.util.HashMap;
import java.util.List;

/**
 * Unit tests for {@link FullscreenHtmlApiHandlerCompat}. TODO(crbug.com/40525786): Can be
 * parametrized with a parametrized Robolectric test runner.
 */
@Features.EnableFeatures({
    ChromeFeatureList.DISPLAY_EDGE_TO_EDGE_FULLSCREEN,
    ChromeFeatureList.ENABLE_FULLSCREEN_TO_ANY_SCREEN_ANDROID
})
@RunWith(BaseRobolectricTestRunner.class)
public class FullscreenHtmlApiHandlerCompatUnitTest {
    @SuppressLint("NewApi")
    private static final Insets NAVIGATION_BAR_INSETS = Insets.of(0, 0, 0, 100);

    private static final Insets STATUS_BAR_INSETS = Insets.of(0, 100, 0, 0);
    private static final Insets SYSTEM_BAR_INSETS = Insets.of(0, 100, 0, 100);

    private static final WindowInsetsCompat VISIBLE_SYSTEM_BARS_WINDOW_INSETS =
            new WindowInsetsCompat.Builder()
                    .setInsets(WindowInsetsCompat.Type.navigationBars(), NAVIGATION_BAR_INSETS)
                    .setInsets(WindowInsetsCompat.Type.statusBars(), STATUS_BAR_INSETS)
                    .setInsets(WindowInsetsCompat.Type.systemBars(), SYSTEM_BAR_INSETS)
                    .setVisible(WindowInsetsCompat.Type.navigationBars(), true)
                    .setVisible(WindowInsetsCompat.Type.statusBars(), true)
                    .setVisible(WindowInsetsCompat.Type.systemBars(), true)
                    .build();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public final FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    @Mock private TabBrowserControlsConstraintsHelper mTabBrowserControlsConstraintsHelper;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private TabModel mTabModel;
    @Mock private TabModelSelector mTabModelSelector;

    private TestContentView mContentView;
    private FullscreenTestActivity mActivity;
    private final ActivityTabProvider mActivityTabProvider = new ActivityTabProvider();
    private MultiWindowModeStateDispatcherImpl mMultiWindowModeStateDispatcher;
    private FullscreenHtmlApiHandlerCompat mFullscreenHtmlApiHandlerCompat;
    private SettableNonNullObservableSupplier<Boolean> mAreControlsHidden;
    private UserDataHost mHost;

    public static class FullscreenTestActivity extends Activity {
        public final HashMap<Integer, Integer> mCounters = new HashMap<>();

        @Override
        public void requestFullscreenMode(
                int request, OutcomeReceiver<Void, Throwable> approvalCallback) {
            if (approvalCallback != null) {
                approvalCallback.onResult(null);
            }
            mCounters.put(request, mCounters.getOrDefault(request, 0) + 1);
        }

        // Robolectric does not associate the Activity's context with a display.
        @Override
        public Display getDisplay() {
            return ShadowDisplay.getDefaultDisplay();
        }
    }

    private void assertEqualNumberOfEnterAndExitActivityFullscreenMode(int numberOfEnters) {
        if (numberOfEnters == 0) {
            assertFalse(mActivity.mCounters.containsKey(Activity.FULLSCREEN_MODE_REQUEST_ENTER));
            assertFalse(mActivity.mCounters.containsKey(Activity.FULLSCREEN_MODE_REQUEST_EXIT));
        } else {
            assertTrue(mActivity.mCounters.containsKey(Activity.FULLSCREEN_MODE_REQUEST_ENTER));
            assertEquals(
                    numberOfEnters,
                    (int) mActivity.mCounters.get(Activity.FULLSCREEN_MODE_REQUEST_ENTER));
            assertTrue(mActivity.mCounters.containsKey(Activity.FULLSCREEN_MODE_REQUEST_EXIT));
            assertEquals(
                    numberOfEnters,
                    (int) mActivity.mCounters.get(Activity.FULLSCREEN_MODE_REQUEST_EXIT));
        }
    }

    /** Returns the given root insets, which Robolectric cannot set on a view. */
    private static class TestContentView extends ContentView {
        private WindowInsets mRootWindowInsets;

        TestContentView(Context context) {
            super(context, /* webContents= */ null);
        }

        void setRootWindowInsets(WindowInsets insets) {
            mRootWindowInsets = insets;
        }

        @Override
        public WindowInsets getRootWindowInsets() {
            return mRootWindowInsets;
        }
    }

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(FullscreenTestActivity.class).setup().get();
        mContentView = new TestContentView(mActivity);

        mHost = new UserDataHost();
        doReturn(mHost).when(mTab).getUserDataHost();
        doReturn(ObservableSuppliers.createMonotonic())
                .when(mTabModelSelector)
                .getCurrentTabModelSupplier();

        mAreControlsHidden = ObservableSuppliers.createNonNull(false);
        mMultiWindowModeStateDispatcher = new MultiWindowModeStateDispatcherImpl(mActivity);
        mFullscreenHtmlApiHandlerCompat =
                new FullscreenHtmlApiHandlerCompat(
                        mActivity, mAreControlsHidden, false, mMultiWindowModeStateDispatcher) {
                    // This needs a PopupController, which isn't available in the test since we
                    // can't mock statics in this version of mockito.  Even if we could mock it, it
                    // casts to WebContentsImpl and other things that we can't reference due to
                    // restrictions in DEPS.
                    @Override
                    public void destroySelectActionMode(Tab tab) {}

                    @Override
                    protected void updateMultiTouchZoomSupport(boolean enable) {}
                };
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testFullscreenRequestCanceledAtPendingStateBeforeControlsDisappear() {
        // avoid calling GestureListenerManager/SelectionPopupController
        doReturn(null).when(mTab).getWebContents();
        doReturn(true).when(mTab).isUserInteractable();

        // Fullscreen process stops at pending state since controls are not hidden.
        mAreControlsHidden.set(false);
        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, INVALID_DISPLAY));

        TabBrowserControlsConstraintsHelper.setForTesting(
                mTab, mTabBrowserControlsConstraintsHelper);

        // Exit is invoked unexpectedly before the controls get hidden. Fullscreen process should be
        // marked as canceled.
        mFullscreenHtmlApiHandlerCompat.exitPersistentFullscreenMode();
        assertTrue(
                "Fullscreen request should have been canceled",
                mFullscreenHtmlApiHandlerCompat.getPendingFullscreenOptionsForTesting().canceled());

        // Controls are hidden afterwards.
        mAreControlsHidden.set(true);

        // The fullscreen request was canceled. Verify the controls are restored.
        verify(mTabBrowserControlsConstraintsHelper).update(BrowserControlsState.SHOWN, true);
        assertEquals(null, mFullscreenHtmlApiHandlerCompat.getPendingFullscreenOptionsForTesting());

        // Verify that fullscreen mode was exited properly
        assertEqualNumberOfEnterAndExitActivityFullscreenMode(1);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testFullscreenRequestCanceledAtPendingStateAfterControlsDisappear() {
        // Avoid calling GestureListenerManager/SelectionPopupController
        doReturn(null).when(mTab).getWebContents();
        doReturn(true).when(mTab).isUserInteractable();

        mAreControlsHidden.set(false);
        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, INVALID_DISPLAY));

        mAreControlsHidden.set(true);
        TabBrowserControlsConstraintsHelper.setForTesting(
                mTab, mTabBrowserControlsConstraintsHelper);

        // Exit is invoked unexpectedly _after_ the controls get hidden.
        mFullscreenHtmlApiHandlerCompat.exitPersistentFullscreenMode();

        // Verify the browser controls are restored.
        verify(mTabBrowserControlsConstraintsHelper).update(BrowserControlsState.SHOWN, true);
        assertEquals(null, mFullscreenHtmlApiHandlerCompat.getPendingFullscreenOptionsForTesting());

        // Verify that fullscreen mode was exited properly
        assertEqualNumberOfEnterAndExitActivityFullscreenMode(1);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testFullscreenAddAndRemoveObserver() {
        // avoid calling GestureListenerManager/SelectionPopupController
        doReturn(null).when(mTab).getWebContents();
        doReturn(true).when(mTab).isUserInteractable();

        // Fullscreen process stops at pending state since controls are not hidden.
        mAreControlsHidden.set(false);
        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);
        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerCompat.addObserver(observer);
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false, INVALID_DISPLAY);
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer).onEnterFullscreen(mTab, fullscreenOptions);
        Assert.assertEquals(
                "Observer is not added.",
                1,
                mFullscreenHtmlApiHandlerCompat.getObserversForTesting().size());

        // Exit is invoked unexpectedly before the controls get hidden. Fullscreen process should be
        // marked as canceled.
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        verify(observer).onExitFullscreen(mTab);

        mFullscreenHtmlApiHandlerCompat.destroy();
        Assert.assertEquals(
                "Observer is not removed.",
                0,
                mFullscreenHtmlApiHandlerCompat.getObserversForTesting().size());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testFullscreenObserverCalledOncePerSession() {
        // avoid calling GestureListenerManager/SelectionPopupController
        doReturn(null).when(mTab).getWebContents();
        doReturn(true).when(mTab).isUserInteractable();

        mAreControlsHidden.set(false);
        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);
        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerCompat.addObserver(observer);
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false, INVALID_DISPLAY);

        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, times(1)).onEnterFullscreen(mTab, fullscreenOptions);

        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        verify(observer, times(1)).onExitFullscreen(mTab);

        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, times(2)).onEnterFullscreen(mTab, fullscreenOptions);
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        verify(observer, times(2)).onExitFullscreen(mTab);

        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, times(3)).onEnterFullscreen(mTab, fullscreenOptions);
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        verify(observer, times(3)).onExitFullscreen(mTab);

        assertEqualNumberOfEnterAndExitActivityFullscreenMode(3);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testFullscreenObserverCalledOncePerSessionWhenWebContentsNotNull() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mContentView).when(mTab).getContentView();
        doReturn(true).when(mTab).isUserInteractable();
        doReturn(true).when(mTab).isHidden();
        mContentView.setRootWindowInsets(VISIBLE_SYSTEM_BARS_WINDOW_INSETS.toWindowInsets());
        mAreControlsHidden.set(true);

        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);
        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerCompat.addObserver(observer);
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false, INVALID_DISPLAY);

        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, times(1)).onEnterFullscreen(mTab, fullscreenOptions);

        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        verify(observer, times(1)).onExitFullscreen(mTab);

        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, times(2)).onEnterFullscreen(mTab, fullscreenOptions);
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        verify(observer, times(2)).onExitFullscreen(mTab);

        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, times(3)).onEnterFullscreen(mTab, fullscreenOptions);
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        verify(observer, times(3)).onExitFullscreen(mTab);

        assertEqualNumberOfEnterAndExitActivityFullscreenMode(3);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testNoObserverWhenCanceledBeforeBeingInteractable() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(false).when(mTab).isUserInteractable();

        mAreControlsHidden.set(false);
        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);
        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerCompat.addObserver(observer);
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false, INVALID_DISPLAY);

        // Before the tab becomes interactable, fullscreen exit gets requested.
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);

        verify(observer, never()).onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, never()).onExitFullscreen(mTab);

        assertEqualNumberOfEnterAndExitActivityFullscreenMode(0);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testFullscreenObserverInTabNonInteractableState() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(false).when(mTab).isUserInteractable(); // Tab not interactable at first.

        mAreControlsHidden.set(false);
        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);
        mFullscreenHtmlApiHandlerCompat.initialize(mActivityTabProvider, mTabModelSelector);
        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerCompat.addObserver(observer);
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false, INVALID_DISPLAY);
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, never()).onEnterFullscreen(mTab, fullscreenOptions);

        // Only after the tab turns interactable does the fullscreen mode is entered.
        mFullscreenHtmlApiHandlerCompat.onTabInteractable(mTab);
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer).onEnterFullscreen(mTab, fullscreenOptions);

        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        verify(observer, times(1)).onExitFullscreen(mTab);

        mFullscreenHtmlApiHandlerCompat.destroy();

        assertEqualNumberOfEnterAndExitActivityFullscreenMode(1);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testFullscreenObserverNotifiedWhenActivityStopped() {
        mFullscreenHtmlApiHandlerCompat =
                new FullscreenHtmlApiHandlerCompat(
                        mActivity, mAreControlsHidden, true, mMultiWindowModeStateDispatcher) {
                    @Override
                    public void destroySelectActionMode(Tab tab) {}
                };

        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mContentView).when(mTab).getContentView();
        doReturn(true).when(mTab).isUserInteractable();
        doReturn(true).when(mTab).isHidden();
        mContentView.setRootWindowInsets(VISIBLE_SYSTEM_BARS_WINDOW_INSETS.toWindowInsets());
        mAreControlsHidden.set(true);

        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);

        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerCompat.addObserver(observer);
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false, INVALID_DISPLAY);

        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, times(1)).onEnterFullscreen(mTab, fullscreenOptions);

        mFullscreenHtmlApiHandlerCompat.onActivityStateChange(mActivity, ActivityState.STOPPED);
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        verify(observer, times(1)).onExitFullscreen(mTab);

        assertEqualNumberOfEnterAndExitActivityFullscreenMode(1);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testFullscreenObserverCalledOnceWhenExitPersistentFullscreenModeCalled() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mContentView).when(mTab).getContentView();
        doReturn(true).when(mTab).isUserInteractable();
        doReturn(true).when(mTab).isHidden();
        mContentView.setRootWindowInsets(VISIBLE_SYSTEM_BARS_WINDOW_INSETS.toWindowInsets());
        mAreControlsHidden.set(true);

        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);

        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerCompat.addObserver(observer);
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false, INVALID_DISPLAY);

        // Enter full screen.
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, times(1)).onEnterFullscreen(mTab, fullscreenOptions);

        // Call exitPersistentFullscreenMode followed by onExitFullscreen. Observers should be
        // notified once.
        mFullscreenHtmlApiHandlerCompat.exitPersistentFullscreenMode();
        mFullscreenHtmlApiHandlerCompat.exitPersistentFullscreenMode();
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        verify(observer, times(1)).onExitFullscreen(mTab);

        assertEqualNumberOfEnterAndExitActivityFullscreenMode(1);
    }

    @Test
    public void testMultiWindowModeChangeIsHandledWhenNoTabIsActive() {
        mFullscreenHtmlApiHandlerCompat =
                new FullscreenHtmlApiHandlerCompat(
                        mActivity, mAreControlsHidden, true, mMultiWindowModeStateDispatcher) {
                    @Override
                    public void destroySelectActionMode(Tab tab) {}
                };
        mMultiWindowModeStateDispatcher.dispatchMultiWindowModeChanged(false);
        mMultiWindowModeStateDispatcher.dispatchMultiWindowModeChanged(true);
        mMultiWindowModeStateDispatcher.dispatchMultiWindowModeChanged(false);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testMultiWindowModeChangeIsHandledDuringAfterEnterFullscreen() {
        mFullscreenHtmlApiHandlerCompat =
                new FullscreenHtmlApiHandlerCompat(
                        mActivity, mAreControlsHidden, true, mMultiWindowModeStateDispatcher) {
                    @Override
                    public void destroySelectActionMode(Tab tab) {}
                };

        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mContentView).when(mTab).getContentView();
        doReturn(true).when(mTab).isUserInteractable();
        doReturn(true).when(mTab).isHidden();
        mContentView.setRootWindowInsets(VISIBLE_SYSTEM_BARS_WINDOW_INSETS.toWindowInsets());
        mAreControlsHidden.set(true);

        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false, INVALID_DISPLAY);

        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        mMultiWindowModeStateDispatcher.dispatchMultiWindowModeChanged(false);
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        mMultiWindowModeStateDispatcher.dispatchMultiWindowModeChanged(true);

        assertEqualNumberOfEnterAndExitActivityFullscreenMode(1);
    }

    @Test
    public void testNullRootWindowInsetsDoesNotCrash() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mContentView).when(mTab).getContentView();
        doReturn(true).when(mTab).isUserInteractable();
        mContentView.setRootWindowInsets(null);
        mAreControlsHidden.set(true);

        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);

        assertFalse(mFullscreenHtmlApiHandlerCompat.isStatusBarHidden(mContentView));
        assertFalse(mFullscreenHtmlApiHandlerCompat.isNavigationBarHidden(mContentView));

        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false, INVALID_DISPLAY);
        // Start small so that the layout below is a change to a bigger size.
        mContentView.layout(0, 0, 10, 10);
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);

        mContentView.layout(0, 0, 100, 100);

        // Process MSG_ID_SET_VISIBILITY_FOR_SYSTEM_BARS while getRootWindowInsets() is null.
        Shadows.shadowOf(Looper.getMainLooper()).idle();
        assertTrue(mFullscreenHtmlApiHandlerCompat.getPersistentFullscreenMode());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testExitFullscreenWhenActiveTabClearedBeforeTabExit() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mContentView).when(mTab).getContentView();
        doReturn(true).when(mTab).isUserInteractable();
        mContentView.setRootWindowInsets(VISIBLE_SYSTEM_BARS_WINDOW_INSETS.toWindowInsets());
        mAreControlsHidden.set(true);

        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);
        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerCompat.addObserver(observer);
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false, INVALID_DISPLAY);

        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        assertTrue(mFullscreenHtmlApiHandlerCompat.getPersistentFullscreenMode());

        // Simulate ActivityTabTabObserver clearing mTab when the last tab in a window is closed
        // before Tab/WebContents destruction invokes onExitFullscreen(mTab).
        mFullscreenHtmlApiHandlerCompat.setTabForTesting(null);
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);

        assertFalse(mFullscreenHtmlApiHandlerCompat.getPersistentFullscreenMode());
        verify(observer, times(1)).onExitFullscreen(mTab);
        assertEqualNumberOfEnterAndExitActivityFullscreenMode(1);
    }

    private static void setFullscreenAttributes(Tab tab, long displayId) {
        TabAttributes attrs = TabAttributes.from(tab);
        attrs.set(
                TabAttributeKeys.FULLSCREEN_OPTIONS,
                new FullscreenOptions(false, false, displayId));
        attrs.set(
                TabAttributeKeys.FULLSCREEN_START_POSITION,
                new Pair<Long, @Nullable Rect>(displayId, new Rect(0, 0, 10, 10)));
    }

    private static void assertFullscreenAttributesCleared(Tab tab) {
        TabAttributes attrs = TabAttributes.from(tab);
        assertNull(attrs.get(TabAttributeKeys.FULLSCREEN_OPTIONS));
        assertNull(attrs.get(TabAttributeKeys.FULLSCREEN_START_POSITION));
    }

    private void setUpTabForEnteredFullscreen() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mContentView).when(mTab).getContentView();
        doReturn(true).when(mTab).isUserInteractable();
        mContentView.setRootWindowInsets(VISIBLE_SYSTEM_BARS_WINDOW_INSETS.toWindowInsets());
        mAreControlsHidden.set(true);
        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testOnExitFullscreen_untrackedTab_clearsThatTabOnly() {
        setUpTabForEnteredFullscreen();
        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerCompat.addObserver(observer);
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, INVALID_DISPLAY));
        assertTrue(mFullscreenHtmlApiHandlerCompat.getPersistentFullscreenMode());
        setFullscreenAttributes(mTab, 1);

        Tab otherTab = Mockito.mock(Tab.class);
        doReturn(new UserDataHost()).when(otherTab).getUserDataHost();
        setFullscreenAttributes(otherTab, 2);

        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(otherTab);

        assertFullscreenAttributesCleared(otherTab);
        TabAttributes attrs = TabAttributes.from(mTab);
        FullscreenOptions options = attrs.get(TabAttributeKeys.FULLSCREEN_OPTIONS);
        assertNotNull("The tracked tab's attributes should be untouched.", options);
        assertEquals(1, options.displayId);
        assertNotNull(attrs.get(TabAttributeKeys.FULLSCREEN_START_POSITION));
        assertTrue(
                "The handler's own fullscreen session should be untouched.",
                mFullscreenHtmlApiHandlerCompat.getPersistentFullscreenMode());
        assertEquals(mTab, mFullscreenHtmlApiHandlerCompat.getTabInFullscreenForTesting());
        verify(observer, never()).onExitFullscreen(otherTab);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testOnExitFullscreen_clearsFullscreenAttributes() {
        setUpTabForEnteredFullscreen();
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, INVALID_DISPLAY));
        setFullscreenAttributes(mTab, 2);

        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);

        assertFullscreenAttributesCleared(mTab);
        assertFalse(mFullscreenHtmlApiHandlerCompat.getPersistentFullscreenMode());
    }

    private long getActivityDisplayId() {
        return mActivity.getDisplay().getDisplayId();
    }

    private @Nullable FullscreenOptions getStoredFullscreenOptions(Tab tab) {
        return TabAttributes.from(tab).get(TabAttributeKeys.FULLSCREEN_OPTIONS);
    }

    private void assertStoredDisplay(Tab tab, long expectedDisplayId) {
        FullscreenOptions stored = getStoredFullscreenOptions(tab);
        Assert.assertNotNull("FULLSCREEN_OPTIONS should be written.", stored);
        assertEquals(expectedDisplayId, stored.displayId);
    }

    /** Puts the tab in the state where a request is parked until the controls hide. */
    private void setUpTabForParkedFullscreen() {
        doReturn(null).when(mTab).getWebContents();
        doReturn(true).when(mTab).isUserInteractable();
        mAreControlsHidden.set(false);
        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);
    }

    /**
     * Provides an app task so that a task move would reach the returned delegate. Needs an SDK
     * where window move is available.
     */
    private AndroidTaskUtils.MoveTaskDelegate setUpWindowMove() {
        AndroidTaskUtils.setAppTaskForTesting(Mockito.mock(ActivityManager.AppTask.class));
        AndroidTaskUtils.MoveTaskDelegate moveTaskDelegate =
                Mockito.mock(AndroidTaskUtils.MoveTaskDelegate.class);
        AndroidTaskUtils.setMoveTaskDelegateForTesting(moveTaskDelegate);
        return moveTaskDelegate;
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testStoreFullscreenOptions_sameDisplayEntered() {
        setUpTabForEnteredFullscreen();
        FullscreenOptions options = new FullscreenOptions(true, false, INVALID_DISPLAY);

        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, options);

        assertTrue(mFullscreenHtmlApiHandlerCompat.getPersistentFullscreenMode());
        assertStoredDisplay(mTab, getActivityDisplayId());
        FullscreenOptions stored = getStoredFullscreenOptions(mTab);
        assertTrue(stored.showNavigationBar);
        assertFalse(stored.showStatusBar);
        Assert.assertNotSame("A copy should be stored.", options, stored);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testStoreFullscreenOptions_parked() {
        setUpTabForParkedFullscreen();

        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, INVALID_DISPLAY));

        Assert.assertNotNull(
                mFullscreenHtmlApiHandlerCompat.getPendingFullscreenOptionsForTesting());
        assertStoredDisplay(mTab, getActivityDisplayId());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testStoreFullscreenOptions_deferred() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(false).when(mTab).isUserInteractable();
        mAreControlsHidden.set(true);
        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);

        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, INVALID_DISPLAY));

        assertFalse(mFullscreenHtmlApiHandlerCompat.getPersistentFullscreenMode());
        assertStoredDisplay(mTab, getActivityDisplayId());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.CINNAMON_BUN)
    public void testStoreFullscreenOptions_namedCurrentDisplay() {
        AndroidTaskUtils.MoveTaskDelegate moveTaskDelegate = setUpWindowMove();
        setUpTabForEnteredFullscreen();

        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, getActivityDisplayId()));

        verify(moveTaskDelegate, never()).moveTaskTo(any(), anyInt(), any());
        assertTrue(mFullscreenHtmlApiHandlerCompat.getPersistentFullscreenMode());
        assertStoredDisplay(mTab, getActivityDisplayId());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.CINNAMON_BUN)
    public void testStoreFullscreenOptions_sameDisplayEnteredWithWindowMove() {
        AndroidTaskUtils.MoveTaskDelegate moveTaskDelegate = setUpWindowMove();
        setUpTabForEnteredFullscreen();

        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, INVALID_DISPLAY));

        verify(moveTaskDelegate, never()).moveTaskTo(any(), anyInt(), any());
        assertTrue(mFullscreenHtmlApiHandlerCompat.getPersistentFullscreenMode());
        assertStoredDisplay(mTab, getActivityDisplayId());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.Q)
    public void testStoreFullscreenOptions_displayUnresolved_clearsEarlierDisplay() {
        setUpTabForEnteredFullscreen();
        TabAttributes.from(mTab)
                .set(TabAttributeKeys.FULLSCREEN_OPTIONS, new FullscreenOptions(false, false, 42));

        // The current display cannot be resolved below Android R.
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, INVALID_DISPLAY));

        assertTrue(mFullscreenHtmlApiHandlerCompat.getPersistentFullscreenMode());
        assertNull(getStoredFullscreenOptions(mTab));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testStoreFullscreenOptions_namedOtherDisplayWithoutWindowMove() {
        setUpTabForEnteredFullscreen();
        long otherDisplayId = getActivityDisplayId() + 5;

        // Window move is unavailable below CINNAMON_BUN, so fullscreen is entered on the current
        // display and that is the display to store.
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, otherDisplayId));

        assertTrue(mFullscreenHtmlApiHandlerCompat.getPersistentFullscreenMode());
        assertStoredDisplay(mTab, getActivityDisplayId());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testStoreFullscreenOptions_skippedDuplicateRequest() {
        setUpTabForEnteredFullscreen();
        FullscreenOptions options = new FullscreenOptions(false, false, INVALID_DISPLAY);
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, options);
        FullscreenOptions sentinel = new FullscreenOptions(false, false, 42);
        TabAttributes.from(mTab).set(TabAttributeKeys.FULLSCREEN_OPTIONS, sentinel);

        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, options);

        Assert.assertSame(sentinel, getStoredFullscreenOptions(mTab));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    @Features.DisableFeatures(ChromeFeatureList.ENABLE_FULLSCREEN_TO_ANY_SCREEN_ANDROID)
    public void testStoreFullscreenOptions_flagOff() {
        setUpTabForEnteredFullscreen();

        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, INVALID_DISPLAY));

        assertStoredDisplay(mTab, getActivityDisplayId());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testStoreFullscreenOptions_enteredThenExit() {
        setUpTabForEnteredFullscreen();
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, INVALID_DISPLAY));
        assertStoredDisplay(mTab, getActivityDisplayId());

        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);

        assertNull(getStoredFullscreenOptions(mTab));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testStoreFullscreenOptions_parkedThenExit() {
        setUpTabForParkedFullscreen();
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, INVALID_DISPLAY));
        assertStoredDisplay(mTab, getActivityDisplayId());

        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);

        assertNull(getStoredFullscreenOptions(mTab));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testStoreFullscreenOptions_deferredThenExit() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(false).when(mTab).isUserInteractable();
        mAreControlsHidden.set(true);
        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, INVALID_DISPLAY));
        assertStoredDisplay(mTab, getActivityDisplayId());

        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);

        assertNull(getStoredFullscreenOptions(mTab));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testStoreFullscreenOptions_exitAllAfterActiveTabChangedWhileParked() {
        setUpTabForParkedFullscreen();
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, INVALID_DISPLAY));
        assertStoredDisplay(mTab, getActivityDisplayId());

        Tab newActiveTab = Mockito.mock(Tab.class);
        doReturn(new UserDataHost()).when(newActiveTab).getUserDataHost();
        mFullscreenHtmlApiHandlerCompat.setTabForTesting(newActiveTab);
        mFullscreenHtmlApiHandlerCompat.exitPersistentFullscreenMode();

        assertNull(getStoredFullscreenOptions(mTab));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testStoreFullscreenOptions_exitAllAfterDestroyLeavesTabUntouched() {
        setUpTabForParkedFullscreen();
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, INVALID_DISPLAY));
        assertStoredDisplay(mTab, getActivityDisplayId());

        mFullscreenHtmlApiHandlerCompat.destroy();
        mFullscreenHtmlApiHandlerCompat.exitPersistentFullscreenMode();

        assertStoredDisplay(mTab, getActivityDisplayId());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.CINNAMON_BUN)
    public void testStoreFullscreenOptions_crossDisplay() {
        AndroidTaskUtils.MoveTaskDelegate moveTaskDelegate = setUpWindowMove();
        int otherDisplayId = ShadowDisplayManager.addDisplay("w1920dp-h1080dp");
        Assert.assertNotEquals(getActivityDisplayId(), otherDisplayId);
        setUpTabForEnteredFullscreen();

        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, otherDisplayId));

        verify(moveTaskDelegate).moveTaskTo(any(), eq(otherDisplayId), any());
        assertStoredDisplay(mTab, otherDisplayId);

        // The cross-display branch does not track the tab as pending, so an exit-all with a
        // different active tab leaves the attribute for the recreated activity.
        Tab newActiveTab = Mockito.mock(Tab.class);
        doReturn(new UserDataHost()).when(newActiveTab).getUserDataHost();
        mFullscreenHtmlApiHandlerCompat.setTabForTesting(newActiveTab);
        mFullscreenHtmlApiHandlerCompat.exitPersistentFullscreenMode();

        assertStoredDisplay(mTab, otherDisplayId);
    }

    /** Returns a tab mock that can hold attributes and enter fullscreen. */
    private Tab createTabThatCanEnterFullscreen() {
        Tab tab = Mockito.mock(Tab.class);
        TestContentView contentView = new TestContentView(mActivity);
        contentView.setRootWindowInsets(VISIBLE_SYSTEM_BARS_WINDOW_INSETS.toWindowInsets());
        doReturn(new UserDataHost()).when(tab).getUserDataHost();
        doReturn(Mockito.mock(WebContents.class)).when(tab).getWebContents();
        doReturn(contentView).when(tab).getContentView();
        doReturn(true).when(tab).isUserInteractable();
        return tab;
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testStoreFullscreenOptions_parkedEntersOnOtherTab_movesToEnteringTab() {
        setUpTabForParkedFullscreen();
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, INVALID_DISPLAY));
        assertStoredDisplay(mTab, getActivityDisplayId());

        // The active tab changes while the request is parked, so the request enters on it.
        Tab newActiveTab = createTabThatCanEnterFullscreen();
        mFullscreenHtmlApiHandlerCompat.setTabForTesting(newActiveTab);
        mAreControlsHidden.set(true);

        assertEquals(newActiveTab, mFullscreenHtmlApiHandlerCompat.getTabInFullscreenForTesting());
        assertNull(
                "The display should move off the tab that asked.",
                getStoredFullscreenOptions(mTab));
        assertStoredDisplay(newActiveTab, getActivityDisplayId());

        mFullscreenHtmlApiHandlerCompat.exitPersistentFullscreenMode();

        assertNull(getStoredFullscreenOptions(newActiveTab));
    }

    /**
     * Initializes the handler with a tab model holding {@link #mTab}, so that the handler's tab
     * observer receives registration events for it.
     *
     * @return The tab model observer, to remove the tab with.
     */
    private TabModelObserver initializeWithRegisteredTab() {
        doReturn(ObservableSuppliers.createMonotonic(mTabModel))
                .when(mTabModelSelector)
                .getCurrentTabModelSupplier();
        doReturn(List.of(mTabModel)).when(mTabModelSelector).getModels();
        TabList emptyTabList = Mockito.mock(TabList.class);
        doReturn(Collections.emptyIterator()).when(emptyTabList).iterator();
        doReturn(emptyTabList).when(mTabModel).getComprehensiveModel();
        mFullscreenHtmlApiHandlerCompat.initialize(mActivityTabProvider, mTabModelSelector);
        ShadowLooper.idleMainLooper();
        ArgumentCaptor<TabModelObserver> modelObserver =
                ArgumentCaptor.forClass(TabModelObserver.class);
        verify(mTabModel).addObserver(modelObserver.capture());
        modelObserver
                .getValue()
                .didAddTab(
                        mTab,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);
        return modelObserver.getValue();
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testStoreFullscreenOptions_unregisteredPendingTab_stopsTrackingIt() {
        TabModelObserver modelObserver = initializeWithRegisteredTab();

        setUpTabForParkedFullscreen();
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, INVALID_DISPLAY));

        // The tab is reparented to another window.
        modelObserver.tabRemoved(mTab);
        Tab newActiveTab = Mockito.mock(Tab.class);
        doReturn(new UserDataHost()).when(newActiveTab).getUserDataHost();
        mFullscreenHtmlApiHandlerCompat.setTabForTesting(newActiveTab);
        mFullscreenHtmlApiHandlerCompat.exitPersistentFullscreenMode();

        assertStoredDisplay(mTab, getActivityDisplayId());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testOnTabUnregistered_clearsDeferredEnterKeepsDisplay() {
        TabModelObserver modelObserver = initializeWithRegisteredTab();
        setUpTabForDeferredFullscreen();
        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerCompat.addObserver(observer);
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, INVALID_DISPLAY));
        assertNotNull(TabAttributes.from(mTab).get(TabAttributeKeys.ENTER_FULLSCREEN));

        // The tab is reparented to another window.
        modelObserver.tabRemoved(mTab);

        assertNull(
                "The deferred enter is bound to this handler and must not travel with the tab.",
                TabAttributes.from(mTab).get(TabAttributeKeys.ENTER_FULLSCREEN));
        assertStoredDisplay(mTab, getActivityDisplayId());
        doReturn(true).when(mTab).isUserInteractable();
        mFullscreenHtmlApiHandlerCompat.onTabInteractable(mTab);
        assertFalse(mFullscreenHtmlApiHandlerCompat.getPersistentFullscreenMode());
        verify(observer, never()).onEnterFullscreen(any(), any());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testStoreFullscreenOptions_supersededPendingTab_clearsPriorTab() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(false).when(mTab).isUserInteractable();
        mAreControlsHidden.set(true);
        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, INVALID_DISPLAY));
        assertStoredDisplay(mTab, getActivityDisplayId());

        Tab otherTab = createTabThatCanEnterFullscreen();
        doReturn(false).when(otherTab).isUserInteractable();
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                otherTab, new FullscreenOptions(true, false, INVALID_DISPLAY));

        assertNull(getStoredFullscreenOptions(mTab));
        assertStoredDisplay(otherTab, getActivityDisplayId());

        // The new request's tab is tracked, so an exit-all clears it.
        mFullscreenHtmlApiHandlerCompat.exitPersistentFullscreenMode();

        assertNull(getStoredFullscreenOptions(otherTab));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.CINNAMON_BUN)
    public void testStoreFullscreenOptions_supersededPendingTabByCrossDisplay_clearsPriorTab() {
        setUpWindowMove();
        int otherDisplayId = ShadowDisplayManager.addDisplay("w1920dp-h1080dp");
        setUpTabForParkedFullscreen();
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, INVALID_DISPLAY));
        assertStoredDisplay(mTab, getActivityDisplayId());

        Tab otherTab = createTabThatCanEnterFullscreen();
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                otherTab, new FullscreenOptions(false, false, otherDisplayId));

        assertNull(getStoredFullscreenOptions(mTab));
        assertStoredDisplay(otherTab, otherDisplayId);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testStoreFullscreenOptions_sameTabFollowUpRequest_keepsOwnDisplay() {
        setUpTabForParkedFullscreen();
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, INVALID_DISPLAY));

        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(true, false, INVALID_DISPLAY));

        assertStoredDisplay(mTab, getActivityDisplayId());
        assertTrue(
                "The follow-up request's options should be stored.",
                getStoredFullscreenOptions(mTab).showNavigationBar);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testStoreFullscreenOptions_supersededParkedRequestStillEnters_recordsDisplay() {
        setUpTabForParkedFullscreen();
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, INVALID_DISPLAY));

        // A background tab's request is deferred and supersedes the parked one.
        Tab otherTab = createTabThatCanEnterFullscreen();
        doReturn(false).when(otherTab).isUserInteractable();
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                otherTab, new FullscreenOptions(true, false, INVALID_DISPLAY));
        assertNull(getStoredFullscreenOptions(mTab));

        // The parked request still enters on the active tab when the controls hide.
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mContentView).when(mTab).getContentView();
        mContentView.setRootWindowInsets(VISIBLE_SYSTEM_BARS_WINDOW_INSETS.toWindowInsets());
        mAreControlsHidden.set(true);

        assertEquals(mTab, mFullscreenHtmlApiHandlerCompat.getTabInFullscreenForTesting());
        assertStoredDisplay(mTab, getActivityDisplayId());
        assertStoredDisplay(otherTab, getActivityDisplayId());

        mFullscreenHtmlApiHandlerCompat.exitPersistentFullscreenMode();

        assertNull(getStoredFullscreenOptions(mTab));
        assertNull(getStoredFullscreenOptions(otherTab));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testStoreFullscreenOptions_exitAllAfterActiveTabChangedWhileEntered() {
        setUpTabForEnteredFullscreen();
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, INVALID_DISPLAY));
        assertEquals(mTab, mFullscreenHtmlApiHandlerCompat.getTabInFullscreenForTesting());

        Tab newActiveTab = Mockito.mock(Tab.class);
        doReturn(new UserDataHost()).when(newActiveTab).getUserDataHost();
        mFullscreenHtmlApiHandlerCompat.setTabForTesting(newActiveTab);
        mFullscreenHtmlApiHandlerCompat.exitPersistentFullscreenMode();

        assertNull(getStoredFullscreenOptions(mTab));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    @Features.EnableFeatures(ChromeFeatureList.ANDROID_ZOOM_IMMERSIVE)
    public void testImmersiveRebind_movesFullscreenAttributes() {
        enterFullscreenAndSetMockDelegate();
        setFullscreenAttributes(mTab, 2);

        Tab newTab = createTabThatCanEnterFullscreen();
        doReturn(newTab).when(mTabModelSelector).getCurrentTab();
        mActivityTabProvider.setForTesting(newTab);

        assertEquals(newTab, mFullscreenHtmlApiHandlerCompat.getTabInFullscreenForTesting());
        assertFullscreenAttributesCleared(mTab);
        // The display is resolved again for the window rather than copied from the old tab.
        assertStoredDisplay(newTab, getActivityDisplayId());
        Pair<Long, @Nullable Rect> startPosition =
                TabAttributes.from(newTab).get(TabAttributeKeys.FULLSCREEN_START_POSITION);
        Assert.assertNotNull("The start position should move with the session.", startPosition);
        assertEquals(2L, (long) startPosition.first);

        mFullscreenHtmlApiHandlerCompat.exitPersistentFullscreenMode();

        assertFullscreenAttributesCleared(newTab);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    @Features.EnableFeatures(ChromeFeatureList.ANDROID_ZOOM_IMMERSIVE)
    public void testImmersiveRebind_replacesEarlierDisplayOnNewTab() {
        enterFullscreenAndSetMockDelegate();
        TabAttributes.from(mTab).clear(TabAttributeKeys.FULLSCREEN_OPTIONS);

        Tab newTab = createTabThatCanEnterFullscreen();
        TabAttributes.from(newTab)
                .set(TabAttributeKeys.FULLSCREEN_OPTIONS, new FullscreenOptions(false, false, 42));
        doReturn(newTab).when(mTabModelSelector).getCurrentTab();
        mActivityTabProvider.setForTesting(newTab);

        assertEquals(newTab, mFullscreenHtmlApiHandlerCompat.getTabInFullscreenForTesting());
        assertStoredDisplay(newTab, getActivityDisplayId());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testOnExitFullscreen_untrackedPendingTab_stopsTrackingIt() {
        setUpTabForParkedFullscreen();
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, INVALID_DISPLAY));
        Tab newActiveTab = Mockito.mock(Tab.class);
        doReturn(new UserDataHost()).when(newActiveTab).getUserDataHost();
        mFullscreenHtmlApiHandlerCompat.setTabForTesting(newActiveTab);

        // The exit names a tab that is neither active nor in fullscreen.
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        assertNull(getStoredFullscreenOptions(mTab));

        // A later value on that tab is no longer the handler's to clear.
        FullscreenOptions later = new FullscreenOptions(false, false, 42);
        TabAttributes.from(mTab).set(TabAttributeKeys.FULLSCREEN_OPTIONS, later);
        mFullscreenHtmlApiHandlerCompat.exitPersistentFullscreenMode();

        Assert.assertSame(later, getStoredFullscreenOptions(mTab));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    @Features.EnableFeatures(ChromeFeatureList.ANDROID_ZOOM_IMMERSIVE)
    public void testImmersiveRebind_sameTab_keepsAttributes() {
        enterFullscreenAndSetMockDelegate();
        setFullscreenAttributes(mTab, 2);

        mFullscreenHtmlApiHandlerCompat.getTabFullscreenObserverForTesting().onContentChanged(mTab);

        assertEquals(mTab, mFullscreenHtmlApiHandlerCompat.getTabInFullscreenForTesting());
        assertStoredDisplay(mTab, 2);
        Assert.assertNotNull(
                TabAttributes.from(mTab).get(TabAttributeKeys.FULLSCREEN_START_POSITION));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    @Features.EnableFeatures(ChromeFeatureList.ANDROID_ZOOM_IMMERSIVE)
    public void testImmersiveRebind_keepsDeferredPendingTab() {
        enterFullscreenAndSetMockDelegate();
        Tab otherTab = createTabThatCanEnterFullscreen();
        doReturn(false).when(otherTab).isUserInteractable();
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                otherTab, new FullscreenOptions(true, false, INVALID_DISPLAY));
        assertStoredDisplay(otherTab, getActivityDisplayId());

        mFullscreenHtmlApiHandlerCompat.getTabFullscreenObserverForTesting().onContentChanged(mTab);
        mFullscreenHtmlApiHandlerCompat.exitPersistentFullscreenMode();

        assertNull(
                "The deferred tab should still be tracked after the rebind.",
                getStoredFullscreenOptions(otherTab));
    }

    /** Puts mTab in the state where a request is deferred until the tab is interactable. */
    private void setUpTabForDeferredFullscreen() {
        setUpTabForEnteredFullscreen();
        doReturn(false).when(mTab).isUserInteractable();
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testOnExitFullscreen_untrackedTab_clearsEnterFullscreenRunnable() {
        setUpTabForEnteredFullscreen();
        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerCompat.addObserver(observer);
        Tab otherTab = createTabThatCanEnterFullscreen();
        doReturn(false).when(otherTab).isUserInteractable();
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                otherTab, new FullscreenOptions(false, false, INVALID_DISPLAY));

        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(otherTab);
        doReturn(true).when(otherTab).isUserInteractable();
        mFullscreenHtmlApiHandlerCompat.onTabInteractable(otherTab);

        assertFalse(mFullscreenHtmlApiHandlerCompat.getPersistentFullscreenMode());
        verify(observer, never()).onEnterFullscreen(any(), any());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testOnExitFullscreen_destroyedTab_doesNotCrash() {
        setUpTabForEnteredFullscreen();
        Tab destroyedTab = Mockito.mock(Tab.class);
        UserDataHost host = new UserDataHost();
        doReturn(host).when(destroyedTab).getUserDataHost();
        doReturn(true).when(destroyedTab).isDestroyed();
        host.destroy();

        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(destroyedTab);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testExitPersistentFullscreenMode_clearsDeferredRunnable() {
        setUpTabForDeferredFullscreen();
        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerCompat.addObserver(observer);
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, INVALID_DISPLAY));

        mFullscreenHtmlApiHandlerCompat.exitPersistentFullscreenMode();
        doReturn(true).when(mTab).isUserInteractable();
        mFullscreenHtmlApiHandlerCompat.onTabInteractable(mTab);

        assertFalse(mFullscreenHtmlApiHandlerCompat.getPersistentFullscreenMode());
        verify(observer, never()).onEnterFullscreen(any(), any());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testExitPersistentFullscreenMode_clearsBackgroundPendingTabRunnable() {
        setUpTabForEnteredFullscreen();
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, INVALID_DISPLAY));
        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerCompat.addObserver(observer);
        Tab otherTab = createTabThatCanEnterFullscreen();
        doReturn(false).when(otherTab).isUserInteractable();
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                otherTab, new FullscreenOptions(true, false, INVALID_DISPLAY));

        mFullscreenHtmlApiHandlerCompat.exitPersistentFullscreenMode();
        doReturn(true).when(otherTab).isUserInteractable();
        mFullscreenHtmlApiHandlerCompat.onTabInteractable(otherTab);

        assertFalse(mFullscreenHtmlApiHandlerCompat.getPersistentFullscreenMode());
        verify(observer, never()).onEnterFullscreen(any(), any());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testOnEnterFullscreen_supersededDeferredTab_clearsPriorRunnable() {
        setUpTabForDeferredFullscreen();
        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerCompat.addObserver(observer);
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, INVALID_DISPLAY));
        Tab otherTab = createTabThatCanEnterFullscreen();
        doReturn(false).when(otherTab).isUserInteractable();
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                otherTab, new FullscreenOptions(true, false, INVALID_DISPLAY));

        doReturn(true).when(mTab).isUserInteractable();
        mFullscreenHtmlApiHandlerCompat.onTabInteractable(mTab);

        assertFalse(mFullscreenHtmlApiHandlerCompat.getPersistentFullscreenMode());
        verify(observer, never()).onEnterFullscreen(eq(mTab), any());
    }

    private FullscreenManagerDelegate enterFullscreenAndSetMockDelegate() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mContentView).when(mTab).getContentView();
        doReturn(true).when(mTab).isUserInteractable();
        doReturn(mTab).when(mTabModelSelector).getCurrentTab();
        Mockito.lenient().doReturn(mTabModel).when(mTabModelSelector).getCurrentModel();
        Mockito.lenient().doReturn(2).when(mTabModel).getCount();

        // Status bar and navigation bar should be visible.
        mContentView.setRootWindowInsets(VISIBLE_SYSTEM_BARS_WINDOW_INSETS.toWindowInsets());
        mAreControlsHidden.set(true);

        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);
        mFullscreenHtmlApiHandlerCompat.initialize(mActivityTabProvider, mTabModelSelector);
        FullscreenOptions fullscreenOptions =
                new FullscreenOptions(
                        /* showNavigationBar= */ false,
                        /* showStatusBar= */ false,
                        /* displayId= */ INVALID_DISPLAY);

        // Enter persistent fullscreen mode.
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);

        FullscreenManagerDelegate delegate = Mockito.mock(FullscreenManagerDelegate.class);
        mFullscreenHtmlApiHandlerCompat.setFullscreenManagerDelegate(delegate);
        return delegate;
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.ANDROID_ZOOM_IMMERSIVE)
    public void testOnHidden_ChangedTabs_WithImmersiveModeEnabled() {
        FullscreenManagerDelegate delegate = enterFullscreenAndSetMockDelegate();
        assertEquals(
                "Initial fullscreen tab should be mTab.",
                mTab,
                mFullscreenHtmlApiHandlerCompat.getTabInFullscreenForTesting());

        // New tab we are switching to.
        Tab newTab = Mockito.mock(Tab.class);
        WebContents newWebContents = Mockito.mock(WebContents.class);
        TestContentView newContentView = new TestContentView(mActivity);
        doReturn(new UserDataHost()).when(newTab).getUserDataHost();
        doReturn(newWebContents).when(newTab).getWebContents();
        doReturn(newContentView).when(newTab).getContentView();
        doReturn(newTab).when(mTabModelSelector).getCurrentTab();

        // Simulate a user switching to a new tab: first the outgoing tab is hidden inside
        // TabModelSelectorImpl.requestToShowTab, and then ActivityTabProvider notifies
        // mActiveTabObserver.onObservingDifferentTab(newTab) once the new tab is selected.
        mFullscreenHtmlApiHandlerCompat
                .getTabFullscreenObserverForTesting()
                .onHidden(mTab, /* type= */ TabHidingType.CHANGED_TABS);
        mActivityTabProvider.setForTesting(newTab);

        verify(
                        delegate,
                        description(
                                "Should pass the hidden tab to exit only tab fullscreen while"
                                        + " keeping browser fullscreen active."))
                .onExitFullscreen(mTab);
        verify(
                        delegate,
                        never().description(
                                        "Should not pass null on tab switch when immersive mode is"
                                                + " enabled, as that exits browser fullscreen."))
                .onExitFullscreen(null);
        assertEquals(
                "Active fullscreen tab reference should update to newTab after switching tabs.",
                newTab,
                mFullscreenHtmlApiHandlerCompat.getTabInFullscreenForTesting());
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.ANDROID_ZOOM_IMMERSIVE)
    public void testOnHidden_ChangedTabs_WithImmersiveModeDisabled() {
        FullscreenManagerDelegate delegate = enterFullscreenAndSetMockDelegate();

        mFullscreenHtmlApiHandlerCompat
                .getTabFullscreenObserverForTesting()
                .onHidden(mTab, /* type= */ TabHidingType.CHANGED_TABS);

        verify(
                        delegate,
                        description(
                                "Should pass null on tab switch to exit fullscreen when immersive"
                                        + " mode is disabled."))
                .onExitFullscreen(null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.ANDROID_ZOOM_IMMERSIVE)
    public void testOnDidFinishNavigationInPrimaryMainFrame_WithImmersiveModeEnabled() {
        FullscreenManagerDelegate delegate = enterFullscreenAndSetMockDelegate();
        NavigationHandle navigation = Mockito.mock(NavigationHandle.class);
        // Navigate to a different document (e.g., clicking a link to a new page) within the same
        // tab.
        doReturn(false).when(navigation).isSameDocument();

        mFullscreenHtmlApiHandlerCompat
                .getTabFullscreenObserverForTesting()
                .onDidFinishNavigationInPrimaryMainFrame(mTab, navigation);

        verify(
                        delegate,
                        description(
                                "Should pass the current tab on cross-document navigation to exit"
                                        + " only tab fullscreen while keeping browser fullscreen"
                                        + " active."))
                .onExitFullscreen(mTab);
        verify(
                        delegate,
                        never().description(
                                        "Should not pass null on cross-document navigation when"
                                                + " immersive mode is enabled, as that exits"
                                                + " browser fullscreen."))
                .onExitFullscreen(null);
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.ANDROID_ZOOM_IMMERSIVE)
    public void testOnDidFinishNavigationInPrimaryMainFrame_WithImmersiveModeDisabled() {
        FullscreenManagerDelegate delegate = enterFullscreenAndSetMockDelegate();

        NavigationHandle navigation = Mockito.mock(NavigationHandle.class);
        doReturn(false).when(navigation).isSameDocument();

        mFullscreenHtmlApiHandlerCompat
                .getTabFullscreenObserverForTesting()
                .onDidFinishNavigationInPrimaryMainFrame(mTab, navigation);

        verify(
                        delegate,
                        description(
                                "Should pass null on cross-document navigation to exit fullscreen"
                                        + " when immersive mode is disabled."))
                .onExitFullscreen(null);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.ANDROID_ZOOM_IMMERSIVE)
    public void testImmersiveModeMetrics_ZeroActivitySession_RecordsZeroCountsAndExactDuration() {
        mFullscreenHtmlApiHandlerCompat.onEnterBrowserImmersiveMode();
        enterFullscreenAndSetMockDelegate();
        mFakeTimeTestRule.advanceMillis(3000);

        try (HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(ImmersiveModeMetrics.HISTOGRAM_NAVIGATIONS_PER_SESSION, 0)
                        .expectIntRecord(ImmersiveModeMetrics.HISTOGRAM_TAB_SWITCHES_PER_SESSION, 0)
                        .expectIntRecord(ImmersiveModeMetrics.HISTOGRAM_SESSION_DURATION, 3000)
                        .build()) {
            mFullscreenHtmlApiHandlerCompat.exitPersistentFullscreenMode();
        }
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.ANDROID_ZOOM_IMMERSIVE)
    public void testImmersiveModeMetrics_SessionCountersRecordedOnExit() {
        mFullscreenHtmlApiHandlerCompat.onEnterBrowserImmersiveMode();
        enterFullscreenAndSetMockDelegate();

        NavigationHandle crossDocNavigation = Mockito.mock(NavigationHandle.class);
        doReturn(false).when(crossDocNavigation).isSameDocument();
        doReturn(true).when(crossDocNavigation).hasCommitted();
        doReturn(JUnitTestGURLs.EXAMPLE_URL).when(crossDocNavigation).getUrl();

        // Perform 2 committed cross-document navigations while in Browser immersive mode.
        mFullscreenHtmlApiHandlerCompat
                .getTabFullscreenObserverForTesting()
                .onDidFinishNavigationInPrimaryMainFrame(mTab, crossDocNavigation);
        mFullscreenHtmlApiHandlerCompat
                .getTabFullscreenObserverForTesting()
                .onDidFinishNavigationInPrimaryMainFrame(mTab, crossDocNavigation);

        // Perform 3 tab switches while remaining in Browser immersive mode.
        mFullscreenHtmlApiHandlerCompat
                .getTabFullscreenObserverForTesting()
                .onHidden(mTab, TabHidingType.CHANGED_TABS);
        mFullscreenHtmlApiHandlerCompat
                .getTabFullscreenObserverForTesting()
                .onHidden(mTab, TabHidingType.CHANGED_TABS);
        mFullscreenHtmlApiHandlerCompat
                .getTabFullscreenObserverForTesting()
                .onHidden(mTab, TabHidingType.CHANGED_TABS);

        mFakeTimeTestRule.advanceMillis(15000);

        try (HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(ImmersiveModeMetrics.HISTOGRAM_NAVIGATIONS_PER_SESSION, 2)
                        .expectIntRecord(ImmersiveModeMetrics.HISTOGRAM_TAB_SWITCHES_PER_SESSION, 3)
                        .expectIntRecord(ImmersiveModeMetrics.HISTOGRAM_SESSION_DURATION, 15000)
                        .build()) {
            mFullscreenHtmlApiHandlerCompat.exitPersistentFullscreenMode();
            // Calling exit a second time should not emit duplicate session histograms.
            mFullscreenHtmlApiHandlerCompat.exitPersistentFullscreenMode();
        }
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.ANDROID_ZOOM_IMMERSIVE)
    public void testImmersiveModeMetrics_ExcludedNavigationsAndNonTabSwitchHiding_NotCounted() {
        mFullscreenHtmlApiHandlerCompat.onEnterBrowserImmersiveMode();
        enterFullscreenAndSetMockDelegate();

        // 1. Same-document navigation (e.g. anchor link) should not count.
        NavigationHandle sameDocNavigation = Mockito.mock(NavigationHandle.class);
        doReturn(true).when(sameDocNavigation).isSameDocument();
        doReturn(true).when(sameDocNavigation).hasCommitted();
        mFullscreenHtmlApiHandlerCompat
                .getTabFullscreenObserverForTesting()
                .onDidFinishNavigationInPrimaryMainFrame(mTab, sameDocNavigation);

        // 2. Uncommitted navigation should not count.
        NavigationHandle uncommittedNavigation = Mockito.mock(NavigationHandle.class);
        doReturn(false).when(uncommittedNavigation).isSameDocument();
        doReturn(false).when(uncommittedNavigation).hasCommitted();
        mFullscreenHtmlApiHandlerCompat
                .getTabFullscreenObserverForTesting()
                .onDidFinishNavigationInPrimaryMainFrame(mTab, uncommittedNavigation);

        // 3. Page reload (e.g. Ctrl + R) should not count.
        NavigationHandle reloadNavigation = Mockito.mock(NavigationHandle.class);
        doReturn(false).when(reloadNavigation).isSameDocument();
        doReturn(true).when(reloadNavigation).hasCommitted();
        doReturn(true).when(reloadNavigation).isReload();
        mFullscreenHtmlApiHandlerCompat
                .getTabFullscreenObserverForTesting()
                .onDidFinishNavigationInPrimaryMainFrame(mTab, reloadNavigation);

        // 4. Initial New Tab Page load with chrome://new-tab-page/ URL (when opening a new tab via
        // Ctrl + T) should not count.
        NavigationHandle ntpUrlNavigation = Mockito.mock(NavigationHandle.class);
        doReturn(false).when(ntpUrlNavigation).isSameDocument();
        doReturn(true).when(ntpUrlNavigation).hasCommitted();
        doReturn(JUnitTestGURLs.NTP_URL).when(ntpUrlNavigation).getUrl();
        mFullscreenHtmlApiHandlerCompat
                .getTabFullscreenObserverForTesting()
                .onDidFinishNavigationInPrimaryMainFrame(mTab, ntpUrlNavigation);

        // 5. Hiding a tab due to activity backgrounding (rather than CHANGED_TABS) should not count
        // as a tab switch.
        mFullscreenHtmlApiHandlerCompat
                .getTabFullscreenObserverForTesting()
                .onHidden(mTab, TabHidingType.ACTIVITY_HIDDEN);

        // 6. Closing the last tab (CHANGED_TABS when the current TabModel is now empty) should not
        // count as a tab switch.
        doReturn(0).when(mTabModel).getCount();
        mFullscreenHtmlApiHandlerCompat
                .getTabFullscreenObserverForTesting()
                .onHidden(mTab, TabHidingType.CHANGED_TABS);

        mFakeTimeTestRule.advanceMillis(8000);

        try (HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(ImmersiveModeMetrics.HISTOGRAM_NAVIGATIONS_PER_SESSION, 0)
                        .expectIntRecord(ImmersiveModeMetrics.HISTOGRAM_TAB_SWITCHES_PER_SESSION, 0)
                        .expectIntRecord(ImmersiveModeMetrics.HISTOGRAM_SESSION_DURATION, 8000)
                        .build()) {
            mFullscreenHtmlApiHandlerCompat.exitPersistentFullscreenMode();
        }
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.ANDROID_ZOOM_IMMERSIVE)
    public void
            testImmersiveModeMetrics_MultipleSequentialSessions_ResetsCountersBetweenSessions() {
        NavigationHandle crossDocNavigation = Mockito.mock(NavigationHandle.class);
        doReturn(false).when(crossDocNavigation).isSameDocument();
        doReturn(true).when(crossDocNavigation).hasCommitted();
        doReturn(JUnitTestGURLs.EXAMPLE_URL).when(crossDocNavigation).getUrl();

        // Session 1: 2 navigations, 1 tab switch, 10000ms duration.
        mFullscreenHtmlApiHandlerCompat.onEnterBrowserImmersiveMode();
        enterFullscreenAndSetMockDelegate();
        mFullscreenHtmlApiHandlerCompat
                .getTabFullscreenObserverForTesting()
                .onDidFinishNavigationInPrimaryMainFrame(mTab, crossDocNavigation);
        mFullscreenHtmlApiHandlerCompat
                .getTabFullscreenObserverForTesting()
                .onDidFinishNavigationInPrimaryMainFrame(mTab, crossDocNavigation);
        mFullscreenHtmlApiHandlerCompat
                .getTabFullscreenObserverForTesting()
                .onHidden(mTab, TabHidingType.CHANGED_TABS);
        mFakeTimeTestRule.advanceMillis(10000);

        try (HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(ImmersiveModeMetrics.HISTOGRAM_NAVIGATIONS_PER_SESSION, 2)
                        .expectIntRecord(ImmersiveModeMetrics.HISTOGRAM_TAB_SWITCHES_PER_SESSION, 1)
                        .expectIntRecord(ImmersiveModeMetrics.HISTOGRAM_SESSION_DURATION, 10000)
                        .build()) {
            mFullscreenHtmlApiHandlerCompat.exitPersistentFullscreenMode();
        }

        // Session 2: 1 navigation, 2 tab switches, 4500ms duration (verifies counters and start
        // time reset cleanly after Session 1).
        mFullscreenHtmlApiHandlerCompat.onEnterBrowserImmersiveMode();
        enterFullscreenAndSetMockDelegate();
        mFullscreenHtmlApiHandlerCompat
                .getTabFullscreenObserverForTesting()
                .onDidFinishNavigationInPrimaryMainFrame(mTab, crossDocNavigation);
        mFullscreenHtmlApiHandlerCompat
                .getTabFullscreenObserverForTesting()
                .onHidden(mTab, TabHidingType.CHANGED_TABS);
        mFullscreenHtmlApiHandlerCompat
                .getTabFullscreenObserverForTesting()
                .onHidden(mTab, TabHidingType.CHANGED_TABS);
        mFakeTimeTestRule.advanceMillis(4500);

        try (HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(ImmersiveModeMetrics.HISTOGRAM_NAVIGATIONS_PER_SESSION, 1)
                        .expectIntRecord(ImmersiveModeMetrics.HISTOGRAM_TAB_SWITCHES_PER_SESSION, 2)
                        .expectIntRecord(ImmersiveModeMetrics.HISTOGRAM_SESSION_DURATION, 4500)
                        .build()) {
            mFullscreenHtmlApiHandlerCompat.exitPersistentFullscreenMode();
        }
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.ANDROID_ZOOM_IMMERSIVE)
    public void testImmersiveModeMetrics_NotRecordedForTabFullscreenSession() {
        // Enter regular HTML5 tab fullscreen (without calling onEnterBrowserImmersiveMode).
        enterFullscreenAndSetMockDelegate();

        NavigationHandle crossDocNavigation = Mockito.mock(NavigationHandle.class);
        doReturn(false).when(crossDocNavigation).isSameDocument();
        doReturn(true).when(crossDocNavigation).hasCommitted();
        mFullscreenHtmlApiHandlerCompat
                .getTabFullscreenObserverForTesting()
                .onDidFinishNavigationInPrimaryMainFrame(mTab, crossDocNavigation);
        mFullscreenHtmlApiHandlerCompat
                .getTabFullscreenObserverForTesting()
                .onHidden(mTab, TabHidingType.CHANGED_TABS);

        try (HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(ImmersiveModeMetrics.HISTOGRAM_NAVIGATIONS_PER_SESSION)
                        .expectNoRecords(ImmersiveModeMetrics.HISTOGRAM_TAB_SWITCHES_PER_SESSION)
                        .expectNoRecords(ImmersiveModeMetrics.HISTOGRAM_SESSION_DURATION)
                        .build()) {
            mFullscreenHtmlApiHandlerCompat.exitPersistentFullscreenMode();
        }
    }
}
