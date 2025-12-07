// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwBrowserContextStore;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

@Lifetime.Singleton
public class ProfileStore {

    @IntDef({
        CallSite.GET_DEFAULT_PROFILE,
        CallSite.ASYNC_WEBVIEW_STARTUP,
        CallSite.ANDROIDX_API_CALL,
        CallSite.COUNT,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface CallSite {
        int GET_DEFAULT_PROFILE = 0;
        int ASYNC_WEBVIEW_STARTUP = 1;
        int ANDROIDX_API_CALL = 2;
        int COUNT = 3;
    };

    private final Map<String, Profile> mProfiles = new HashMap<>();

    private static ProfileStore sINSTANCE;
    private static final String PROFILE_WAS_CREATED_BY_ASYNC_WEBVIEW_STARTUP_HISTOGRAM =
            "Android.WebView.ProfileWasCreatedByAsyncStartup";

    private ProfileStore() {}

    public static ProfileStore getInstance() {
        ThreadUtils.checkUiThread();
        if (sINSTANCE == null) {
            sINSTANCE = new ProfileStore();
        }
        return sINSTANCE;
    }

    private static String callSiteToString(@CallSite int callSite) {
        return switch (callSite) {
            case CallSite.GET_DEFAULT_PROFILE -> "GET_DEFAULT_PROFILE";
            case CallSite.ASYNC_WEBVIEW_STARTUP -> "ASYNC_WEBVIEW_STARTUP";
            case CallSite.ANDROIDX_API_CALL -> "ANDROIDX_API_CALL";
            default -> "UNKNOWN";
        };
    }

    @NonNull
    public Profile getOrCreateProfile(@NonNull String name, @CallSite int callSite) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.ProfileStore.GET_OR_CREATE_PROFILE", callSiteToString(callSite))) {
            ThreadUtils.checkUiThread();
            RecordHistogram.recordBooleanHistogram(
                    PROFILE_WAS_CREATED_BY_ASYNC_WEBVIEW_STARTUP_HISTOGRAM,
                    callSite == CallSite.ASYNC_WEBVIEW_STARTUP);
            return mProfiles.computeIfAbsent(
                    name,
                    profileName ->
                            new Profile(AwBrowserContextStore.getNamedContext(profileName, true)));
        }
    }

    @Nullable
    public Profile getProfile(@NonNull String name) {
        try (TraceEvent event = TraceEvent.scoped("WebView.ProfileStore.ApiCall.GET_PROFILE")) {
            ThreadUtils.checkUiThread();
            return mProfiles.computeIfAbsent(
                    name,
                    profileName -> {
                        AwBrowserContext browserContext =
                                AwBrowserContextStore.getNamedContext(profileName, false);
                        return browserContext != null ? new Profile(browserContext) : null;
                    });
        }
    }

    @NonNull
    public List<String> getAllProfileNames() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.ProfileStore.ApiCall.GET_ALL_PROFILE_NAMES")) {
            ThreadUtils.checkUiThread();
            return AwBrowserContextStore.listAllContexts();
        }
    }

    public boolean deleteProfile(@NonNull String name) {
        try (TraceEvent event = TraceEvent.scoped("WebView.ProfileStore.ApiCall.DELETE_PROFILE")) {
            ThreadUtils.checkUiThread();
            boolean deletionResult = AwBrowserContextStore.deleteNamedContext(name);
            if (deletionResult) {
                mProfiles.remove(name);
            } else {
                assert !mProfiles.containsKey(name);
            }
            return deletionResult;
        }
    }
}
