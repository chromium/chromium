// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.readaloud.external;

import com.google.common.util.concurrent.Futures;
import com.google.common.util.concurrent.ListenableFuture;

/**
 *  This interface allows creating ReadAloud audio playback for a page.
 */
public interface ReadAloud {
    /**
     * Generate audio and metadata for a page.
     * @param readaloudAudioLoadArgs Serialized ReadAloudAudioLoadArgs proto message.
     * @return ListenableFuture returning the playback controller.
     */
    default ListenableFuture<Playback> createPlayback(byte[] readaloudAudioLoadArgs) {
        return Futures.immediateFuture(null);
    }
}
