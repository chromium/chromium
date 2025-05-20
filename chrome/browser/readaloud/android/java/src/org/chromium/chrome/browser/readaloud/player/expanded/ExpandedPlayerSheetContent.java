// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.text.format.DateUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.TextView;

import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.readaloud.ReadAloudFeatures;
import org.chromium.chrome.browser.readaloud.player.Colors;
import org.chromium.chrome.browser.readaloud.player.InteractionHandler;
import org.chromium.chrome.browser.readaloud.player.PlayerProperties;
import org.chromium.chrome.browser.readaloud.player.R;
import org.chromium.chrome.browser.readaloud.player.TouchDelegateUtil;
import org.chromium.chrome.modules.readaloud.Feedback.FeedbackType;
import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackMode;
import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackModeSelectionEnablementStatus;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.Toast;

@NullMarked
public class ExpandedPlayerSheetContent implements BottomSheetContent {
    private static final String TAG = "RAPlayerSheet";
    // Note: if these times need to change, the "back 10" and "forward 10" icons
    // should also be changed.
    private static final int BACK_SECONDS = 10;
    private static final int FORWARD_SECONDS = 10;

    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final PropertyModel mModel;
    private final SeekBar mSeekBar;
    private final ScrollView mScrollView;
    private final LinearLayout mPlayerControls;
    private final ImageView mModeSelectorButton;
    private boolean mIsModeActive;
    private final View mContentView;
    // Effectively final and non null, can be null only in tests
    private OptionsMenuSheetContent mOptionsMenu;
    private final NegativeFeedbackMenuSheetContent mNegativeFeedbackMenu;
    private SpeedMenuSheetContent mSpeedMenu;
    private final TextView mSpeedButton;

    private final TextView mLoadingTextView;

    private final ImageView mPlayPauseButton;
    private final ImageView mRewindButton;
    private final ImageView mForwardButton;

    private final ImageView mMoreOptionsButton;
    private final ImageView mThumbUpButton;
    private final ImageView mThumbDownButton;

    private final LinearLayout mNormalLayout;
    private final LinearLayout mErrorLayout;
    private final LinearLayout mLoadingLayout;

    private final PlaybackModeIphController mPlaybackModeIphController;

    public ExpandedPlayerSheetContent(
            Context context,
            BottomSheetController bottomSheetController,
            PropertyModel model,
            PlaybackModeIphController playbackModeIphController) {
        this(
                context,
                bottomSheetController,
                LayoutInflater.from(context)
                        .inflate(R.layout.readaloud_expanded_player_layout, null),
                model,
                playbackModeIphController);
    }

    @VisibleForTesting
    ExpandedPlayerSheetContent(
            Context context,
            BottomSheetController bottomSheetController,
            View contentView,
            PropertyModel model,
            PlaybackModeIphController playbackModeIphController) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mContentView = contentView;
        mModel = model;
        mPlaybackModeIphController = playbackModeIphController;

        Resources res = mContext.getResources();
        mSpeedButton = (TextView) mContentView.findViewById(R.id.readaloud_playback_speed);
        mContentView
                .findViewById(R.id.readaloud_seek_back_button)
                .setContentDescription(res.getString(R.string.readaloud_replay, BACK_SECONDS));
        mContentView
                .findViewById(R.id.readaloud_seek_forward_button)
                .setContentDescription(res.getString(R.string.readaloud_forward, FORWARD_SECONDS));

        mOptionsMenu =
                new OptionsMenuSheetContent(
                        mContext, /* parent= */ this, mBottomSheetController, mModel);
        mNegativeFeedbackMenu =
                new NegativeFeedbackMenuSheetContent(
                        mContext, /* parent= */ this, mBottomSheetController);
        mSpeedMenu =
                new SpeedMenuSheetContent(
                        mContext, /* parent= */ this, mBottomSheetController, mModel);
        mNormalLayout = (LinearLayout) mContentView.findViewById(R.id.normal_layout);
        mLoadingLayout = (LinearLayout) mContentView.findViewById(R.id.readaloud_loading_overlay);
        mErrorLayout = (LinearLayout) mContentView.findViewById(R.id.error_layout);
        mSeekBar = (SeekBar) mContentView.findViewById(R.id.readaloud_expanded_player_seek_bar);
        mScrollView = (ScrollView) mContentView.findViewById(R.id.scroll_view);
        mModeSelectorButton = mContentView.findViewById(R.id.readaloud_mode_selector);
        mModeSelectorButton.setSelected(mIsModeActive);
        mPlaybackModeIphController.setAnchorView(mModeSelectorButton);

        mLoadingTextView = mContentView.findViewById(R.id.readaloud_loading_text);

