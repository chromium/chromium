// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.ThreadUtils;

import java.util.List;

@Lifetime.Singleton
public class ProfileStore {
    @NonNull
    public Profile getOrCreateProfile(@NonNull String name) {
        ThreadUtils.checkUiThread();
        return new Profile(AwBrowserContext.getNamedContext(name, true));
    }

    @Nullable
    public Profile getProfile(@NonNull String name) {
        ThreadUtils.checkUiThread();
        AwBrowserContext browserContext = AwBrowserContext.getNamedContext(name, false);
        if (browserContext != null) {
            return new Profile(browserContext);
        }
        return null;
    }

    @NonNull
    public List<String> getAllProfileNames() {
        ThreadUtils.checkUiThread();
        return AwBrowserContext.listAllContexts();
    }

    @NonNull
    public boolean deleteProfile(@NonNull String name) {
        ThreadUtils.checkUiThread();
        return AwBrowserContext.deleteNamedContext(name);
    }
}
