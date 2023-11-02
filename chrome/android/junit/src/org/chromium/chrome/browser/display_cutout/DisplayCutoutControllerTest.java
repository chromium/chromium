// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.display_cutout;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.Window;
import android.view.WindowManager.LayoutParams;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink.mojom.ViewportFit;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.components.browser_ui.display_cutout.DisplayCutoutController;
import org.chromium.components.browser_ui.widget.InsetObserverView;
import org.chromium.components.browser_ui.widget.InsetObserverViewSupplier;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Tests for {@link DisplayCutoutController} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DisplayCutoutControllerTest {
    @Mock
    private TabImpl mTab;

    @Mock
    private WebContents mWebContents;

    @Mock
    private WindowAndroid mWindowAndroid;

    @Mock
    private Window mWindow;

    @Captor
    private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    @Mock
    private ChromeActivity mChromeActivity;

    @Mock
    private InsetObserverView mInsetObserver;

    private DisplayCutoutTabHelper mDisplayCutoutTabHelper;
    private DisplayCutoutController mController;

    private WeakReference<Activity> mActivityRef;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mActivityRef = new WeakReference<>(mChromeActivity);

        when(mChromeActivity.getWindow()).thenReturn(mWindow);
        when(mWindow.getAttributes()).thenReturn(new LayoutParams());
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroid);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.isFullscreenForCurrentTab()).thenReturn(true);
        when(mWindowAndroid.getActivity()).thenReturn(mActivityRef);

        InsetObserverViewSupplier.setInstanceForTesting(mInsetObserver);
        ActivityDisplayCutoutModeSupplier.setInstanceForTesting(0);

        mDisplayCutoutTabHelper = spy(new DisplayCutoutTabHelper(mTab));
        mController = spy(mDisplayCutoutTabHelper.mCutoutController);
        mDisplayCutoutTabHelper.mCutoutController = mController;
    }

    @Test
    @SmallTest
    public void testViewportFitUpdate() {
        verify(mController, never()).maybeUpdateLayout();

        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.COVER);
        verify(mController).maybeUpdateLayout();
    }

    @Test
    @SmallTest
    public void testViewportFitUpdateNotChanged() {
        verify(mController, never()).maybeUpdateLayout();

        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.AUTO);
        verify(mController, never()).maybeUpdateLayout();
    }

    @Test
    @SmallTest
    public void testCutoutModeWhenAutoAndInteractable() {
        when(mTab.isUserInteractable()).thenReturn(true);

        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.AUTO);
        Assert.assertEquals(LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT,
                mController.computeDisplayCutoutMode());
    }

    @Test
    @SmallTest
    public void testCutoutModeWhenCoverAndInteractable() {
        when(mTab.isUserInteractable()).thenReturn(true);

        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.COVER);
        Assert.assertEquals(LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES,
                mController.computeDisplayCutoutMode());
    }

    @Test
    @SmallTest
    public void testCutoutModeWhenCoverForcedAndInteractable() {
        when(mTab.isUserInteractable()).thenReturn(true);

        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.COVER_FORCED_BY_USER_AGENT);
        Assert.assertEquals(LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES,
                mController.computeDisplayCutoutMode());
    }

    @Test
    @SmallTest
    public void testCutoutModeWhenContainAndInteractable() {
        when(mTab.isUserInteractable()).thenReturn(true);

        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.CONTAIN);
        Assert.assertEquals(LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER,
                mController.computeDisplayCutoutMode());
    }

    @Test
    @SmallTest
    public void testCutoutModeWhenAutoAndNotInteractable() {
        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.AUTO);
        Assert.assertEquals(LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT,
                mController.computeDisplayCutoutMode());
    }

    @Test
    @SmallTest
    public void testCutoutModeWhenCoverAndNotInteractable() {
        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.COVER);
        Assert.assertEquals(LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT,
                mController.computeDisplayCutoutMode());
    }

    @Test
    @SmallTest
    public void testCutoutModeWhenCoverForcedAndNotInteractable() {
        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.COVER_FORCED_BY_USER_AGENT);
        Assert.assertEquals(LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT,
                mController.computeDisplayCutoutMode());
    }

    @Test
    @SmallTest
    public void testCutoutModeWhenContainAndNotInteractable() {
        mDisplayCutoutTabHelper.setViewportFit(ViewportFit.CONTAIN);
        Assert.assertEquals(LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT,
                mController.computeDisplayCutoutMode());
    }

    @Test
    @SmallTest
    public void testLayoutOnInteractability_True() {
        // In this test we are checking for a side effect of maybeUpdateLayout.
        // This is because the tab observer holds a reference to the original
        // mDisplayCutoutTabHelper and not the spied one.
        verify(mTab).addObserver(mTabObserverCaptor.capture());
        reset(mTab);

        mTabObserverCaptor.getValue().onInteractabilityChanged(mTab, true);
        verify(mWindow).getAttributes();
    }

    @Test
    @SmallTest
    public void testLayoutOnInteractability_False() {
        // In this test we are checking for a side effect of maybeUpdateLayout.
        // This is because the tab observer holds a reference to the original
        // mDisplayCutoutTabHelper and not the spied one.
        verify(mTab).addObserver(mTabObserverCaptor.capture());
        reset(mTab);

        mTabObserverCaptor.getValue().onInteractabilityChanged(mTab, false);
        verify(mWindow).getAttributes();
    }

    @Test
    @SmallTest
    public void testLayout_NoWindow() {
        // Verify there's no crash when the tab's interactability changes after activity detachment.
        verify(mTab).addObserver(mTabObserverCaptor.capture());
        reset(mTab);

        mTabObserverCaptor.getValue().onActivityAttachmentChanged(mTab, null);
        mTabObserverCaptor.getValue().onInteractabilityChanged(mTab, false);
        verify(mWindow, never()).getAttributes();
    }

    @Test
    @SmallTest
    public void testLayoutOnShown() {
        // In this test we are checking for a side effect of maybeUpdateLayout.
        // This is because the tab observer holds a reference to the original
        // mDisplayCutoutTabHelper and not the spied one.
        verify(mTab).addObserver(mTabObserverCaptor.capture());
        reset(mTab);

        mTabObserverCaptor.getValue().onShown(mTab, TabSelectionType.FROM_NEW);
        verify(mWindow).getAttributes();
    }
}
