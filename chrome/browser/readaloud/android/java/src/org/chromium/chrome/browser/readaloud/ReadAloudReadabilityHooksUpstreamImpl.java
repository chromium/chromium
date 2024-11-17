// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import android.content.Context;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;

import java.util.HashSet;

/** Empty implementation of ReadAloudReadabilityHooks. */
public class ReadAloudReadabilityHooksUpstreamImpl implements ReadAloudReadabilityHooks {
    public ReadAloudReadabilityHooksUpstreamImpl(Context context, Profile profile) {}

    @Override
    public boolean isEnabled() {
        return false;
    }

    @Override
    public void isPageReadable(Tab tab, String url, ReadabilityCallback callback) {}

    @Override
    public void isPageReadable(String url, ReadabilityCallback callback) {}

    @Override
    public HashSet<String> getCompatibleLanguages() {
        return new HashSet<String>();
    }
}
