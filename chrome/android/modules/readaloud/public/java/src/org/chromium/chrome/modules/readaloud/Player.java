// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.readaloud;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackVoice;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.util.List;
import java.util.Map;

/** This interface represents Read Aloud player UI. */
public interface Player {
    /** Embedders of the Read Aloud player must provide a Delegate implementation. */
    interface Delegate {
        /** Returns the BottomSheetController that will manage the bottom sheets. */
        BottomSheetController getBottomSheetController();
        /** Returns true if highlighting is supported. */
        boolean isHighlightingSupported();
        /** Returns the supplier for the "highlighting enabled" setting. */
        ObservableSupplierImpl<Boolean> getHighlightingEnabledSupplier();
        /** Returns the supplier for the list of voices to show in the voice menu. */
        ObservableSupplier<List<PlaybackVoice>> getCurrentLanguageVoicesSupplier();
        /** Returns the supplier for the current language's selected voice. */
        ObservableSupplier<String> getVoiceIdSupplier();
        /** Returns the mapping of language to current user-selected voice. */
        Map<String, String> getVoiceOverrides();

        /**
         * Called when the user selects a voice in the voice settings menu.
         * Saves the new choice for the given language and continues playback from the
         * same position.
         */
        void setVoiceOverride(PlaybackVoice voice);
        /** Play a short example of the specified voice. */
        void previewVoice(PlaybackVoice voice);
        /** Navigate to the tab associated with the current playback */
        void navigateToPlayingTab();
    }

    /** Observer interface to provide updates about player UI. */
    interface Observer {
        /*
         * Called when the user dismisses the player. The observer is responsible for
         * then calling dismissPlayers().
         */
        void onRequestClosePlayers();
    }

    /**
     * Add an observer to receive event updates.
     *
     * @param observer Observer to add.
     */
    default void addObserver(Observer observer) {}

    /**
     * Remove an observer that was previously added. No effect if the observer was
     * never added.
     * @param observer Observer to remove.
     */
    default void removeObserver(Observer observer) {}

    /** Stop playback and stop tracking players. */
    default void destroy() {}

    /**
     * Show the mini player, called when playback is requested.
     */
    default void playTabRequested() {}

    /**
     * Update players when playback is ready.
     *
     * @param playback             New Playback object.
     * @param currentPlaybackState Playback state.
     */
    default void playbackReady(
            Playback playback, @PlaybackListener.State int currentPlaybackState) {}

    /** Update players when playback fails. */
    default void playbackFailed() {}

    /** Hide players. */
    default void dismissPlayers() {}
}
