// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.readaloud;

import org.chromium.chrome.browser.profiles.Profile;

/** Provides an empty implementation of ReadAloudPlaybackHooks. */
public class ReadAloudPlaybackHooksProvider {
    /** Creates or returns an instance associated with the specified Profile. */
    public static ReadAloudPlaybackHooks getForProfile(Profile profile) {
        return new ReadAloudPlaybackHooks() {};
    }
}
