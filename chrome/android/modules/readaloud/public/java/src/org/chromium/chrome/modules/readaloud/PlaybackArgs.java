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
        @IntDef({
            Tone.NONE,
            Tone.BOLD,
            Tone.CALM,
            Tone.STEADY,
            Tone.SMOOTH,
            Tone.RELAXED,
            Tone.WARM,
            Tone.SERENE,
            Tone.GENTLE,
            Tone.BRIGHT,
            Tone.BREEZY,
            Tone.SOOTHING,
            Tone.PEACEFUL
        })
        @Retention(RetentionPolicy.SOURCE)
        public @interface Tone {
            int NONE = 0;
            int BOLD = 1;
            int CALM = 2;
            int STEADY = 3;
            int SMOOTH = 4;
            int RELAXED = 5;
            int WARM = 6;
            int SERENE = 7;
            int GENTLE = 8;
            int BRIGHT = 9;
            int BREEZY = 10;
            int SOOTHING = 11;
            int PEACEFUL = 12;
        }

        /** Enum for Voice Display Names. */
        @IntDef({
            DisplayName.NONE,
            DisplayName.RUBY,
            DisplayName.RIVER,
            DisplayName.FIELD,
            DisplayName.MOSS,
            DisplayName.CLOUD,
            DisplayName.DALE,
            DisplayName.LAKE,
            DisplayName.AIR,
            DisplayName.COAST,
            DisplayName.CORAL,
        })
        @Retention(RetentionPolicy.SOURCE)
        public @interface DisplayName {
            int NONE = 0;
            int RUBY = 1;
            int RIVER = 2;
            int FIELD = 3;
            int MOSS = 4;
            int CLOUD = 5;
            int DALE = 6;
            int LAKE = 7;
            int AIR = 8;
            int COAST = 9;
            int CORAL = 10;
        }

        private final String mLanguage;
        @Nullable private final String mAccentRegionCode;
        private final String mVoiceId;
        private final String mDisplayName;

        private final @Pitch int mPitch;
        private final @Tone int mTone;
        private final @DisplayName int mVoiceDisplayName;

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
            mVoiceDisplayName = DisplayName.NONE;
            mPitch = pitch;
            mTone = tone;
        }

        public PlaybackVoice(
                String language,
                @Nullable String accentRegionCode,
                String voiceId,
                @DisplayName int displayName,
                @Pitch int pitch,
                @Tone int tone) {
            mLanguage = language;
            mAccentRegionCode = accentRegionCode;
            mVoiceId = voiceId;
            mVoiceDisplayName = displayName;
            mDisplayName = null;
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

        // TODO(iwells): Remove this method when it is no longer called internally.
        @Nullable
        public String getDescription() {
            return mDisplayName;
        }

        @Nullable
        public String getDisplayName() {
            return mDisplayName;
        }

        public @DisplayName int getVoiceDisplayName() {
            return mVoiceDisplayName;
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
