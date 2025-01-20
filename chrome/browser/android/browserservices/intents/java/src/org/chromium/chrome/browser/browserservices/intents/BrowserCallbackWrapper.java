// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.intents;

import android.net.Uri;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.OptIn;
import androidx.browser.auth.AuthTabCallback;
import androidx.browser.auth.AuthTabSessionToken;
import androidx.browser.auth.ExperimentalAuthTab;
import androidx.browser.customtabs.CustomTabsCallback;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.ExperimentalMinimizationCallback;

/** Class that holds either a {@link CustomTabsSessionToken} or {@link AuthTabSessionToken}. */
@OptIn(markerClass = {ExperimentalAuthTab.class, ExperimentalMinimizationCallback.class})
public class BrowserCallbackWrapper {
    private final CustomTabsCallback mCustomTabsCallback;
    private final AuthTabCallback mAuthTabCallback;

    public BrowserCallbackWrapper(@NonNull CustomTabsCallback callback) {
        mCustomTabsCallback = callback;
        mAuthTabCallback = null;
    }

    public BrowserCallbackWrapper(@NonNull AuthTabCallback callback) {
        mAuthTabCallback = callback;
        mCustomTabsCallback = null;
    }

    public void onNavigationEvent(int navigationEvent, @Nullable Bundle extras) {
        if (mCustomTabsCallback != null) {
            mCustomTabsCallback.onNavigationEvent(navigationEvent, extras);
        } else {
            assert mAuthTabCallback != null;
            mAuthTabCallback.onNavigationEvent(
                    navigationEvent, extras != null ? extras : Bundle.EMPTY);
        }
    }

    public void extraCallback(@NonNull String callbackName, @Nullable Bundle args) {
        if (mCustomTabsCallback != null) {
            mCustomTabsCallback.extraCallback(callbackName, args);
        } else {
            assert mAuthTabCallback != null;
            mAuthTabCallback.onExtraCallback(callbackName, args != null ? args : Bundle.EMPTY);
        }
    }

    @Nullable
    public Bundle extraCallbackWithResult(@NonNull String callbackName, @Nullable Bundle args) {
        if (mCustomTabsCallback != null) {
            return mCustomTabsCallback.extraCallbackWithResult(callbackName, args);
        } else {
            assert mAuthTabCallback != null;
            return mAuthTabCallback.onExtraCallbackWithResult(
                    callbackName, args != null ? args : Bundle.EMPTY);
        }
    }

    public void onMessageChannelReady(@Nullable Bundle extras) {
        if (mCustomTabsCallback != null) {
            mCustomTabsCallback.onMessageChannelReady(extras);
        }
    }

    public void onPostMessage(@NonNull String message, @Nullable Bundle extras) {
        if (mCustomTabsCallback != null) {
            mCustomTabsCallback.onPostMessage(message, extras);
        }
    }

    public void onRelationshipValidationResult(
            int relation, @NonNull Uri requestedOrigin, boolean result, @Nullable Bundle extras) {
        if (mCustomTabsCallback != null) {
            mCustomTabsCallback.onRelationshipValidationResult(
                    relation, requestedOrigin, result, extras);
        }
    }

    public void onActivityResized(int height, int width, @NonNull Bundle extras) {
        if (mCustomTabsCallback != null) {
            mCustomTabsCallback.onActivityResized(height, width, extras);
        }
    }

    public void onWarmupCompleted(@NonNull Bundle extras) {
        if (mCustomTabsCallback != null) {
            mCustomTabsCallback.onWarmupCompleted(extras);
        } else {
            assert mAuthTabCallback != null;
            mAuthTabCallback.onWarmupCompleted(extras);
        }
    }

    public void onActivityLayout(
            int left, int top, int right, int bottom, int state, @NonNull Bundle extras) {
        if (mCustomTabsCallback != null) {
            mCustomTabsCallback.onActivityLayout(left, top, right, bottom, state, extras);
        }
    }

    public void onMinimized(@NonNull Bundle extras) {
        if (mCustomTabsCallback != null) {
            mCustomTabsCallback.onMinimized(extras);
        }
    }

    public void onUnminimized(@NonNull Bundle extras) {
        if (mCustomTabsCallback != null) {
            mCustomTabsCallback.onUnminimized(extras);
        }
    }
}
