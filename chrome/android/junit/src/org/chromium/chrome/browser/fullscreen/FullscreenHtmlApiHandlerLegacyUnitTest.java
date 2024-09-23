// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View.OnLayoutChangeListener;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.ActivityState;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.WebContents;

/** Unit tests for {@link FullscreenHtmlApiHandlerLegacy}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FullscreenHtmlApiHandlerLegacyUnitTest {
    private static final int DEVICE_WIDTH = 900;
    private static final int DEVICE_HEIGHT = 1600;
    private static final int SYSTEM_UI_HEIGHT = 100;

    private Activity mActivity;
    @Mock private TabBrowserControlsConstraintsHelper mTabBrowserControlsConstraintsHelper;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private ContentView mContentView;
    @Mock private ActivityTabProvider mActivityTabProvider;
    @Mock private TabModelSelector mTabModelSelector;

    private FullscreenHtmlApiHandlerLegacy mFullscreenHtmlApiHandlerLegacy;
    private ObservableSupplierImpl<Boolean> mAreControlsHidden;
    private UserDataHost mHost;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mHost = new UserDataHost();
        doReturn(mHost).when(mTab).getUserDataHost();

        mAreControlsHidden = new ObservableSupplierImpl<Boolean>();
        mFullscreenHtmlApiHandlerLegacy =
                new FullscreenHtmlApiHandlerLegacy(mActivity, mAreControlsHidden, false) {
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
    public void testFullscreenRequestCanceledAtPendingStateBeforeControlsDisappear() {
        // avoid calling GestureListenerManager/SelectionPopupController
        doReturn(null).when(mTab).getWebContents();
        doReturn(true).when(mTab).isUserInteractable();

        // Fullscreen process stops at pending state since controls are not hidden.
        mAreControlsHidden.set(false);
        mFullscreenHtmlApiHandlerLegacy.setTabForTesting(mTab);
        mFullscreenHtmlApiHandlerLegacy.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false));

        TabBrowserControlsConstraintsHelper.setForTesting(
                mTab, mTabBrowserControlsConstraintsHelper);

        // Exit is invoked unexpectedly before the controls get hidden. Fullscreen process should be
        // marked as canceled.
        mFullscreenHtmlApiHandlerLegacy.exitPersistentFullscreenMode();
        assertTrue(
                "Fullscreen request should have been canceled",
                mFullscreenHtmlApiHandlerLegacy.getPendingFullscreenOptionsForTesting().canceled());

        // Controls are hidden afterwards.
        mAreControlsHidden.set(true);

        // The fullscreen request was canceled. Verify the controls are restored.
        verify(mTabBrowserControlsConstraintsHelper).update(BrowserControlsState.SHOWN, true);
        assertEquals(null, mFullscreenHtmlApiHandlerLegacy.getPendingFullscreenOptionsForTesting());
    }

    @Test
    public void testFullscreenRequestCanceledAtPendingStateAfterControlsDisappear() {
        // Avoid calling GestureListenerManager/SelectionPopupController
        doReturn(null).when(mTab).getWebContents();
        doReturn(true).when(mTab).isUserInteractable();

        mAreControlsHidden.set(false);
        mFullscreenHtmlApiHandlerLegacy.setTabForTesting(mTab);
        mFullscreenHtmlApiHandlerLegacy.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false));

        mAreControlsHidden.set(true);
        TabBrowserControlsConstraintsHelper.setForTesting(
                mTab, mTabBrowserControlsConstraintsHelper);

        // Exit is invoked unexpectedly _after_ the controls get hidden.
        mFullscreenHtmlApiHandlerLegacy.exitPersistentFullscreenMode();

        // Verify the browser controls are restored.
        verify(mTabBrowserControlsConstraintsHelper).update(BrowserControlsState.SHOWN, true);
        assertEquals(null, mFullscreenHtmlApiHandlerLegacy.getPendingFullscreenOptionsForTesting());
    }

    @Test
    public void testFullscreenAddAndRemoveObserver() {
        // avoid calling GestureListenerManager/SelectionPopupController
        doReturn(null).when(mTab).getWebContents();
        doReturn(true).when(mTab).isUserInteractable();

        // Fullscreen process stops at pending state since controls are not hidden.
        mAreControlsHidden.set(false);
        mFullscreenHtmlApiHandlerLegacy.setTabForTesting(mTab);
        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerLegacy.addObserver(observer);
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false);
        mFullscreenHtmlApiHandlerLegacy.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer).onEnterFullscreen(mTab, fullscreenOptions);
        Assert.assertEquals(
                "Observer is not added.",
                1,
                mFullscreenHtmlApiHandlerLegacy.getObserversForTesting().size());

        // Exit is invoked unexpectedly before the controls get hidden. Fullscreen process should be
        // marked as canceled.
        mFullscreenHtmlApiHandlerLegacy.onExitFullscreen(mTab);
        verify(observer).onExitFullscreen(mTab);

        mFullscreenHtmlApiHandlerLegacy.destroy();
        Assert.assertEquals(
                "Observer is not removed.",
                0,
                mFullscreenHtmlApiHandlerLegacy.getObserversForTesting().size());
    }

    @Test
    public void testFullscreenObserverCalledOncePerSession() {
        // avoid calling GestureListenerManager/SelectionPopupController
        doReturn(null).when(mTab).getWebContents();
        doReturn(true).when(mTab).isUserInteractable();

        mAreControlsHidden.set(false);
        mFullscreenHtmlApiHandlerLegacy.setTabForTesting(mTab);
        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerLegacy.addObserver(observer);
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false);

        mFullscreenHtmlApiHandlerLegacy.onEnterFullscreen(mTab, fullscreenOptions);
        mFullscreenHtmlApiHandlerLegacy.onEnterFullscreen(mTab, fullscreenOptions);
        mFullscreenHtmlApiHandlerLegacy.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, times(1)).onEnterFullscreen(mTab, fullscreenOptions);

        mFullscreenHtmlApiHandlerLegacy.onExitFullscreen(mTab);
        mFullscreenHtmlApiHandlerLegacy.onExitFullscreen(mTab);
        mFullscreenHtmlApiHandlerLegacy.onExitFullscreen(mTab);
        verify(observer, times(1)).onExitFullscreen(mTab);

        mFullscreenHtmlApiHandlerLegacy.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, times(2)).onEnterFullscreen(mTab, fullscreenOptions);
        mFullscreenHtmlApiHandlerLegacy.onExitFullscreen(mTab);
        verify(observer, times(2)).onExitFullscreen(mTab);

        mFullscreenHtmlApiHandlerLegacy.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, times(3)).onEnterFullscreen(mTab, fullscreenOptions);
        mFullscreenHtmlApiHandlerLegacy.onExitFullscreen(mTab);
        verify(observer, times(3)).onExitFullscreen(mTab);
    }

    @Test
    public void testFullscreenObserverCalledOncePerSessionWhenWebContentsNotNull() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mContentView).when(mTab).getContentView();
        doReturn(true).when(mTab).isUserInteractable();
        doReturn(true).when(mTab).isHidden();
        doReturn(true).when(mContentView).hasWindowFocus();
        mAreControlsHidden.set(true);

        mFullscreenHtmlApiHandlerLegacy.setTabForTesting(mTab);
        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerLegacy.addObserver(observer);
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false);

        mFullscreenHtmlApiHandlerLegacy.onEnterFullscreen(mTab, fullscreenOptions);
        mFullscreenHtmlApiHandlerLegacy.onEnterFullscreen(mTab, fullscreenOptions);
        mFullscreenHtmlApiHandlerLegacy.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, times(1)).onEnterFullscreen(mTab, fullscreenOptions);

        mFullscreenHtmlApiHandlerLegacy.onExitFullscreen(mTab);
        mFullscreenHtmlApiHandlerLegacy.onExitFullscreen(mTab);
        mFullscreenHtmlApiHandlerLegacy.onExitFullscreen(mTab);
        verify(observer, times(1)).onExitFullscreen(mTab);

        mFullscreenHtmlApiHandlerLegacy.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, times(2)).onEnterFullscreen(mTab, fullscreenOptions);
        mFullscreenHtmlApiHandlerLegacy.onExitFullscreen(mTab);
        verify(observer, times(2)).onExitFullscreen(mTab);

        mFullscreenHtmlApiHandlerLegacy.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, times(3)).onEnterFullscreen(mTab, fullscreenOptions);
        mFullscreenHtmlApiHandlerLegacy.onExitFullscreen(mTab);
        verify(observer, times(3)).onExitFullscreen(mTab);
    }

    @Test
    public void testNoObserverWhenCanceledBeforeBeingInteractable() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(false).when(mTab).isUserInteractable();

        mAreControlsHidden.set(false);
        mFullscreenHtmlApiHandlerLegacy.setTabForTesting(mTab);
        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerLegacy.addObserver(observer);
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false);

        // Before the tab becomes interactable, fullscreen exit gets requested.
        mFullscreenHtmlApiHandlerLegacy.onEnterFullscreen(mTab, fullscreenOptions);
        mFullscreenHtmlApiHandlerLegacy.onExitFullscreen(mTab);

        verify(observer, never()).onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, never()).onExitFullscreen(mTab);
    }

    @Test
    public void testFullscreenObserverInTabNonInteractableState() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(false).when(mTab).isUserInteractable(); // Tab not interactable at first.

        mAreControlsHidden.set(false);
        mFullscreenHtmlApiHandlerLegacy.setTabForTesting(mTab);
        mFullscreenHtmlApiHandlerLegacy.initialize(mActivityTabProvider, mTabModelSelector);
        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerLegacy.addObserver(observer);
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false);
        mFullscreenHtmlApiHandlerLegacy.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, never()).onEnterFullscreen(mTab, fullscreenOptions);

        // Only after the tab turns interactable does the fullscreen mode is entered.
        mFullscreenHtmlApiHandlerLegacy.onTabInteractable(mTab);
        mFullscreenHtmlApiHandlerLegacy.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer).onEnterFullscreen(mTab, fullscreenOptions);

        mFullscreenHtmlApiHandlerLegacy.onExitFullscreen(mTab);
        mFullscreenHtmlApiHandlerLegacy.onExitFullscreen(mTab);
        verify(observer, times(1)).onExitFullscreen(mTab);

        mFullscreenHtmlApiHandlerLegacy.destroy();
    }

    @Test
    public void testToastIsShownInFullscreenButNotPictureInPicture() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mContentView).when(mTab).getContentView();
        doReturn(true).when(mTab).isUserInteractable();
        doReturn(true).when(mTab).isHidden();
        // The window must have focus and already have the controls hidden, else fullscreen will be
        // deferred.  The toast would be deferred with it.
        doReturn(true).when(mContentView).hasWindowFocus();
        mAreControlsHidden.set(true);

        mFullscreenHtmlApiHandlerLegacy.setTabForTesting(mTab);

        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false);
        mFullscreenHtmlApiHandlerLegacy.onEnterFullscreen(mTab, fullscreenOptions);

        // Catch the layout listener, which is an implementation detail but what can one do?  Note
        // that we make the layout appear to have gotten bigger, which is important since the
        // fullscreen handler checks for it.
        ArgumentCaptor<OnLayoutChangeListener> arg =
                ArgumentCaptor.forClass(OnLayoutChangeListener.class);
        verify(mContentView).addOnLayoutChangeListener(arg.capture());
        arg.getValue().onLayoutChange(mContentView, 0, 0, 100, 100, 0, 0, 10, 10);

        // We should now be in fullscreen, with the toast shown.
        assertTrue(
                "Fullscreen toast should be visible in fullscreen",
                mFullscreenHtmlApiHandlerLegacy.isToastVisibleForTesting());

        // Losing / gaining the focus should hide / show the toast when it's applicable.  This also
        // covers picture in picture.
        mFullscreenHtmlApiHandlerLegacy.onWindowFocusChanged(mActivity, false);
        assertTrue(
                "Fullscreen toast should not be visible when unfocused",
                !mFullscreenHtmlApiHandlerLegacy.isToastVisibleForTesting());
        mFullscreenHtmlApiHandlerLegacy.onWindowFocusChanged(mActivity, true);
        assertTrue(
                "Fullscreen toast should be visible when focused",
                mFullscreenHtmlApiHandlerLegacy.isToastVisibleForTesting());

        // Toast should not be visible when we exit fullscreen.
        mFullscreenHtmlApiHandlerLegacy.exitPersistentFullscreenMode();
        assertTrue(
                "Fullscreen toast should not be visible outside of fullscreen",
                !mFullscreenHtmlApiHandlerLegacy.isToastVisibleForTesting());

        // If we gain / lose the focus outside of fullscreen, then nothing interesting should happen
        // with the toast.
        mFullscreenHtmlApiHandlerLegacy.onActivityStateChange(mActivity, ActivityState.PAUSED);
        assertTrue(
                "Fullscreen toast should not be visible after pause",
                !mFullscreenHtmlApiHandlerLegacy.isToastVisibleForTesting());
        mFullscreenHtmlApiHandlerLegacy.onActivityStateChange(mActivity, ActivityState.RESUMED);
        assertTrue(
                "Fullscreen toast should not be visible after resume when not in fullscreen",
                !mFullscreenHtmlApiHandlerLegacy.isToastVisibleForTesting());
    }

    @Test
    public void testToastIsShownAtLayoutChangeWithRotation() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mContentView).when(mTab).getContentView();
        doReturn(true).when(mTab).isUserInteractable();
        doReturn(true).when(mTab).isHidden();
        doReturn(true).when(mContentView).hasWindowFocus();
        mAreControlsHidden.set(true);

        mFullscreenHtmlApiHandlerLegacy.setTabForTesting(mTab);

        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false);
        mFullscreenHtmlApiHandlerLegacy.onEnterFullscreen(mTab, fullscreenOptions);

        ArgumentCaptor<OnLayoutChangeListener> arg =
                ArgumentCaptor.forClass(OnLayoutChangeListener.class);
        verify(mContentView).addOnLayoutChangeListener(arg.capture());

        // Device rotation swaps device width/height dimension.
        arg.getValue()
                .onLayoutChange(
                        mContentView,
                        0,
                        0,
                        DEVICE_HEIGHT,
                        DEVICE_WIDTH,
                        0,
                        0,
                        /* oldRight= */ DEVICE_WIDTH,
                        /* oldBottom= */ DEVICE_HEIGHT - SYSTEM_UI_HEIGHT);

        // We should now be in fullscreen, with the toast shown.
        assertTrue(
                "Fullscreen toast should be visible in fullscreen",
                mFullscreenHtmlApiHandlerLegacy.isToastVisibleForTesting());
    }

    @Test
    public void testFullscreenObserverNotifiedWhenActivityStopped() {
        mFullscreenHtmlApiHandlerLegacy =
                new FullscreenHtmlApiHandlerLegacy(mActivity, mAreControlsHidden, true) {
                    @Override
                    public void destroySelectActionMode(Tab tab) {}
                };

        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mContentView).when(mTab).getContentView();
        doReturn(true).when(mTab).isUserInteractable();
        doReturn(true).when(mTab).isHidden();
        doReturn(true).when(mContentView).hasWindowFocus();
        mAreControlsHidden.set(true);

        mFullscreenHtmlApiHandlerLegacy.setTabForTesting(mTab);

        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerLegacy.addObserver(observer);
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false);

        mFullscreenHtmlApiHandlerLegacy.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, times(1)).onEnterFullscreen(mTab, fullscreenOptions);

        mFullscreenHtmlApiHandlerLegacy.onActivityStateChange(mActivity, ActivityState.STOPPED);
        mFullscreenHtmlApiHandlerLegacy.onExitFullscreen(mTab);
        verify(observer, times(1)).onExitFullscreen(mTab);
    }

    @Test
    public void testFullscreenObserverCalledOnceWhenExitPersistentFullscreenModeCalled() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mContentView).when(mTab).getContentView();
        doReturn(true).when(mTab).isUserInteractable();
        doReturn(true).when(mTab).isHidden();
        doReturn(true).when(mContentView).hasWindowFocus();
        mAreControlsHidden.set(true);

        mFullscreenHtmlApiHandlerLegacy.setTabForTesting(mTab);

        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerLegacy.addObserver(observer);
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false);

        // Enter full screen.
        mFullscreenHtmlApiHandlerLegacy.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, times(1)).onEnterFullscreen(mTab, fullscreenOptions);

        // Call exitPersistentFullscreenMode followed by onExitFullscreen. Observers should be
        // notified once.
        mFullscreenHtmlApiHandlerLegacy.exitPersistentFullscreenMode();
        mFullscreenHtmlApiHandlerLegacy.exitPersistentFullscreenMode();
        mFullscreenHtmlApiHandlerLegacy.onExitFullscreen(mTab);
        mFullscreenHtmlApiHandlerLegacy.onExitFullscreen(mTab);
        verify(observer, times(1)).onExitFullscreen(mTab);
    }
}
