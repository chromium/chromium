// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

/** Empty implementation of ReadAloudReadabilityHooks. */
public class ReadAloudReadabilityHooksImpl implements ReadAloudReadabilityHooks {
    @Override
    public boolean isEnabled() {
        return false;
    }

    @Override
    public void isPageReadable(String url, ReadabilityCallback callback) {
        return;
    }
}
