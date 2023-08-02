// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.readaloud;

/** Provides an empty implementation of ReadAloudPlaybackHooks. */
public class ReadAloudPlaybackHooksProvider {
    public static ReadAloudPlaybackHooks getInstance() {
        return new ReadAloudPlaybackHooks() {};
    }
}
