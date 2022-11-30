// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View.OnLayoutChangeListener;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.ActivityState;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAttributes;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.WebContents;

/**
 * Unit tests for {@link FullscreenHtmlApiHandler}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class FullscreenHtmlApiHandlerUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private Activity mActivity;
    @Mock
    private TabAttributes mTabAttributes;
    @Mock
    private TabBrowserControlsConstraintsHelper mTabBrowserControlsConstraintsHelper;
    @Mock
    private Tab mTab;
    @Mock
    private WebContents mWebContents;
    @Mock
    private ContentView mContentView;

    private FullscreenHtmlApiHandler mFullscreenHtmlApiHandler;
    private ObservableSupplierImpl<Boolean> mAreControlsHidden;
    private UserDataHost mHost;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mHost = new UserDataHost();
        doReturn(mHost).when(mTab).getUserDataHost();

        mAreControlsHidden = new ObservableSupplierImpl<Boolean>();
        mFullscreenHtmlApiHandler =
                new FullscreenHtmlApiHandler(mActivity, mAreControlsHidden, false) {
                    // This needs a PopupController, which isn't available in the test since we
                    // can't mock statics in this version of mockito.  Even if we could mock it, it
                    // casts to WebContentsImpl and other things that we can't reference due to
                    // restrictions in DEPS.
                    @Override
                    public void destroySelectActionMode(Tab tab) {}
                };
    }

    @Test
    public void testFullscreenRequestCanceledAtPendingState() {
        // avoid calling GestureListenerManager/SelectionPopupController
        doReturn(null).when(mTab).getWebContents();
        doReturn(true).when(mTab).isUserInteractable();

        // Fullscreen process stops at pending state since controls are not hidden.
        mAreControlsHidden.set(false);
        mFullscreenHtmlApiHandler.setTabForTesting(mTab);
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false);
        mFullscreenHtmlApiHandler.onEnterFullscreen(mTab, fullscreenOptions);

        // Exit is invoked unexpectedly before the controls get hidden. Fullscreen process should be
        // marked as canceled.
        mFullscreenHtmlApiHandler.exitPersistentFullscreenMode();
        assertTrue("Fullscreen request should have been canceled", fullscreenOptions.canceled());

        // Controls are hidden afterwards. Since the fullscreen request was canceled, we should
        // restore the controls.
        TabBrowserControlsConstraintsHelper.setForTesting(
                mTab, mTabBrowserControlsConstraintsHelper);
        mAreControlsHidden.set(true);
        verify(mTabBrowserControlsConstraintsHelper, times(1))
                .update(BrowserControlsState.SHOWN, true);
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

        mFullscreenHtmlApiHandler.setTabForTesting(mTab);

        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false);
        mFullscreenHtmlApiHandler.onEnterFullscreen(mTab, fullscreenOptions);

        // Catch the layout listener, which is an implementation detail but what can one do?  Note
        // that we make the layout appear to have gotten bigger, which is important since the
        // fullscreen handler checks for it.
        ArgumentCaptor<OnLayoutChangeListener> arg =
                ArgumentCaptor.forClass(OnLayoutChangeListener.class);
        verify(mContentView).addOnLayoutChangeListener(arg.capture());
        arg.getValue().onLayoutChange(mContentView, 0, 0, 100, 100, 0, 0, 10, 10);

        // We should now be in fullscreen, with the toast shown.
        assertTrue("Fullscreen toast should be visible in fullscreen",
                mFullscreenHtmlApiHandler.isToastVisibleForTesting());

        // Losing / gaining the focus should hide / show the toast when it's applicable.  This also
        // covers picture in picture.
        mFullscreenHtmlApiHandler.onWindowFocusChanged(mActivity, false);
        assertTrue("Fullscreen toast should not be visible when unfocused",
                !mFullscreenHtmlApiHandler.isToastVisibleForTesting());
        mFullscreenHtmlApiHandler.onWindowFocusChanged(mActivity, true);
        assertTrue("Fullscreen toast should be visible when focused",
                mFullscreenHtmlApiHandler.isToastVisibleForTesting());

        // Toast should not be visible when we exit fullscreen.
        mFullscreenHtmlApiHandler.exitPersistentFullscreenMode();
        assertTrue("Fullscreen toast should not be visible outside of fullscreen",
                !mFullscreenHtmlApiHandler.isToastVisibleForTesting());

        // If we gain / lose the focus outside of fullscreen, then nothing interesting should happen
        // with the toast.
        mFullscreenHtmlApiHandler.onActivityStateChange(mActivity, ActivityState.PAUSED);
        assertTrue("Fullscreen toast should not be visible after pause",
                !mFullscreenHtmlApiHandler.isToastVisibleForTesting());
        mFullscreenHtmlApiHandler.onActivityStateChange(mActivity, ActivityState.RESUMED);
        assertTrue("Fullscreen toast should not be visible after resume when not in fullscreen",
                !mFullscreenHtmlApiHandler.isToastVisibleForTesting());
    }
}
