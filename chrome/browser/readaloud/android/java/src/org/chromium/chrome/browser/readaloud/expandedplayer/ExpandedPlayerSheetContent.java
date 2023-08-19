// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.expandedplayer;

import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

public class ExpandedPlayerSheetContent implements BottomSheetContent {
    private static final String TAG = "RAPlayerSheet";
    private static final float DEFAULT_INITIAL_SPEED = 1f;
    // Note: if these times need to change, the "back 10" and "forward 30" icons
    // should also be changed.
    private static final int BACK_SECONDS = 10;
    private static final int FORWARD_SECONDS = 30;

    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private View mContentView;

    // TODO remove hard-coded strings
    @SuppressWarnings("SetTextI18n")
    public ExpandedPlayerSheetContent(
            Context context, BottomSheetController bottomSheetController) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mContentView = LayoutInflater.from(context).inflate(
                R.layout.readaloud_expanded_player_layout, null);
        ((TextView) mContentView.findViewById(R.id.readaloud_expanded_player_title))
                .setText("Page title");
        ((TextView) mContentView.findViewById(R.id.readaloud_expanded_player_publisher))
                .setText("Site");
        Resources res = mContext.getResources();
        mContentView.findViewById(R.id.readaloud_seek_back_button)
                .setContentDescription(res.getString(R.string.readaloud_replay, BACK_SECONDS));
        mContentView.findViewById(R.id.readaloud_seek_forward_button)
                .setContentDescription(res.getString(R.string.readaloud_forward, FORWARD_SECONDS));
        ((TextView) mContentView.findViewById(R.id.readaloud_player_time)).setText("00:00");
        ((TextView) mContentView.findViewById(R.id.readaloud_player_duration)).setText("00:00");
        setSpeed(DEFAULT_INITIAL_SPEED);
    }

    public void show() {
        mBottomSheetController.requestShowContent(this, /*animate=*/true);
    }

    public void hide() {
        mBottomSheetController.hideContent(this, /*animate=*/true);
    }

    void setCloseButtonHandler(View.OnClickListener onClick) {
        mContentView.findViewById(R.id.readaloud_expanded_player_close_button)
                .setOnClickListener(onClick);
    }

    @SuppressWarnings({"SetTextI18n", "DefaultLocale"})
    public void setSpeed(float speed) {
        TextView speedButton = (TextView) mContentView.findViewById(R.id.readaloud_playback_speed);
        speedButton.setText(String.format("%.1fx", speed));
        speedButton.setContentDescription(mContext.getResources().getString(
                R.string.readaloud_speed_menu_button, String.format("%.1f", speed)));
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
        Log.e(TAG,
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
}
