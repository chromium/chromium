// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import static android.view.Display.INVALID_DISPLAY;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.ActivityState;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.WebContents;

/** Unit tests for {@link FullscreenHtmlApiHandlerLegacy}. */
@Features.EnableFeatures({
    ChromeFeatureList.DISPLAY_EDGE_TO_EDGE_FULLSCREEN,
    ChromeFeatureList.ENABLE_FULLSCREEN_TO_ANY_SCREEN_ANDROID
})
@RunWith(BaseRobolectricTestRunner.class)
public class FullscreenHtmlApiHandlerLegacyUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    private Activity mActivity;
    @Mock private TabBrowserControlsConstraintsHelper mTabBrowserControlsConstraintsHelper;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;

    private ContentView mContentView;
    private final ActivityTabProvider mActivityTabProvider = new ActivityTabProvider();
    private FullscreenHtmlApiHandlerLegacy mFullscreenHtmlApiHandlerLegacy;
    private SettableNonNullObservableSupplier<Boolean> mAreControlsHidden;
    private UserDataHost mHost;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mContentView = ContentView.createContentView(mActivity, /* webContents= */ null);
        mHost = new UserDataHost();
        doReturn(mHost).when(mTab).getUserDataHost();
        doReturn(ObservableSuppliers.createMonotonic())
                .when(mTabModelSelector)
                .getCurrentTabModelSupplier();

        mAreControlsHidden = ObservableSuppliers.createNonNull(false);
        mFullscreenHtmlApiHandlerLegacy =
                new FullscreenHtmlApiHandlerLegacy(
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
    public void testFullscreenRequestCanceledAtPendingStateBeforeControlsDisappear() {
        // avoid calling GestureListenerManager/SelectionPopupController
        doReturn(null).when(mTab).getWebContents();
        doReturn(true).when(mTab).isUserInteractable();

        // Fullscreen process stops at pending state since controls are not hidden.
        mAreControlsHidden.set(false);
        mFullscreenHtmlApiHandlerLegacy.setTabForTesting(mTab);
        mFullscreenHtmlApiHandlerLegacy.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, INVALID_DISPLAY));

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
                mTab, new FullscreenOptions(false, false, INVALID_DISPLAY));

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
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false, INVALID_DISPLAY);
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
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false, INVALID_DISPLAY);

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
        mAreControlsHidden.set(true);

        mFullscreenHtmlApiHandlerLegacy.setTabForTesting(mTab);
        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerLegacy.addObserver(observer);
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false, INVALID_DISPLAY);

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
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false, INVALID_DISPLAY);

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
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false, INVALID_DISPLAY);
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
    public void testFullscreenObserverNotifiedWhenActivityStopped() {
        mFullscreenHtmlApiHandlerLegacy =
                new FullscreenHtmlApiHandlerLegacy(
                        mActivity, mAreControlsHidden, true, mMultiWindowModeStateDispatcher) {
                    @Override
                    public void destroySelectActionMode(Tab tab) {}
                };

        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mContentView).when(mTab).getContentView();
        doReturn(true).when(mTab).isUserInteractable();
        doReturn(true).when(mTab).isHidden();
        mAreControlsHidden.set(true);

        mFullscreenHtmlApiHandlerLegacy.setTabForTesting(mTab);

        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerLegacy.addObserver(observer);
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false, INVALID_DISPLAY);

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
        mAreControlsHidden.set(true);

        mFullscreenHtmlApiHandlerLegacy.setTabForTesting(mTab);

        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerLegacy.addObserver(observer);
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false, INVALID_DISPLAY);

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
