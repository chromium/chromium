// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.intents;

import android.content.Intent;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.OptIn;
import androidx.browser.auth.AuthTabIntent;
import androidx.browser.auth.AuthTabSessionToken;
import androidx.browser.auth.ExperimentalAuthTab;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.base.IntentUtils;

/**
 * Class that holds either a {@link CustomTabsSessionToken} or {@link AuthTabSessionToken}.
 *
 * @param <T> The type of the session; either {@link CustomTabsSessionToken} or {@link
 *     AuthTabSessionToken}.
 */
@OptIn(markerClass = ExperimentalAuthTab.class)
public class SessionHolder<T> {
    @NonNull private final T mSession;

    public SessionHolder(@NonNull T session) {
        mSession = session;
        assert isCustomTab() || isAuthTab();
    }

    @Nullable
    public static SessionHolder<?> getSessionHolderFromIntent(Intent intent) {
        boolean isAuthTab =
                IntentUtils.safeGetBooleanExtra(intent, AuthTabIntent.EXTRA_LAUNCH_AUTH_TAB, false);
        if (isAuthTab) {
            AuthTabSessionToken token = AuthTabSessionToken.getSessionTokenFromIntent(intent);
            if (token != null) {
                return new SessionHolder<>(token);
            }
        } else {
            CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
            if (token != null) {
                return new SessionHolder<>(token);
            }
        }
        return null;
    }

    /** Returns the session held by this object. */
    @NonNull
    public T getSession() {
        return mSession;
    }

    /** Returns the session as a {@link CustomTabsSessionToken}. */
    @NonNull
    public CustomTabsSessionToken getSessionAsCustomTab() {
        return (CustomTabsSessionToken) mSession;
    }

    /** Returns the session as an {@link AuthTabSessionToken}. */
    @NonNull
    public AuthTabSessionToken getSessionAsAuthTab() {
        return (AuthTabSessionToken) mSession;
    }

    /** Whether the session has an id. */
    public boolean hasId() {
        if (mSession instanceof AuthTabSessionToken session) {
            return session.hasId();
        } else if (mSession instanceof CustomTabsSessionToken session) {
            return session.hasId();
        }
        return false;
    }

    @Override
    public int hashCode() {
        return mSession.hashCode();
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (obj instanceof SessionHolder<?> holder) {
            return mSession.equals(holder.mSession);
        }
        return false;
    }

    /** Returns whether the session is a {@link CustomTabsSessionToken}. */
    public boolean isCustomTab() {
        return mSession instanceof CustomTabsSessionToken;
    }

    /** Returns whether the session is an {@link AuthTabSessionToken}. */
    public boolean isAuthTab() {
        return mSession instanceof AuthTabSessionToken;
    }
}
