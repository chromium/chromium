// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import android.content.Context;

import androidx.annotation.Nullable;

/** Empty implementation of ReadAloudReadabilityHooks. */
public class ReadAloudReadabilityHooksImpl implements ReadAloudReadabilityHooks {
    public ReadAloudReadabilityHooksImpl(Context context, @Nullable String apiKeyOverride) {}

    @Override
    public boolean isEnabled() {
        return false;
    }

    @Override
    public void isPageReadable(String url, ReadabilityCallback callback) {
        return;
    }
}
