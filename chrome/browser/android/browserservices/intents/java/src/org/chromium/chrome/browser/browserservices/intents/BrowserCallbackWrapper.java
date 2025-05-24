// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.intents;

import android.net.Uri;
import android.os.Bundle;

import androidx.browser.auth.AuthTabCallback;
import androidx.browser.auth.AuthTabSessionToken;
import androidx.browser.customtabs.CustomTabsCallback;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Class that holds either a {@link CustomTabsSessionToken} or {@link AuthTabSessionToken}. */
@NullMarked
public class BrowserCallbackWrapper {
    private final @Nullable CustomTabsCallback mCustomTabsCallback;
    private final @Nullable AuthTabCallback mAuthTabCallback;

    public BrowserCallbackWrapper(CustomTabsCallback callback) {
        mCustomTabsCallback = callback;
        mAuthTabCallback = null;
    }

    public BrowserCallbackWrapper(AuthTabCallback callback) {
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

    public void extraCallback(String callbackName, @Nullable Bundle args) {
        if (mCustomTabsCallback != null) {
            mCustomTabsCallback.extraCallback(callbackName, args);
        } else {
            assert mAuthTabCallback != null;
            mAuthTabCallback.onExtraCallback(callbackName, args != null ? args : Bundle.EMPTY);
        }
    }

    public @Nullable Bundle extraCallbackWithResult(String callbackName, @Nullable Bundle args) {
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

    public void onPostMessage(String message, @Nullable Bundle extras) {
        if (mCustomTabsCallback != null) {
            mCustomTabsCallback.onPostMessage(message, extras);
        }
    }

    public void onRelationshipValidationResult(
            int relation, Uri requestedOrigin, boolean result, @Nullable Bundle extras) {
        if (mCustomTabsCallback != null) {
            mCustomTabsCallback.onRelationshipValidationResult(
                    relation, requestedOrigin, result, extras);
        }
    }

    public void onActivityResized(int height, int width, Bundle extras) {
        if (mCustomTabsCallback != null) {
            mCustomTabsCallback.onActivityResized(height, width, extras);
        }
    }

    public void onWarmupCompleted(Bundle extras) {
        if (mCustomTabsCallback != null) {
            mCustomTabsCallback.onWarmupCompleted(extras);
        } else {
            assert mAuthTabCallback != null;
            mAuthTabCallback.onWarmupCompleted(extras);
        }
    }

    public void onActivityLayout(
            int left, int top, int right, int bottom, int state, Bundle extras) {
        if (mCustomTabsCallback != null) {
            mCustomTabsCallback.onActivityLayout(left, top, right, bottom, state, extras);
        }
    }

    public void onMinimized(Bundle extras) {
        if (mCustomTabsCallback != null) {
            mCustomTabsCallback.onMinimized(extras);
        }
    }

    public void onUnminimized(Bundle extras) {
        if (mCustomTabsCallback != null) {
            mCustomTabsCallback.onUnminimized(extras);
        }
    }
}
