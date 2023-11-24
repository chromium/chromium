// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.readaloud;

import androidx.annotation.Nullable;

import java.util.List;

/** Encapsulates information about the playback tha's requested. */
public class PlaybackArgs {
    /** TODO(basiaz): Delete after source lands e2e */
    private final String mUrl;

    /* Can represent either page url or plain text */
    private final String mSource;
    /* if false, the surce is plain text rather than url of a website. */
    private final boolean mIsSourceUrl;

    @Nullable private final String mLanguage;
    @Nullable private final List<PlaybackVoice> mVoices;
    private final long mDateModifiedMsSinceEpoch;

    /**
     * Encapsulates info about a TTS voice that can be used for playback.
     * Description is only relevant for the UI, language and voiceId are required
     * for the server request.
     */
    public static class PlaybackVoice {
        private final String mLanguage;
        private final String mVoiceId;
        @Nullable private final String mDescription;

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

        @Override
        public String toString() {
            return "PlaybackVoice{"
                    + "language="
                    + mLanguage
                    + " id="
                    + mVoiceId
                    + " description="
                    + mDescription
                    + "}";
        }
    }

    public PlaybackArgs(
            String url,
            @Nullable String language,
            @Nullable List<PlaybackVoice> voices,
            long dateModifiedMsSinceEpoch) {
        this(url, true, language, voices, dateModifiedMsSinceEpoch);
    }

    public PlaybackArgs(
            String mSource,
            boolean isUrl,
            @Nullable String language,
            @Nullable List<PlaybackVoice> voices,
            long dateModifiedMsSinceEpoch) {
        this.mUrl = mSource;
        this.mSource = mSource;
        this.mIsSourceUrl = isUrl;
        this.mLanguage = language;
        this.mVoices = voices;
        this.mDateModifiedMsSinceEpoch = dateModifiedMsSinceEpoch;
    }

    /** Returns the URL of the playback page. */
    @Deprecated
    public String getUrl() {
        return mUrl;
    }

    /** Returns the source which can either be an URL or plain text */
    public String getSource() {
        return mSource;
    }

    /** Returns true if the source is an URL, false if it's plain text. */
    public boolean isSourceUrl() {
        return mIsSourceUrl;
    }

    /**
     * Returns the language to request the audio in. If not set, the default is
     * used.
     */
    @Nullable
    public String getLanguage() {
        return mLanguage;
    }

    /** Returns the list of voices that may be used for synthesis. */
    @Nullable
    public List<PlaybackVoice> getVoices() {
        return mVoices;
    }

    /** Represents the website version. */
    public long getDateModifiedMsSinceEpoch() {
        return mDateModifiedMsSinceEpoch;
    }

    // Override toString() to help with debug logging.
    @Override
    public String toString() {
        String voicesString = "";
        for (PlaybackVoice voice : mVoices) {
            voicesString += "\t\t" + voice + "\n";
        }

        return "PlaybackArgs{\n"
                + "\t"
                + (mIsSourceUrl ? "url=" : "text=")
                + mSource
                + "\n"
                + "\tlanguage="
                + mLanguage
                + "\n"
                + "\tvoices={\n"
                + voicesString
                + "\t}\n"
                + "\tdateModifiedMs="
                + mDateModifiedMsSinceEpoch
                + "\n"
                + "}";
    }
}
