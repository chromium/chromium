// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.readaloud.contentjs;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.modules.readaloud.Playback;
import org.chromium.chrome.modules.readaloud.PlaybackListener.PhraseTiming;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Interface for javascript-based page highlighter, aka "karaoke mode". */
public interface Highlighter {
    @IntDef({
        Mode.TEXT_HIGHLIGHTING_MODE_WORD,
        Mode.TEXT_HIGHLIGHTING_MODE_WORD_OVER_PARAGRAPH,
        Mode.TEXT_HIGHLIGHTING_MODE_PARAGRAPH,
        Mode.TEXT_HIGHLIGHTING_MODE_OFF
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface Mode {
        // Highlight word only.
        int TEXT_HIGHLIGHTING_MODE_WORD = 0;
        // Highlight word over paragraph.
        int TEXT_HIGHLIGHTING_MODE_WORD_OVER_PARAGRAPH = 1;
        // Highlight paragraph only.
        int TEXT_HIGHLIGHTING_MODE_PARAGRAPH = 2;
        // No highlighting.
        int TEXT_HIGHLIGHTING_MODE_OFF = 3;
    }

    /** Highlighting configuration. To be added: colors, different mode support */
    public static class Config {
        private @Mode int mMode = Mode.TEXT_HIGHLIGHTING_MODE_WORD;

        public @Mode int getMode() {
            return mMode;
        }
    }

    /**
     * Initializes and sets up the highlighting. The GlobalRenderFrameHostId retrieved from this
     * tabs main frame should be used for any subsequent call to request highlights.
     *
     * @param tab the currently playing tab
     * @param metadata information about playback used tor the script initialization.
     * @param isLightMode if user is in dark or light UI mode. Used for highlight color selection.
     */
    default void initializeJs(Tab tab, Playback.Metadata metadata, Config config) {}

    /**
     * Highlight text. Must be called after initializeJs.
     *
     * @param renderFrameHostId id of the RenderFrameHost that should evaluate js
     * @param tab the currently playing tab
     * @param phraseTiming timing information for selecting words to highlight.
     */
    default void highlightText(
            GlobalRenderFrameHostId frameId, Tab tab, PhraseTiming phraseTiming) {}

    /**
     * Reloading tab may require cleaning internal state.
     *
     * @param tab the tab that was reloaded
     */
    default void handleTabReloaded(Tab tab) {}

    /**
     * @param renderFrameHostId id of the RenderFrameHost that should evaluate js
     * @param tab the tab to clear highlights for. A no-op if highlighting was never requested.
     */
    default void clearHighlights(GlobalRenderFrameHostId frameId, Tab tab) {}
}
