// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import android.content.Context;
import android.content.res.Resources;
import android.text.format.DateUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.chrome.browser.readaloud.player.InteractionHandler;
import org.chromium.chrome.browser.readaloud.player.PlayerProperties;
import org.chromium.chrome.browser.readaloud.player.R;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

public class ExpandedPlayerSheetContent implements BottomSheetContent {
    private static final String TAG = "RAPlayerSheet";
    // Note: if these times need to change, the "back 10" and "forward 30" icons
    // should also be changed.
    private static final int BACK_SECONDS = 10;
    private static final int FORWARD_SECONDS = 30;

    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final PropertyModel mModel;
    private final SeekBar mSeekBar;
    private View mContentView;
    // Effectively final and non null, can be null only in tests
    private OptionsMenuSheetContent mOptionsMenu;
    private SpeedMenuSheetContent mSpeedMenu;
    private TextView mSpeedButton;
    private boolean mHighlightingEnabled;
    private boolean mHighlightingSupported;

    private LinearLayout mNormalLayout;
    private LinearLayout mErrorLayout;

    public ExpandedPlayerSheetContent(
            Context context, BottomSheetController bottomSheetController, PropertyModel model) {
        this(
                context,
                bottomSheetController,
                LayoutInflater.from(context)
                        .inflate(R.layout.readaloud_expanded_player_layout, null),
                model);
        mOptionsMenu =
                new OptionsMenuSheetContent(
                        mContext, /* parent= */ this, mBottomSheetController, mModel);
        mSpeedMenu =
                new SpeedMenuSheetContent(
                        mContext, /* parent= */ this, mBottomSheetController, mModel);
    }

