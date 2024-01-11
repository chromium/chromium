// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.readaloud;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/** Encapsulates information about the playback being requested. */
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
     * Encapsulates info about a TTS voice that can be used for playback. Tone is only relevant for
     * the UI, language and voiceId are required for the server request.
     */
    public static class PlaybackVoice {
        /** Enum for voice pitch. */
        @IntDef({Pitch.NONE, Pitch.LOW, Pitch.MID})
        @Retention(RetentionPolicy.SOURCE)
        public @interface Pitch {
            int NONE = 0;
            int LOW = 1;
            int MID = 2;
        }

        /** Enum for descriptive words about voices. */
        @IntDef({Tone.NONE, Tone.BOLD, Tone.CALM, Tone.STEADY, Tone.SMOOTH, Tone.RELAXED})
        @Retention(RetentionPolicy.SOURCE)
        public @interface Tone {
            int NONE = 0;
            int BOLD = 1;
            int CALM = 2;
            int STEADY = 3;
            int SMOOTH = 4;
            int RELAXED = 5;
        }

        private final String mLanguage;
        @Nullable private final String mAccentRegionCode;
        private final String mVoiceId;
        private final String mDisplayName;

        private final @Pitch int mPitch;
        private final @Tone int mTone;

        // Deprecated. Remove once internal code no longer uses it.
        public PlaybackVoice(String language, String voiceId, String displayName) {
            this(
                    language,
                    /* accentRegionCode= */ null,
                    voiceId,
                    displayName,
                    Pitch.NONE,
                    Tone.NONE);
        }

        public PlaybackVoice(
                String language,
                @Nullable String accentRegionCode,
                String voiceId,
                String displayName,
                @Pitch int pitch,
                @Tone int tone) {
            mLanguage = language;
            mAccentRegionCode = accentRegionCode;
            mVoiceId = voiceId;
            mDisplayName = displayName;
            mPitch = pitch;
            mTone = tone;
        }

        @VisibleForTesting
        public PlaybackVoice(String language, String voiceId) {
            this(
                    language,
                    /* accentRegionCode= */ null,
                    voiceId,
                    /* displayName= */ null,
                    Pitch.NONE,
                    Tone.NONE);
        }

        public String getLanguage() {
            return mLanguage;
        }

        public String getVoiceId() {
            return mVoiceId;
        }

        @Nullable
        public String getAccentRegionCode() {
            return mAccentRegionCode;
        }

        @Nullable
        public String getDisplayName() {
            return mDisplayName;
        }

        public @Pitch int getPitch() {
            return mPitch;
        }

        public @Tone int getTone() {
            return mTone;
        }

        @Override
        public String toString() {
            return "PlaybackVoice{"
                    + "language="
                    + mLanguage
                    + " accent="
                    + mAccentRegionCode
                    + " id="
                    + mVoiceId
                    + " displayName="
                    + mDisplayName
                    + " pitch="
                    + mPitch
                    + " tone="
                    + mTone
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