        mPlayPauseButton = mContentView.findViewById(R.id.readaloud_play_pause_button);
        mRewindButton = mContentView.findViewById(R.id.readaloud_seek_back_button);
        mForwardButton = mContentView.findViewById(R.id.readaloud_seek_forward_button);

        mThumbUpButton = mContentView.findViewById(R.id.readaloud_thumb_up_button);
        mThumbDownButton = mContentView.findViewById(R.id.readaloud_thumb_down_button);
        mMoreOptionsButton = mContentView.findViewById(R.id.readaloud_more_button);

        View publisherButton = mContentView.findViewById(R.id.readaloud_player_publisher_button);
        publisherButton.addOnLayoutChangeListener(
                new View.OnLayoutChangeListener() {
                    @Override
                    public void onLayoutChange(
                            View v,
                            int left,
                            int top,
                            int right,
                            int bottom,
                            int oldLeft,
                            int oldTop,
                            int oldRight,
                            int oldBottom) {
                        TouchDelegateUtil.setBiggerTouchTarget(publisherButton);
                    }
                });

        mSeekBar.setAccessibilityDelegate(
                new View.AccessibilityDelegate() {
                    @Override
                    public void onInitializeAccessibilityEvent(
                            View host, AccessibilityEvent event) {
                        // Drop progress announcements that repeatedly interrupt playback.
                        if (event.getEventType()
                                == AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED) {
                            return;
                        }
                        super.onInitializeAccessibilityEvent(host, event);
                    }
                });
        mPlayerControls =
                (LinearLayout) mContentView.findViewById(R.id.readaloud_playback_controls);
        // Apply dynamic colors.
        Colors.setBottomSheetContentBackground(mContentView);
        Colors.setProgressBarColor(mSeekBar);

