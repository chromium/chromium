// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import org.chromium.chrome.browser.readaloud.external.ReadAloud;

/** Empty implementation of ReadAloudHooks. */
public class ReadAloudHooksImpl implements ReadAloudHooks {
    @Override
    ReadAloud getReadAloud() {
        return new ReadAloud() {};
    }
}
