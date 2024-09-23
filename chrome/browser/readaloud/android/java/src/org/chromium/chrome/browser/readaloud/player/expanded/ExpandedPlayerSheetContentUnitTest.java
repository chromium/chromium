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
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
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
import org.chromium.chrome.browser.readaloud.player.PlayerProperties;
import org.chromium.chrome.browser.readaloud.player.R;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Locale;

/** Unit tests for {@link ExpandedPlayerSheetContent}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ExpandedPlayerSheetContentUnitTest {
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private InteractionHandler mInteractionHandler;
    @Mock private PropertyModel mModel;
    @Mock private OptionsMenuSheetContent mOptionsMenu;
    @Mock private SpeedMenuSheetContent mSpeedMenu;
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
    private SeekBar mSeekbar;
    private View mContentView;
    private Activity mActivity;
    private LinearLayout mNormalLayout;
    private LinearLayout mErrorLayout;

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
        mSpeedView = (TextView) mContentView.findViewById(R.id.readaloud_playback_speed);
        mBackButton = (ImageView) mContentView.findViewById(R.id.readaloud_seek_back_button);
        mForwardButton = (ImageView) mContentView.findViewById(R.id.readaloud_seek_forward_button);
        mPlayPauseButton = (ImageView) mContentView.findViewById(R.id.readaloud_play_pause_button);
        mSeekbar = (SeekBar) mContentView.findViewById(R.id.readaloud_expanded_player_seek_bar);
        mNormalLayout = (LinearLayout) mContentView.findViewById(R.id.normal_layout);
        mErrorLayout = (LinearLayout) mContentView.findViewById(R.id.error_layout);
        mContent =
                new ExpandedPlayerSheetContent(
                        mActivity, mBottomSheetController, mContentView, mModel);
        mContent.setOptionsMenuSheetContent(mOptionsMenu);
        mContent.setSpeedMenuSheetContent(mSpeedMenu);
        // PlayerMediator is responsible for setting initial speed.
        mContent.setSpeed(1f);
    }

    @Test
    public void verifyInitialA11yStrings() {
        assertEquals("1x", mSpeedView.getText());
        assertEquals("1x increase/decrease speed.", mSpeedView.getContentDescription());
        assertEquals("Go back 10 seconds", mBackButton.getContentDescription());
        assertEquals("Fast forward 10 seconds", mForwardButton.getContentDescription());
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
    public void testSetElapsed() {
        mContent.setElapsed(1_000_000_000L * 20);
        assertEquals(
                "00:20",
                ((TextView) mContentView.findViewById(R.id.readaloud_player_time)).getText());
        mContent.setElapsed(1_000_000_000L * 123);
        assertEquals(
                "02:03",
                ((TextView) mContentView.findViewById(R.id.readaloud_player_time)).getText());

        mContent.setElapsed(1_000_000_000L * -30);
        assertEquals(
                "00:00",
                ((TextView) mContentView.findViewById(R.id.readaloud_player_time)).getText());
    }

    @Test
    public void testSetDuration() {
        mContent.setDuration(1_000_000_000L * 20);
        assertEquals(
                "00:20",
                ((TextView) mContentView.findViewById(R.id.readaloud_player_duration)).getText());
        mContent.setDuration(1_000_000_000L * 12345);
        assertEquals(
                "3:25:45",
                ((TextView) mContentView.findViewById(R.id.readaloud_player_duration)).getText());
        mContent.setDuration(1_000_000_000L * -100);
        assertEquals(
                "00:00",
                ((TextView) mContentView.findViewById(R.id.readaloud_player_duration)).getText());
    }

    @Test
    public void testSetSpeed() {
        mContent.setSpeed(0.5f);
        assertEquals("0.5x", mSpeedView.getText().toString());
        assertEquals("0.5x increase/decrease speed.", mSpeedView.getContentDescription());

        mContent.setSpeed(2f);
        assertEquals("2x", mSpeedView.getText());
        assertEquals("2x increase/decrease speed.", mSpeedView.getContentDescription());
    }

    @Test
    public void testSetSpeed_languages() {
        Locale.setDefault(new Locale("es", "ES"));
        mContent.setSpeed(0.5f);
        assertEquals("0,5x", mSpeedView.getText().toString());
    }

    @Test
    public void testSetPlaying() {
        mContent.setPlaying(true);
        assertEquals("Pause", mPlayPauseButton.getContentDescription());

        mContent.setPlaying(false);
        assertEquals("Play", mPlayPauseButton.getContentDescription());
    }

    @Test
    public void testSetProgress() {
        mContent.setProgress(0.5f);
        assertEquals(mSeekbar.getProgress(), (int) (mSeekbar.getMax() * 0.5f));

        mContent.setProgress(0.75f);
        assertEquals(mSeekbar.getProgress(), (int) (mSeekbar.getMax() * 0.75f));
    }

    @Test
    public void testOnPlaybackStateChanged() {
        mContent.onPlaybackStateChanged(PlaybackListener.State.ERROR);
        assertTrue(mErrorLayout.getVisibility() == View.VISIBLE);
        assertTrue(mNormalLayout.getVisibility() == View.GONE);

        mContent.onPlaybackStateChanged(PlaybackListener.State.PLAYING);
        assertTrue(mErrorLayout.getVisibility() == View.GONE);
        assertTrue(mNormalLayout.getVisibility() == View.VISIBLE);
        assertEquals(mPlayPauseButton.getContentDescription(), "Pause");

        mContent.onPlaybackStateChanged(PlaybackListener.State.PAUSED);
        assertTrue(mErrorLayout.getVisibility() == View.GONE);
        assertTrue(mNormalLayout.getVisibility() == View.VISIBLE);
        assertEquals(mPlayPauseButton.getContentDescription(), "Play");
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
        verify(mModel).set(PlayerProperties.SHOW_MINI_PLAYER_ON_DISMISS, false);
        verify(mBottomSheetController).hideContent(mContent, false);
        verify(mBottomSheetController).requestShowContent(mOptionsMenu, false);
    }

    @Test
    public void testShowSpeedMenu() {
        mContent.showSpeedMenu();
        verify(mModel).set(PlayerProperties.SHOW_MINI_PLAYER_ON_DISMISS, false);
        verify(mBottomSheetController).hideContent(mContent, false);
        verify(mBottomSheetController).requestShowContent(mSpeedMenu, false);
    }

    @Test
    public void testNotifySheetClosed() {
        mContent.notifySheetClosed(mContent);
        verify(mOptionsMenu).notifySheetClosed(eq(mContent));
        verify(mSpeedMenu).notifySheetClosed(eq(mContent));
    }

    @Test
    public void testCanSuppressInAnyState() {
        assertTrue(mContent.canSuppressInAnyState());
    }

    @Test
    public void testGetVerticalScrollOffset() {
        ScrollView scrollView = mContentView.findViewById(R.id.scroll_view);
        scrollView.setPadding(0, 100, 0, 100);
        scrollView.scrollTo(0, 100);
        assertEquals(100, mContent.getVerticalScrollOffset());
    }
}
