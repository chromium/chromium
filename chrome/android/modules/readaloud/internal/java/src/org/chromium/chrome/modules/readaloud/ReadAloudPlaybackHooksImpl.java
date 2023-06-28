// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

/** Empty implementation of ReadAloudPlaybackHooks. */
public class ReadAloudPlaybackHooksImpl implements ReadAloudPlaybackHooks {
    public static ReadAloudPlaybackHooks getInstance() {
        return new ReadAloudPlaybackHooks() {};
    }
}
