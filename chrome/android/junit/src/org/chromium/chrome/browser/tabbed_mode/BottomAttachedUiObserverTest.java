// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.graphics.Color;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;

@RunWith(BaseRobolectricTestRunner.class)
public class BottomAttachedUiObserverTest {

    private BottomAttachedUiObserver mBottomAttachedUiObserver;
    private MockColorChangeObserver mColorChangeObserver;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mBottomAttachedUiObserver = new BottomAttachedUiObserver(mBrowserControlsStateProvider);
        mColorChangeObserver = new MockColorChangeObserver();
        mBottomAttachedUiObserver.addObserver(mColorChangeObserver);
    }

    @Test
    public void testAdaptsColorToBrowserControls() {
        int bottomControlsHeight = 100;

        mColorChangeObserver.assertColor(null);

        // Show bottom controls.
        mBottomAttachedUiObserver.onBottomControlsBackgroundColorChanged(Color.RED);
        mBottomAttachedUiObserver.onBottomControlsHeightChanged(bottomControlsHeight, 0);
        mColorChangeObserver.assertColor(Color.RED);

        // Scroll off bottom controls partway.
        mBottomAttachedUiObserver.onControlsOffsetChanged(0, 0, bottomControlsHeight / 2, 0, false);
        mColorChangeObserver.assertColor(Color.RED);

        // Scroll off bottom controls fully.
        mBottomAttachedUiObserver.onControlsOffsetChanged(0, 0, bottomControlsHeight, 0, false);
        mColorChangeObserver.assertColor(null);

        // Scroll bottom controls back.
        mBottomAttachedUiObserver.onControlsOffsetChanged(0, 0, 0, 0, false);
        mColorChangeObserver.assertColor(Color.RED);

        // Hide bottom controls.
        mBottomAttachedUiObserver.onBottomControlsHeightChanged(0, 0);
        mColorChangeObserver.assertColor(null);
    }

    @Test
    public void testDestroy() {
        mBottomAttachedUiObserver.destroy();
        verify(mBrowserControlsStateProvider).removeObserver(eq(mBottomAttachedUiObserver));
    }

    private static class MockColorChangeObserver implements BottomAttachedUiObserver.Observer {
        private @Nullable @ColorInt Integer mColor;

        @Override
        public void onBottomAttachedColorChanged(@Nullable Integer color) {
            mColor = color;
        }

        public void assertColor(@Nullable @ColorInt Integer expectedColor) {
            assertEquals("Incorrect bottom attached color.", expectedColor, mColor);
        }
    }
}
