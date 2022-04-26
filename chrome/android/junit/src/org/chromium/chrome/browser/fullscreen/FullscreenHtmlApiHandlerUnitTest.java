// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAttributes;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.chrome.test.util.browser.Features;

/**
 * Unit tests for {@link FullscreenHtmlApiHandler}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class FullscreenHtmlApiHandlerUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private Activity mActivity;
    @Mock
    private TabAttributes mTabAttributes;
    @Mock
    private TabBrowserControlsConstraintsHelper mTabBrowserControlsConstraintsHelper;
    @Mock
    private Tab mTab;

    private FullscreenHtmlApiHandler mFullscreenHtmlApiHandler;
    private ObservableSupplierImpl<Boolean> mAreControlsHidden;
    private UserDataHost mHost;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mHost = new UserDataHost();
        doReturn(mHost).when(mTab).getUserDataHost();

        mAreControlsHidden = new ObservableSupplierImpl<Boolean>();
        mFullscreenHtmlApiHandler =
                new FullscreenHtmlApiHandler(mActivity, mAreControlsHidden, false);
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
}
