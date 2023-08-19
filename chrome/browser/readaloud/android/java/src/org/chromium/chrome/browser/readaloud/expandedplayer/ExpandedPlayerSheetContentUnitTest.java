// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.expandedplayer;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Unit tests for {@link ExpandedPlayerSheetContent}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ExpandedPlayerSheetContentUnitTest {
    @Mock
    private BottomSheetController mBottomSheetController;

    private Context mContext;
    private Drawable mPlayDrawable;
    private Drawable mPauseDrawable;
    private ExpandedPlayerSheetContent mContent;
    private TextView mSpeedView;
    private ImageView mBackButton;
    private ImageView mForwardButton;
    private ImageView mPlayPauseButton;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = ApplicationProvider.getApplicationContext();
        mPlayDrawable = mContext.getDrawable(R.drawable.play_button);
        mPauseDrawable = mContext.getDrawable(R.drawable.pause_button);
        mContent = new ExpandedPlayerSheetContent(mContext, mBottomSheetController);
        final View contentView = mContent.getContentView();
        mSpeedView = (TextView) contentView.findViewById(R.id.readaloud_playback_speed);
        mBackButton = (ImageView) contentView.findViewById(R.id.readaloud_seek_back_button);
        mForwardButton = (ImageView) contentView.findViewById(R.id.readaloud_seek_forward_button);
        mPlayPauseButton = (ImageView) contentView.findViewById(R.id.readaloud_play_pause_button);
    }

    @Test
    public void verifyInitialA11yStrings() {
        assertEquals("1.0x", mSpeedView.getText());
        assertEquals("Playback speed: 1.0. Click to change.", mSpeedView.getContentDescription());
        assertEquals("Back 10 seconds", mBackButton.getContentDescription());
        assertEquals("Forward 30 seconds", mForwardButton.getContentDescription());
    }

    @Test
    public void testSetSpeed() {
        mContent.setSpeed(0.5f);
        assertEquals("0.5x", mSpeedView.getText());
        assertEquals("Playback speed: 0.5. Click to change.", mSpeedView.getContentDescription());

        mContent.setSpeed(2f);
        assertEquals("2.0x", mSpeedView.getText());
        assertEquals("Playback speed: 2.0. Click to change.", mSpeedView.getContentDescription());
    }

    @Test
    public void testSetPlaying() {
        mContent.setPlaying(true);
        assertEquals("Pause", mPlayPauseButton.getContentDescription());

        mContent.setPlaying(false);
        assertEquals("Play", mPlayPauseButton.getContentDescription());
    }

    @Test
    public void testShow() {
        mContent.show();
        verify(mBottomSheetController, times(1)).requestShowContent(eq(mContent), eq(true));
    }

    @Test
    public void testHide() {
        mContent.hide();
        verify(mBottomSheetController, times(1)).hideContent(eq(mContent), eq(true));
    }
}
