// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.readaloud;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.base.Promise;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackVoice;
import org.chromium.chrome.modules.readaloud.contentjs.Highlighter;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.prefs.PrefService;

import java.util.List;

/** This interface represents Read Aloud player UI. */
public interface Player {
    /** Embedders of the Read Aloud player must provide a Delegate implementation. */
    interface Delegate {
        /** Returns the BottomSheetController that will manage the bottom sheets. */
        BottomSheetController getBottomSheetController();

        /** Returns true if highlighting is supported. */
        boolean isHighlightingSupported();

        /** Set highlighter mode. */
        void setHighlighterMode(@Highlighter.Mode int mode);

        /** Returns the supplier for the "highlighting enabled" setting. */
        ObservableSupplierImpl<Boolean> getHighlightingEnabledSupplier();

        /** Returns the supplier for the list of voices to show in the voice menu. */
        ObservableSupplier<List<PlaybackVoice>> getCurrentLanguageVoicesSupplier();

        /** Returns the supplier for the current language's selected voice. */
        ObservableSupplier<String> getVoiceIdSupplier();

        /**
         * Called when the user selects a voice in the voice settings menu. Saves the new choice for
         * the given language and continues playback from the same position.
         */
        void setVoiceOverrideAndApplyToPlayback(PlaybackVoice voice);

        /**
         * Play a short example of the specified voice.
         *
         * @param voice Voice to preview.
         * @return Promise that resolves to the preview's playback.
         */
        Promise<Playback> previewVoice(PlaybackVoice voice);

        /**
         * Restores playback to the UI. If playback is stopped due to background playback, the UI
         * for the original playback will still be shown so it can be seamless restored on play.
         */
        void restorePlayback();

        /** Navigate to the tab associated with the current playback */
        void navigateToPlayingTab();

        /** Returns the Activity in which the player UI should live. */
        Activity getActivity();

        /** Returns the current profile's PrefService. */
        PrefService getPrefService();

        /** Returns the {@link BottomControlsStacker} to allow pushing web contents up. */
        BottomControlsStacker getBottomControlsStacker();

        /**
         * Returns the LayoutManager, needed for showing the mini player SceneLayer which is drawn
         * in place of the mini player layout during browser controls resizing when showing and
         * hiding.
         */
        LayoutManager getLayoutManager();

        /**
         * Return {@link ActivityLifecycleDispatcher} that can be used to register for configuration
         * change updates.
         */
        ActivityLifecycleDispatcher getActivityLifecycleDispatcher();

        /** Return the current {@link Profile}. */
        @Nullable
        Profile getProfile();

        /** Return {@link UserEducationHelper} for requesting in-product-help. */
        UserEducationHelper getUserEducationHelper();
    }

    /** Observer interface to provide updates about player UI. */
    interface Observer {
        /*
         * Called when the user dismisses the player. The observer is responsible for
         * then calling dismissPlayers().
         */
        void onRequestClosePlayers();

        /** Called when the user closes the voice menu. */
        void onVoiceMenuClosed();

        /** Called when the mini player finishes its transition from gone to showing. */
        void onMiniPlayerShown();
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

    /** Show the mini player, called when playback is requested. */
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

    /** Show mini player. Assumes the playback is running. */
    default void restoreMiniPlayer() {}

    /** Only called after playback is released and no more events are coming. */
    default void recordPlaybackDuration() {}

    /** Hide players, clears playback and sets UI to a stopped state. */
    default void dismissPlayers() {}

    /**
     * Hide miniPlayer, unsubscribe from updates. State updates will resume after {@link
     * #restoreMiniPlayer() restoreMiniPlayer} is called.
     */
    default void hideMiniPlayer() {}

    /**
     * Hide players, unsubscribe from progress updates. Updates will resume after {@link
     * #restoreMiniPlayer() restoreMiniPlayer} or {@link #restorePlayers() restorePlayers} is
     * called.
     */
    default void hidePlayers() {}

    /** Show back whatever player was shown last. Assumes the playback is running. */
    default void restorePlayers() {}

    /**
     * Called when the Application state changes.
     *
     * @param boolean isScreenLocked
     */
    default void onScreenStatusChanged(boolean isScreenLocked) {}

    /**
     * Sets the player restorable property. This is used to indicate whether there is a restorable
     * playback that has been stopped due to background playback.
     */
    default void setPlayerRestorable(boolean isPlayerRestorable) {}
}
