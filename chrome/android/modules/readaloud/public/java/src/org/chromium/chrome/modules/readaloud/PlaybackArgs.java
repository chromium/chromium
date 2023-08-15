// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.readaloud;

import androidx.annotation.Nullable;

import java.util.List;
/**
 * Encapsulates information about the playback tha's requested.
 */
public class PlaybackArgs {

  private final String mUrl;
  @Nullable
  private final String mLanguage;
  @Nullable
  private final List<PlaybackVoice> mVoices;

  private final long mDateModifiedMsSinceEpoch;

  public PlaybackArgs(String url) {
    this(url, null, null, 0);
  }

  /**
   * Encapsulates info about a TTS voice that can be used for playback.
   * Description is only relevant for the UI, language and voiceId are required
   * for the server request.
   */
  class PlaybackVoice {
      private final String mLanguage;
      private final String mVoiceId;
      @Nullable
      private final String mDescription;

      public PlaybackVoice(String language, String voiceId, String description) {
          this.mLanguage = language;
          this.mVoiceId = voiceId;
          this.mDescription = description;
      }

      public String getLanguage() {
          return mLanguage;
      }

      public String getVoiceId() {
          return mVoiceId;
      }

      @Nullable
      public String getDescription() {
          return mDescription;
      }
  }

  public PlaybackArgs(String url, @Nullable String language, @Nullable List<PlaybackVoice> voices,
          long dateModifiedMsSinceEpoch) {
      this.mUrl = url;
      this.mLanguage = language;
      this.mVoices = voices;
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
   * Returns the list of voices that may be used for synthesis.
   */
  @Nullable
  public List<PlaybackVoice> getVoices() {
      return mVoices;
  }

  /**
   * Represents the website version.
   */
  public long getDateModifiedMsSinceEpoch() {
    return mDateModifiedMsSinceEpoch;
  }
}
