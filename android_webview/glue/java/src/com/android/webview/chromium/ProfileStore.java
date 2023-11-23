// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.ThreadUtils;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

@Lifetime.Singleton
public class ProfileStore {
    private final Map<String, Profile> mProfiles = new HashMap<>();

    private static ProfileStore sINSTANCE;

    private ProfileStore() {}

    public static ProfileStore getInstance() {
        ThreadUtils.checkUiThread();
        if (sINSTANCE == null) {
            sINSTANCE = new ProfileStore();
        }
        return sINSTANCE;
    }

    @NonNull
    public Profile getOrCreateProfile(@NonNull String name) {
        ThreadUtils.checkUiThread();
        return mProfiles.computeIfAbsent(
                name,
                profileName -> new Profile(AwBrowserContext.getNamedContext(profileName, true)));
    }

    @Nullable
    public Profile getProfile(@NonNull String name) {
        ThreadUtils.checkUiThread();
        return mProfiles.computeIfAbsent(
                name,
                profileName -> {
                    AwBrowserContext browserContext =
                            AwBrowserContext.getNamedContext(profileName, false);
                    return browserContext != null ? new Profile(browserContext) : null;
                });
    }

    @NonNull
    public List<String> getAllProfileNames() {
        ThreadUtils.checkUiThread();
        return AwBrowserContext.listAllContexts();
    }

    @NonNull
    public boolean deleteProfile(@NonNull String name) {
        ThreadUtils.checkUiThread();
        boolean deletionResult = AwBrowserContext.deleteNamedContext(name);
        if (deletionResult) {
            mProfiles.remove(name);
        } else {
            assert !mProfiles.containsKey(name);
        }
        return deletionResult;
    }
}
