// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.readaloud;

/** Interface for creating ReadAloud playback. */
public interface ReadAloudPlaybackHooks {
    /** Interface to receive createPlayback result. */
    interface CreatePlaybackCallback {
        /** Called if createPlayback() succeeds. */
        void onSuccess(Playback playback);
        /** Called if createPlayback() fails. */
        void onFailure(Throwable t);
    }

    /**
     * Request playback for the given URL.
     *
     * @param url Page URL.
     * @param dateModifiedMsSinceEpoch Page's dateModified attribute in terms of milliseconds since
     *         Unix epoch. Pass current time if dateModified isn't available.
     * @param callback Called when request finishes or fails.
     */
    default void createPlayback(
            String url, long dateModifiedMsSinceEpoch, CreatePlaybackCallback callback) {}
}
