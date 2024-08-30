// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.readaloud.player.InteractionHandler;
import org.chromium.chrome.browser.readaloud.player.PlayerProperties;
import org.chromium.chrome.browser.readaloud.player.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link SpeedMenuSheetContent}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SpeedMenuSheetContentUnitTest {
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private ExpandedPlayerSheetContent mBottomSheetContent;
    @Mock private InteractionHandler mHandler;
    private PropertyModel mModel;
    private Activity mActivity;
    private Menu mMenu;
    private SpeedMenuSheetContent mContent;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        // Need to set theme before inflating layout.
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mModel = new PropertyModel.Builder(PlayerProperties.ALL_KEYS).build();
        mModel.set(PlayerProperties.SPEED, 1.0f);
        mContent =
                new SpeedMenuSheetContent(
                        mActivity, mBottomSheetContent, mBottomSheetController, mModel);
        mMenu = mContent.getMenuForTesting();
    }

    @Test
    public void testSetup() {
        assertEquals(
                ((TextView) mMenu.getItem(0).findViewById(R.id.item_label)).getText().toString(),
                "0.5x");
        assertEquals(
                ((TextView) mMenu.getItem(7).findViewById(R.id.item_label)).getText().toString(),
                "4x");
    }

    @Test
    public void testOnClick() {
        mContent.setInteractionHandler(mHandler);
        assertTrue(mMenu.getItem(0).getChildAt(0).performClick());
        verify(mHandler).onSpeedChange(0.5f);

        assertTrue(mMenu.getItem(7).getChildAt(0).performClick());
        verify(mHandler).onSpeedChange(4.0f);
    }

    @Test
    public void testSetInteractionHandler() {
        mContent.setInteractionHandler(mHandler);
        assertTrue(mMenu.getItem(0).getChildAt(0).performClick());
        verify(mHandler).onSpeedChange(0.5f);
    }

    @Test
    public void testResetScrollOnSheetClosed() {
        // Scroll down a bit
        mContent.getMenuForTesting().getScrollView().scrollTo(0, 10);

        // Closing the sheet should cause the content scroll position to be reset
        mContent.notifySheetClosed(mContent);
        assertEquals(0, mContent.getMenuForTesting().getScrollView().getScrollY());
        assertEquals(0, mContent.getVerticalScrollOffset());
    }
}