    @VisibleForTesting
    ExpandedPlayerSheetContent(
            Context context,
            BottomSheetController bottomSheetController,
            View contentView,
            PropertyModel model) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mContentView = contentView;
        mModel = model;
        Resources res = mContext.getResources();
        mSpeedButton = (TextView) mContentView.findViewById(R.id.readaloud_playback_speed);
        mContentView
                .findViewById(R.id.readaloud_seek_back_button)
                .setContentDescription(res.getString(R.string.readaloud_replay, BACK_SECONDS));
        mContentView
                .findViewById(R.id.readaloud_seek_forward_button)
                .setContentDescription(res.getString(R.string.readaloud_forward, FORWARD_SECONDS));
        mNormalLayout = (LinearLayout) mContentView.findViewById(R.id.normal_layout);
        mErrorLayout = (LinearLayout) mContentView.findViewById(R.id.error_layout);
        mSeekBar = (SeekBar) mContentView.findViewById(R.id.readaloud_expanded_player_seek_bar);
    }

    public void onPlaybackStateChanged(@PlaybackListener.State int state) {
        setPlaying(state == PlaybackListener.State.PLAYING);
        if (state == PlaybackListener.State.ERROR) {
            showOnly(mErrorLayout);
        } else {
            showOnly(mNormalLayout);
        }
    }

    // Show `layout` and hide the other layouts.
    private void showOnly(LinearLayout layout) {
        setVisibleIfMatch(mNormalLayout, layout);
        setVisibleIfMatch(mErrorLayout, layout);
    }

    private static void setVisibleIfMatch(LinearLayout a, LinearLayout b) {
        a.setVisibility(a == b ? View.VISIBLE : View.GONE);
    }

    public void show() {
        mBottomSheetController.requestShowContent(this, /* animate= */ true);
    }

    public void hide() {
        mBottomSheetController.hideContent(this, /* animate= */ true);
    }

    void setTitle(String title) {
        ((TextView) mContentView.findViewById(R.id.readaloud_expanded_player_title)).setText(title);
    }

    void setPublisher(String publisher) {
        ((TextView) mContentView.findViewById(R.id.readaloud_expanded_player_publisher))
                .setText(publisher);
    }

    void setElapsed(Long nanos) {
        ((TextView) mContentView.findViewById(R.id.readaloud_player_time))
                .setText(formatTimeNanos(nanos));
    }

    void setDuration(Long nanos) {
        ((TextView) mContentView.findViewById(R.id.readaloud_player_duration))
                .setText(formatTimeNanos(nanos));
    }

    private static String formatTimeNanos(long nanos) {
        if (nanos <= 0) {
            return DateUtils.formatElapsedTime(0);
        }
        final long nanosPerSecond = 1_000_000_000L;
        long seconds = nanos / nanosPerSecond;
        return DateUtils.formatElapsedTime(seconds);
    }

    void setInteractionHandler(InteractionHandler handler) {
        setOnClickListener(R.id.readaloud_play_pause_button, handler::onPlayPauseClick);
        setOnClickListener(R.id.readaloud_seek_back_button, handler::onSeekBackClick);
        setOnClickListener(R.id.readaloud_seek_forward_button, handler::onSeekForwardClick);
        setOnClickListener(R.id.readaloud_expanded_player_publisher, handler::onPublisherClick);
        setOnClickListener(R.id.readaloud_playback_speed, this::showSpeedMenu);
        setOnClickListener(R.id.readaloud_more_button, this::showOptionsMenu);

        SeekBar seekBar =
                (SeekBar) mContentView.findViewById(R.id.readaloud_expanded_player_seek_bar);
        seekBar.setOnSeekBarChangeListener(handler.getSeekBarChangeListener());
        mSpeedMenu.setInteractionHandler(handler);
        mOptionsMenu.setInteractionHandler(handler);
    }

    public void setSpeed(float speed) {
        mModel.set(PlayerProperties.SPEED, speed);
        String speedString = SpeedMenuSheetContent.speedFormatter(speed);
        mSpeedButton.setText(
                mContext.getResources().getString(R.string.readaloud_speed, speedString));
        mSpeedButton.setContentDescription(
                mContext.getResources()
                        .getString(R.string.readaloud_speed_menu_button, speedString));
    }

    void setHighlightingSupported(boolean supported) {
        mOptionsMenu.setHighlightingSupported(supported);
    }

    void setHighlightingEnabled(boolean enabled) {
        mOptionsMenu.setHighlightingEnabled(enabled);
    }

    public void setPlaying(boolean playing) {
        ImageView playButton =
                (ImageView) mContentView.findViewById(R.id.readaloud_play_pause_button);
        // If playing, update to show the pause button.
        if (playing) {
            playButton.setImageResource(R.drawable.pause_button);
            playButton.setContentDescription(
                    mContext.getResources().getString(R.string.readaloud_pause));
        } else {
            playButton.setImageResource(R.drawable.play_button);
            playButton.setContentDescription(
                    mContext.getResources().getString(R.string.readaloud_play));
        }
    }

    /**
     * @param percentProgress out of 1.0
     */
    public void setProgress(float percent) {
        mSeekBar.setProgress((int) (percent * mSeekBar.getMax()), true);
    }

    @Nullable
    OptionsMenuSheetContent getOptionsMenu() {
        return mOptionsMenu;
    }

    public void showOptionsMenu() {
        mBottomSheetController.hideContent(this, /* animate= */ false);
        mBottomSheetController.requestShowContent(mOptionsMenu, /* animate= */ true);
    }

    @Nullable
    VoiceMenuSheetContent getVoiceMenu() {
        if (mOptionsMenu == null) {
            return null;
        }
        return mOptionsMenu.getVoiceMenu();
    }

    public void notifySheetClosed(BottomSheetContent contentClosed) {
        mOptionsMenu.notifySheetClosed(contentClosed);
        mSpeedMenu.notifySheetClosed(contentClosed);
    }

    public void showSpeedMenu() {
        mBottomSheetController.hideContent(this, /* animate= */ false);
        mBottomSheetController.requestShowContent(mSpeedMenu, /* animate= */ true);
    }

    // BottomSheetContent implementation

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    @Nullable
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {}

    @Override
    @ContentPriority
    public int getPriority() {
        // The player is persistent. If another bottom sheet wants to show, this one
        // should hide temporarily.
        return BottomSheetContent.ContentPriority.LOW;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        // The user can dismiss the expanded player by swiping it.
        return true;
    }

    @Override
    public boolean hasCustomLifecycle() {
        // Dismiss if the user navigates the page, switches tabs, or changes layout.
        return false;
    }

    @Override
    public boolean hasCustomScrimLifecycle() {
        // Don't show a scrim when open (gray overlay on page).
        return true;
    }

    @Override
    public int getPeekHeight() {
        // Only full height mode enabled.
        return HeightMode.DISABLED;
    }

    @Override
    public float getHalfHeightRatio() {
        // Only full height mode enabled.
        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        // "Read Aloud player."
        // Automatically appended: "Swipe down to close."
        return R.string.readaloud_player_name;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        Log.e(
                TAG,
                "Tried to get half height accessibility string, but half height isn't supported.");
        assert false;
        return 0;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        // "Read Aloud player opened at full height."
        return R.string.readaloud_player_opened_at_full_height;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        // "Read Aloud player minimized."
        return R.string.readaloud_player_minimized;
    }

    private void setOnClickListener(int id, Runnable onClick) {
        mContentView
                .findViewById(id)
                .setOnClickListener(
                        (view) -> {
                            onClick.run();
                        });
    }

    @VisibleForTesting
    public void setOptionsMenuSheetContent(OptionsMenuSheetContent optionsMenu) {
        mOptionsMenu = optionsMenu;
    }

    @VisibleForTesting
    public void setSpeedMenuSheetContent(SpeedMenuSheetContent speedMenu) {
        mSpeedMenu = speedMenu;
    }
}
