// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.expanded;

import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.BUFFERING;
import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.ERROR;
import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.PAUSED;
import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.PLAYING;
import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.STOPPED;
import static org.chromium.chrome.modules.readaloud.PlaybackListener.State.UNKNOWN;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.chrome.browser.readaloud.player.InteractionHandler;
import org.chromium.chrome.browser.readaloud.player.PlayerProperties;
import org.chromium.chrome.browser.readaloud.player.R;
import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackVoice;
import org.chromium.chrome.modules.readaloud.PlaybackListener;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.HashMap;
import java.util.List;
import java.util.Locale;

/** Read Aloud voices submenu. */
class VoiceMenu {
    private static final String TAG = "ReadAloudVoices";
    private final Context mContext;
    private PlaybackVoice[] mVoices;
    private HashMap<String, Integer> mVoiceIdToMenuItemId;
    private final Menu mMenu;
    private final PropertyModel mModel;

    VoiceMenu(
            Context context,
            PropertyModel model,
            Runnable hideSelf) {
                this(context, model, hideSelf, LayoutInflater.from(context));
            }

    @VisibleForTesting
    VoiceMenu(
            Context context,
            PropertyModel model,
            Runnable hideSelf,
            LayoutInflater layoutInflater) {
                mModel = model;
        mMenu = (Menu) layoutInflater.inflate(R.layout.readaloud_menu, null);
        mMenu.afterInflating(() -> {
            // Views inside menu layout are only available after inflating.
            mMenu.setTitle(R.string.readaloud_voice_menu_title);
            mMenu.setContentDescription(R.string.readaloud_voice_menu_description);
            mMenu.setBackPressHandler(hideSelf);
            mMenu.setPlayButtonClickHandler(
                (itemId) -> {
                    getInteractionHandler().onPreviewVoiceClick(mVoices[itemId]);
                });
            mMenu.setVisibility(View.GONE);
        });

        mContext = context;
        mVoiceIdToMenuItemId = new HashMap<>();
        setVoices(model.get(PlayerProperties.VOICES_LIST));
        setVoiceSelection(model.get(PlayerProperties.SELECTED_VOICE_ID));
        mMenu.setRadioTrueHandler(this::onItemSelected);
    }

    Menu getMenu() {
        return mMenu;
    }

    void setVoices(List<PlaybackVoice> voices) {
        mMenu.clearItems();
        if (voices == null || voices.isEmpty()) {
            mVoices = new PlaybackVoice[0];
            return;
        }
        mVoices = new PlaybackVoice[voices.size()];

        int id = 0;
        String displayLocale = null;
        for (PlaybackVoice voice : voices) {
            if (id == 0 || isDifferentLocale(voice, voices.get(id - 1))) {
                displayLocale =
                        new Locale(voice.getLanguage(), voice.getAccentRegionCode())
                                .getDisplayName();
            } else {
                displayLocale = null;
            }
            MenuItem item =
                    mMenu.addItem(
                            id,
                            /* iconId= */ 0,
                            voice.getDisplayName(),
                            displayLocale,
                            MenuItem.Action.RADIO);
            item.addPlayButton();
            String secondLine = getAttributesString(voice);
            if (secondLine != null) {
                item.setSecondLine(secondLine);
            }
            mVoices[id] = voice;
            mVoiceIdToMenuItemId.put(voice.getVoiceId(), id);
            ++id;
        }
    }

    private boolean isDifferentLocale(PlaybackVoice current, PlaybackVoice previous) {
        return (!current.getLanguage().equals(previous.getLanguage())
                || !current.getAccentRegionCode().equals(previous.getAccentRegionCode()));
    }

    void setVoiceSelection(String voiceId) {
        if (mVoices.length == 0) {
            return;
        }
        Integer maybeId = mVoiceIdToMenuItemId.get(voiceId);
        int id = 0;

        // It's possible for an invalid voiceId to be passed in if the voice is removed
        // in an app update but its ID is still stored in prefs.
        // TODO(b/311060608): handle centrally in ReadAloudController and remove this case.
        if (maybeId == null) {
            Log.d(TAG, "Selected voice %s not available, falling back to the default.", voiceId);
        } else {
            id = maybeId.intValue();
        }

        // Let menu handle unchecking the existing selection.
        mMenu.getItem(id).setValue(true);
    }

    void updatePreviewButtons(String voiceId, @PlaybackListener.State int state) {
        Integer maybeId = mVoiceIdToMenuItemId.get(voiceId);
        assert maybeId != null : "Tried to preview a voice that isn't in the menu";

        MenuItem item = mMenu.getItem(maybeId.intValue());
        switch (state) {
            case BUFFERING:
                item.showPlayButtonSpinner();
                break;

                // TODO: handle error state
            case ERROR:
            case PAUSED:
            case STOPPED:
                item.setPlayButtonStopped();
                item.showPlayButton();
                break;

            case PLAYING:
                item.setPlayButtonPlaying();
                item.showPlayButton();
                break;

            case UNKNOWN:
            default:
                break;
        }
    }

    private void onItemSelected(int itemId) {
        InteractionHandler handler = getInteractionHandler();
        if (handler != null) {
            handler.onVoiceSelected(mVoices[itemId]);
        }
    }

    @Nullable
    private String getAttributesString(PlaybackVoice voice) {
        String pitch = getPitchString(voice);
        String tone = getToneString(voice);
        if (pitch == null || tone == null) {
            return null;
        }
        return mContext.getResources().getString(R.string.readaloud_voice_description, pitch, tone);
    }

    @Nullable
    private String getPitchString(PlaybackVoice voice) {
        return getStringOrNull(
                switch (voice.getPitch()) {
                    case PlaybackVoice.Pitch.LOW -> R.string.readaloud_pitch_low;
                    case PlaybackVoice.Pitch.MID -> R.string.readaloud_pitch_mid;
                    default -> 0;
                });
    }

    @Nullable
    private String getToneString(PlaybackVoice voice) {
        return getStringOrNull(
                switch (voice.getTone()) {
                    case PlaybackVoice.Tone.BOLD -> R.string.readaloud_tone_bold;
                    case PlaybackVoice.Tone.CALM -> R.string.readaloud_tone_calm;
                    case PlaybackVoice.Tone.STEADY -> R.string.readaloud_tone_steady;
                    case PlaybackVoice.Tone.SMOOTH -> R.string.readaloud_tone_smooth;
                    case PlaybackVoice.Tone.RELAXED -> R.string.readaloud_tone_relaxed;
                    case PlaybackVoice.Tone.WARM -> R.string.readaloud_tone_warm;
                    case PlaybackVoice.Tone.SERENE -> R.string.readaloud_tone_serene;
                    case PlaybackVoice.Tone.GENTLE -> R.string.readaloud_tone_gentle;
                    case PlaybackVoice.Tone.BRIGHT -> R.string.readaloud_tone_bright;
                    case PlaybackVoice.Tone.BREEZY -> R.string.readaloud_tone_breezy;
                    case PlaybackVoice.Tone.SOOTHING -> R.string.readaloud_tone_soothing;
                    case PlaybackVoice.Tone.PEACEFUL -> R.string.readaloud_tone_peaceful;
                    default -> 0;
                });
    }

    @Nullable
    private String getStringOrNull(int id) {
        return id != 0 ? mContext.getResources().getString(id) : null;
    }

    InteractionHandler getInteractionHandler() {
        return mModel.get(PlayerProperties.INTERACTION_HANDLER);
    }
}
