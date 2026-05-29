// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.messages;

import static org.mockito.Mockito.when;

import android.content.res.Resources;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.components.messages.MessageContainer;

/** Unit tests for {@link MessageContainerCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MessageContainerCoordinatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private MessageContainer mContainer;
    @Mock private BrowserControlsManager mControlsManager;
    @Mock private Resources mResources;

    private MessageContainerCoordinator mCoordinator;
    private static final int BUBBLE_INSET = 10;

    @Before
    public void setUp() {
        when(mContainer.getResources()).thenReturn(mResources);
        when(mResources.getDimensionPixelOffset(R.dimen.message_bubble_inset))
                .thenReturn(BUBBLE_INSET);
        mCoordinator = new MessageContainerCoordinator(mContainer, mControlsManager);
    }

    @Test
    public void testGetMessageTopOffset_withContentOffset() {
        when(mControlsManager.getContentOffset()).thenReturn(100);

        int offset = mCoordinator.getMessageTopOffset();
        Assert.assertEquals(100 - BUBBLE_INSET, offset);
    }

    @Test
    public void testGetMessageTopOffset_withZeroContentOffsetAndTopPosition() {
        when(mControlsManager.getContentOffset()).thenReturn(0);
        when(mControlsManager.getControlsPosition())
                .thenReturn(BrowserControlsStateProvider.ControlsPosition.TOP);
        when(mControlsManager.getTopControlsHeight()).thenReturn(80);
        when(mControlsManager.isVisibilityForced()).thenReturn(true);

        int offset = mCoordinator.getMessageTopOffset();
        Assert.assertEquals(80 - BUBBLE_INSET, offset);
    }

    @Test
    public void testGetMessageTopOffset_withZeroContentOffsetAndTopPosition_notForced() {
        when(mControlsManager.getContentOffset()).thenReturn(0);
        when(mControlsManager.getControlsPosition())
                .thenReturn(BrowserControlsStateProvider.ControlsPosition.TOP);
        when(mControlsManager.isVisibilityForced()).thenReturn(false);

        int offset = mCoordinator.getMessageTopOffset();
        Assert.assertEquals(0, offset);
    }

    @Test
    public void testGetMessageTopOffset_withZeroContentOffsetAndBottomPosition() {
        when(mControlsManager.getContentOffset()).thenReturn(0);
        when(mControlsManager.getControlsPosition())
                .thenReturn(BrowserControlsStateProvider.ControlsPosition.BOTTOM);

        int offset = mCoordinator.getMessageTopOffset();
        Assert.assertEquals(0, offset);
    }
}