        onOrientationChange(res.getConfiguration().orientation);
    }

    public void onPlaybackStateChanged(@PlaybackListener.State int state) {
      setPlaying(state == PlaybackListener.State.PLAYING);
      switch (state) {
        case PlaybackListener.State.ERROR:
          showErrorLayout();
          break;
         case PlaybackListener.State.BUFFERING:
           showLoadingLayout();
           break;
        default:
          showNormalLayout();
          break;
      }
    }

    private void showLoadingLayout() {
      mErrorLayout.setVisibility(View.GONE);
      mLoadingLayout.setVisibility(View.VISIBLE);
      mNormalLayout.setAlpha(0.3f);

      mSpeedButton.setVisibility(View.GONE);
      mLoadingTextView.setVisibility(View.VISIBLE);

      setPlaybackControlsEnabled(false);
    }

    private void showErrorLayout() {
      mNormalLayout.setVisibility(View.GONE);
      mLoadingLayout.setVisibility(View.GONE);
      mErrorLayout.setVisibility(View.VISIBLE);
      mLoadingTextView.setVisibility(View.GONE);
    }

    private void showNormalLayout() {
      mNormalLayout.setVisibility(View.VISIBLE);
      mLoadingLayout.setVisibility(View.GONE);
      mErrorLayout.setVisibility(View.GONE);
      mLoadingTextView.setVisibility(View.GONE);

      mNormalLayout.setAlpha(1f);

      mSpeedButton.setVisibility(View.VISIBLE);
      setPlaybackControlsEnabled(true);
    }

    private void setPlaybackControlsEnabled(boolean enabled) {
      mModeSelectorButton.setEnabled(enabled);
      mModeSelectorButton.setClickable(enabled);
      mSeekBar.setEnabled(enabled);

      mPlayPauseButton.setEnabled(enabled);
      mPlayPauseButton.setClickable(enabled);

      mRewindButton.setEnabled(enabled);
      mRewindButton.setClickable(enabled);
      mForwardButton.setEnabled(enabled);
      mForwardButton.setClickable(enabled);
    }

    public void show() {
        boolean shown = mBottomSheetController.requestShowContent(this, /* animate= */ true);
        // Reset scrolling if needed.
        mScrollView.scrollTo(0, 0);

        if (shown) {
          mPlaybackModeIphController.maybeShowPlaybackModeIph();
        }
    }

    public void hide() {
        mBottomSheetController.hideContent(this, /* animate= */ true);
    }

    void setPlaybackMode(PlaybackMode playbackMode) {
      TextView chromeNowPlaying = mContentView.findViewById(R.id.chrome_now_playing_text);
      if (playbackMode == PlaybackMode.OVERVIEW) {
            mIsModeActive = true;
            mModeSelectorButton.setSelected(true);
            chromeNowPlaying.setText(
                    mContext.getString(R.string.readaloud_chrome_now_playing_audio_overview));
            if (ReadAloudFeatures.isAudioOverviewsFeedbackAllowed()) {
                showFeedbackButtons();
            } else {
              hideFeedbackButtons();
            }
            hideMoreOptions();
        } else {
            mIsModeActive = false;
            mModeSelectorButton.setSelected(false);
            chromeNowPlaying.setText(mContext.getString(R.string.readaloud_chrome_now_playing));
            hideFeedbackButtons();
            showMoreOptions();
        }
    }

    void showFeedbackButtons() {
        mThumbUpButton.setVisibility(View.VISIBLE);
        mThumbDownButton.setVisibility(View.VISIBLE);
    }

    void hideFeedbackButtons() {
        mThumbUpButton.setVisibility(View.GONE);
        mThumbDownButton.setVisibility(View.GONE);
    }

    void showMoreOptions() {
        mMoreOptionsButton.setVisibility(View.VISIBLE);
    }

    void hideMoreOptions() {
        mMoreOptionsButton.setVisibility(View.GONE);
    }

    void setPlaybackModeSelectionEnabled(PlaybackModeSelectionEnablementStatus status) {
        if (status == PlaybackModeSelectionEnablementStatus.MODE_SELECTION_ENABLED) {
          mModeSelectorButton.setVisibility(View.VISIBLE);
        } else {
          mModeSelectorButton.setVisibility(View.GONE);
        }
    }

    void setTitle(String title) {
        ((TextView) mContentView.findViewById(R.id.readaloud_expanded_player_title)).setText(title);
    }

    void setPublisher(String publisher) {
        ((TextView) mContentView.findViewById(R.id.readaloud_expanded_player_publisher))
                .setText(publisher);
    }

    void setSentFeedback(FeedbackType feedbackType) {
      if (feedbackType == FeedbackType.NONE) {
        mThumbUpButton.setSelected(false);
        mThumbDownButton.setSelected(false);
        return;
      }
      Toast.makeText(mContentView.getContext(), R.string.readaloud_feedback_toast_message, Toast.LENGTH_SHORT).show();
      if (feedbackType == FeedbackType.POSITIVE) {
        mThumbUpButton.setSelected(true);
        mThumbDownButton.setSelected(false);
        return;
      }
      if (feedbackType == FeedbackType.NEGATIVE) {
        mThumbUpButton.setSelected(false);
        mThumbDownButton.setSelected(true);
      }
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
        setOnClickListener(R.id.readaloud_mode_selector, () -> onPlaybackModeChangeClick(handler));
        setOnClickListener(R.id.readaloud_thumb_down_button, () -> showNegativeFeedbackMenu());
        setOnClickListener(R.id.readaloud_thumb_up_button, () -> handlePositiveFeedback(handler));

        SeekBar seekBar =
                (SeekBar) mContentView.findViewById(R.id.readaloud_expanded_player_seek_bar);
        seekBar.setOnSeekBarChangeListener(handler.getSeekBarChangeListener());
        mSpeedMenu.setInteractionHandler(handler);
        mOptionsMenu.setInteractionHandler(handler);
        mNegativeFeedbackMenu.setInteractionHandler(handler);
    }

    public void setSpeed(float speed) {
        mModel.set(PlayerProperties.SPEED, speed);
        String speedString = SpeedMenuSheetContent.speedFormatter(speed);
        mSpeedButton.setText(mContext.getString(R.string.readaloud_speed, speedString));
        mSpeedButton.setContentDescription(
                mContext.getString(R.string.readaloud_speed_menu_button, speedString));
    }

    void setHighlightingSupported(boolean supported) {
        mOptionsMenu.setHighlightingSupported(supported);
    }

    void setHighlightingEnabled(boolean enabled) {
        mOptionsMenu.setHighlightingEnabled(enabled);
    }

    void handlePositiveFeedback(InteractionHandler handler) {
      if (mThumbUpButton.isSelected()) {
        // Positive feedback was already sent.
        return;
      }
      handler.onPositiveFeedback();
    }

    public void setPlaying(boolean playing) {
        ImageView playButton =
                (ImageView) mContentView.findViewById(R.id.readaloud_play_pause_button);
        // If playing, update to show the pause button.
        if (playing) {
            playButton.setImageResource(R.drawable.pause_button);
            playButton.setContentDescription(mContext.getString(R.string.readaloud_pause));
        } else {
            playButton.setImageResource(R.drawable.play_button);
            playButton.setContentDescription(mContext.getString(R.string.readaloud_play));
        }
    }

    /**
     * @param percent out of 1.0
     */
    public void setProgress(float percent) {
        mSeekBar.setProgress((int) (percent * mSeekBar.getMax()), true);
    }

    @Nullable
    OptionsMenuSheetContent getOptionsMenu() {
        return mOptionsMenu;
    }

    private void onPlaybackModeChangeClick(InteractionHandler interactionHandler) {
        mIsModeActive = !mIsModeActive;
        mModeSelectorButton.setSelected(mIsModeActive);

        if (mIsModeActive) {
            interactionHandler.onPlaybackModeChanged(PlaybackMode.OVERVIEW);
        } else {
            interactionHandler.onPlaybackModeChanged(PlaybackMode.CLASSIC);
        }
    }

    public void showOptionsMenu() {
        // set bit saying we're waiting for another sheet
        mModel.set(PlayerProperties.SHOW_MINI_PLAYER_ON_DISMISS, false);
        mBottomSheetController.hideContent(this, /* animate= */ false);
        mBottomSheetController.requestShowContent(mOptionsMenu, /* animate= */ false);
    }

    public void showNegativeFeedbackMenu() {
        mModel.set(PlayerProperties.SHOW_MINI_PLAYER_ON_DISMISS, false);
        mBottomSheetController.hideContent(this, /* animate= */ false);
        mBottomSheetController.requestShowContent(mNegativeFeedbackMenu, /* animate= */ false);
    }

    @Nullable
    VoiceMenu getVoiceMenu() {
        if (mOptionsMenu == null) {
            return null;
        }
        return mOptionsMenu.getVoiceMenu();
    }

    public void notifySheetClosed(BottomSheetContent contentClosed) {
        mOptionsMenu.notifySheetClosed(contentClosed);
        mSpeedMenu.notifySheetClosed(contentClosed);
        mNegativeFeedbackMenu.notifySheetClosed(contentClosed);
    }

    public void showSpeedMenu() {
        // set bit saying we're waiting for another sheet
        mModel.set(PlayerProperties.SHOW_MINI_PLAYER_ON_DISMISS, false);
        mBottomSheetController.hideContent(this, /* animate= */ false);
        mBottomSheetController.requestShowContent(mSpeedMenu, /* animate= */ false);
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
        return mScrollView.getScrollY();
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
    public float getHalfHeightRatio() {
        // Only full height mode enabled.
        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public String getSheetContentDescription(Context context) {
        // "'Listen to this page' player."
        // Automatically appended: "Swipe down to close."
        return context.getString(R.string.readaloud_player_name);
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        Log.e(
                TAG,
                "Tried to get half height accessibility string, but half height isn't supported.");
        assert false;
        return Resources.ID_NULL;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        // "Read Aloud player opened at full height."
        return R.string.readaloud_player_opened_at_full_height;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        // "Read Aloud player minimized."
        return R.string.readaloud_player_minimized;
    }

    @Override
    public boolean canSuppressInAnyState() {
        // Always immediately hide if a higher-priority sheet content wants to show.
        return true;
    }

    private void setOnClickListener(int id, Runnable onClick) {
        mContentView
                .findViewById(id)
                .setOnClickListener(
                        (view) -> {
                            onClick.run();
                        });
    }

    /** Customize portraint and landscape mode sheets. Landscape mode layout is more compressed. */
    public void onOrientationChange(int orientation) {
        if (mOptionsMenu != null) {
            mOptionsMenu.onOrientationChange(orientation);
        }
        if (mNegativeFeedbackMenu != null) {
            mNegativeFeedbackMenu.onOrientationChange(orientation);
        }
        if (mSpeedMenu != null) {
            mSpeedMenu.onOrientationChange(orientation);
        }
        TextView chromeNowPlaying = mContentView.findViewById(R.id.chrome_now_playing_text);
        ViewGroup.MarginLayoutParams chromeNowPlayingParams =
                (ViewGroup.MarginLayoutParams) chromeNowPlaying.getLayoutParams();

        ViewGroup.LayoutParams errorParams = mErrorLayout.getLayoutParams();
        int bottomPadding = 0;
        int topMargin = 0;
        if (orientation == Configuration.ORIENTATION_PORTRAIT) {
            errorParams.height =
                    mContext.getResources()
                            .getDimensionPixelSize(R.dimen.error_layour_portrait_height);
            bottomPadding =
                    mContext.getResources()
                            .getDimensionPixelSize(R.dimen.readaloud_controls_portrait_padding);
            topMargin =
                    mContext.getResources()
                            .getDimensionPixelSize(R.dimen.readaloud_now_playing_spacing_portrait);

        } else if (orientation == Configuration.ORIENTATION_LANDSCAPE) {

            errorParams.height =
                    mContext.getResources()
                            .getDimensionPixelSize(R.dimen.error_layour_landscape_height);
            topMargin =
                    mContext.getResources()
                            .getDimensionPixelSize(R.dimen.readaloud_now_playing_spacing_landscape);
        }
        chromeNowPlayingParams.setMargins(0, topMargin, 0, 0);
        chromeNowPlaying.setLayoutParams(chromeNowPlayingParams);
        mErrorLayout.setLayoutParams(errorParams);
        mPlayerControls.setPadding(0, 0, 0, bottomPadding);
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
