// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.readaloud;

import androidx.annotation.Nullable;

/**
 * Encapsulates information about the playback tha's requested.
 */
public class PlaybackArgs {

  private final String mUrl;
  @Nullable
  private final String mLanguage;
  @Nullable
  private final String mVoice;

  private final long mDateModifiedMsSinceEpoch;

  public PlaybackArgs(String url) {
    this(url, null, null, 0);
  }

  public PlaybackArgs(String url, @Nullable String language, @Nullable String voice,
      long dateModifiedMsSinceEpoch) {
    this.mUrl = url;
    this.mLanguage = language;
    this.mVoice = voice;
    this.mDateModifiedMsSinceEpoch = dateModifiedMsSinceEpoch;
  }

  /** Returns the URL of the playback page. */
  public String getUrl() {
    return mUrl;
  }

  /**
   * Returns the language to request the audio in. If not set, the default is
   * used.
   */
  @Nullable
  public String getLanguage() {
    return mLanguage;
  }

  /**
   * Returns the voice to use for playback audio. If not set, the default is used.
   */
  @Nullable
  public String getVoice() {
    return mVoice;
  }

  /**
   * Represents the website version.
   */
  public long getDateModifiedMsSinceEpoch() {
    return mDateModifiedMsSinceEpoch;
  }
}
