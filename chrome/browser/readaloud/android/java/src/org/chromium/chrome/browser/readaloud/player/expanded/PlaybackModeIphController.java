// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import android.view.View;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.readaloud.player.R;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.feature_engagement.FeatureConstants;

@NullMarked
class PlaybackModeIphController {
  private static final int TOOLTIP_DELAY_MILLIS = 500;

  private final UserEducationHelper mUserEducationHelper;
  private @Nullable View mAnchorView;

  PlaybackModeIphController(UserEducationHelper userEducationHelper) {
    mUserEducationHelper = userEducationHelper;
  }

  public void setAnchorView(View anchorView) {
    mAnchorView = anchorView;
  }

  /** Request to show IPH for playback mode button. */
  void maybeShowPlaybackModeIph() {
    if (mAnchorView == null || mAnchorView.getVisibility() != View.VISIBLE) {
      return;
    }

    PostTask.postDelayedTask(
        TaskTraits.UI_DEFAULT,
        () -> {
          if (mAnchorView == null || mAnchorView.getVisibility() != View.VISIBLE) {
            return;
          }
          mUserEducationHelper.requestShowIph(
              new IphCommandBuilder(
                      mAnchorView.getContext().getResources(),
                      FeatureConstants.READ_ALOUD_PLAYBACK_MODE_FEATURE,
                      R.string.readaloud_playback_mode_iph,
                      R.string.readaloud_playback_mode_iph)
                  .setAnchorView(mAnchorView)
                  .setShowTextBubble(true)
                  .build());
        },
        /* delay= */ TOOLTIP_DELAY_MILLIS);
  }
}
