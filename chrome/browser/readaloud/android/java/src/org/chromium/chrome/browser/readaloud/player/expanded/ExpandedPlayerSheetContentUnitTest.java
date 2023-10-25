// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.readaloud.player.InteractionHandler;
import org.chromium.chrome.browser.readaloud.player.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link ExpandedPlayerSheetContent}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ExpandedPlayerSheetContentUnitTest {
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private InteractionHandler mInteractionHandler;
    @Mock private PropertyModel mModel;
    @Mock private OptionsMenuSheetContent mOptionsMenu;
    @Mock private View.OnClickListener mOnClickListener;

    private Context mContext;
    private Drawable mPlayDrawable;
    private Drawable mPauseDrawable;
    private ExpandedPlayerSheetContent mContent;
    private TextView mSpeedView;
    private TextView mTitleView;
    private TextView mPublisherView;
    private ImageView mBackButton;
    private ImageView mForwardButton;
    private ImageView mPlayPauseButton;
    private View mContentView;
    private Activity mActivity;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = ApplicationProvider.getApplicationContext();
        mPlayDrawable = mContext.getDrawable(R.drawable.play_button);
        mPauseDrawable = mContext.getDrawable(R.drawable.pause_button);
        mActivity = Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        // Need to set theme before inflating layout.
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mContentView =
                mActivity
                        .getLayoutInflater()
                        .inflate(R.layout.readaloud_expanded_player_layout, null);
        mSpeedView = (TextView) mContentView.findViewById(R.id.readaloud_playback_speed);
        mTitleView = (TextView) mContentView.findViewById(R.id.readaloud_expanded_player_title);
        mPublisherView =
                (TextView) mContentView.findViewById(R.id.readaloud_expanded_player_publisher);
        mBackButton = (ImageView) mContentView.findViewById(R.id.readaloud_seek_back_button);
        mForwardButton = (ImageView) mContentView.findViewById(R.id.readaloud_seek_forward_button);
        mPlayPauseButton = (ImageView) mContentView.findViewById(R.id.readaloud_play_pause_button);
        mContent =
                new ExpandedPlayerSheetContent(
                        mContext, mBottomSheetController, mContentView, mModel);
        mContent.setOptionsMenu(mOptionsMenu);
    }

    @Test
    public void verifyInitialA11yStrings() {
        assertEquals("1.0x", mSpeedView.getText());
        assertEquals("Playback speed: 1.0. Click to change.", mSpeedView.getContentDescription());
        assertEquals("Back 10 seconds", mBackButton.getContentDescription());
        assertEquals("Forward 30 seconds", mForwardButton.getContentDescription());
    }

    @Test
    public void testSetInteractionHandler() {
        mContent.setInteractionHandler(mInteractionHandler);

        assertTrue(mBackButton.performClick());
        verify(mInteractionHandler).onSeekBackClick();

        assertTrue(mForwardButton.performClick());
        verify(mInteractionHandler).onSeekForwardClick();

        assertTrue(mPlayPauseButton.performClick());
        verify(mInteractionHandler).onPlayPauseClick();
        verify(mInteractionHandler).onSeekForwardClick();

        assertTrue(mPublisherView.performClick());
        verify(mInteractionHandler).onPublisherClick();
    }

    @Test
    public void testSetTitleAndPublisher() {
        mContent.setTitle("title test");
        assertEquals("title test", mTitleView.getText());
        mContent.setPublisher("publisher test");
        assertEquals("publisher test", mPublisherView.getText());
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

    @Test
    public void testGetOptionsMenu() {
        assertEquals(mContent.getOptionsMenu(), mOptionsMenu);
    }

    @Test
    public void testShowOptionsMenu() {
        mContent.showOptionsMenu();
        verify(mBottomSheetController).hideContent(mContent, false);
        verify(mBottomSheetController).requestShowContent(mContent.getOptionsMenu(), true);
    }

    @Test
    public void testNotifySheetClosed() {
        mContent.notifySheetClosed();
        verify(mOptionsMenu).notifySheetClosed();
    }
}
