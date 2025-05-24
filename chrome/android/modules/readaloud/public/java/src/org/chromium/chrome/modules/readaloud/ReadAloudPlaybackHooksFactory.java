// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.modules.readaloud;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

@NullMarked
public interface ReadAloudPlaybackHooksFactory {
    /** Creates or returns an instance associated with the specified Profile. */
    public ReadAloudPlaybackHooks getForProfile(Profile profile);
}
