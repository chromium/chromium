// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.intents;

import android.app.PendingIntent;
import android.content.Intent;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Parcel;
import android.os.Parcelable;

import androidx.browser.auth.AuthTabIntent;
import androidx.browser.auth.AuthTabSessionToken;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.base.IntentUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Holds either a {@link CustomTabsSessionToken} or an {@link AuthTabSessionToken}.
 *
 * <p>The hierarchy is sealed, so a session is always exactly one of {@link CustomTab} or {@link
 * AuthTab}. To reach the underlying token, use a {@code switch} with a case for each: the compiler
 * rejects the switch if either case is missing.
 *
 * <pre>{@code
 * var callback = switch (sessionHolder) {
 *     case SessionHolder.CustomTab customTab -> customTab.getToken().getCallback();
 *     case SessionHolder.AuthTab authTab -> authTab.getToken().getCallback();
 * };
 * }</pre>
 */
@NullMarked
public sealed interface SessionHolder extends Parcelable
        permits SessionHolder.CustomTab, SessionHolder.AuthTab {
    /** Returns a holder for the given Custom Tab session. */
    static CustomTab of(CustomTabsSessionToken token) {
        return new CustomTab(token);
    }

    /** Returns a holder for the given Auth Tab session. */
    static AuthTab of(AuthTabSessionToken token) {
        return new AuthTab(token);
    }

    /**
     * Returns a holder for the session the given intent belongs to, or null if the intent does not
     * have a session.
     */
    static @Nullable SessionHolder getSessionHolderFromIntent(Intent intent) {
        boolean isAuthTab =
                IntentUtils.safeGetBooleanExtra(intent, AuthTabIntent.EXTRA_LAUNCH_AUTH_TAB, false);
        if (isAuthTab) {
            AuthTabSessionToken token = AuthTabSessionToken.createSessionTokenFromIntent(intent);
            return token != null ? of(token) : null;
        }
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        if (token == null) return null;
        // Unlike AuthTabSessionToken, CustomTabsSessionToken#getCallbackBinder() and #getId() are
        // package-private, so we extract and retain them from the intent here to support
        // Parcelable serialization.
        IBinder callbackBinder =
                IntentUtils.safeGetBinderExtra(intent, CustomTabsIntent.EXTRA_SESSION);
        PendingIntent sessionId =
                IntentUtils.safeGetParcelableExtra(intent, CustomTabsIntent.EXTRA_SESSION_ID);
        return new CustomTab(token, callbackBinder, sessionId);
    }

    @Override
    default int describeContents() {
        return 0;
    }

    Parcelable.Creator<SessionHolder> CREATOR =
            new Parcelable.Creator<>() {
                @Override
                public @Nullable SessionHolder createFromParcel(Parcel in) {
                    // Reads the values written in `CustomTab#writeToParcel` and
                    // `AuthTab#writeToParcel`.
                    boolean isAuthTab = in.readBoolean();
                    IBinder callbackBinder = in.readStrongBinder();
                    PendingIntent sessionId = in.readTypedObject(PendingIntent.CREATOR);
                    Bundle bundle = new Bundle();
                    bundle.putBoolean(AuthTabIntent.EXTRA_LAUNCH_AUTH_TAB, isAuthTab);
                    bundle.putBinder(CustomTabsIntent.EXTRA_SESSION, callbackBinder);
                    bundle.putParcelable(CustomTabsIntent.EXTRA_SESSION_ID, sessionId);
                    return getSessionHolderFromIntent(new Intent().putExtras(bundle));
                }

                @Override
                public SessionHolder[] newArray(int size) {
                    return new SessionHolder[size];
                }
            };

    /** Whether the session has an id. */
    boolean hasId();

    /**
     * Returns whether the session is a {@link CustomTabsSessionToken}. Prefer {@code instanceof
     * SessionHolder.CustomTab customTab} when the token itself is needed, since the pattern binds
     * it without a cast.
     */
    default boolean isCustomTab() {
        return this instanceof CustomTab;
    }

    /**
     * Returns whether the session is an {@link AuthTabSessionToken}. Prefer {@code instanceof
     * SessionHolder.AuthTab authTab} when the token itself is needed, since the pattern binds it
     * without a cast.
     */
    default boolean isAuthTab() {
        return this instanceof AuthTab;
    }

    /** Holds the session of a Custom Tab. */
    final class CustomTab implements SessionHolder {
        private final CustomTabsSessionToken mToken;
        private final @Nullable IBinder mCallbackBinder;
        private final @Nullable PendingIntent mSessionId;

        private CustomTab(CustomTabsSessionToken token) {
            this(token, null, null);
        }

        private CustomTab(
                CustomTabsSessionToken token,
                @Nullable IBinder callbackBinder,
                @Nullable PendingIntent sessionId) {
            mToken = token;
            mCallbackBinder = callbackBinder;
            mSessionId = sessionId;
        }

        /** Returns the token identifying the Custom Tab session. */
        public CustomTabsSessionToken getToken() {
            return mToken;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            assert mCallbackBinder != null || mSessionId != null;
            dest.writeBoolean(false);
            dest.writeStrongBinder(mCallbackBinder);
            dest.writeTypedObject(mSessionId, flags);
        }

        @Override
        public boolean hasId() {
            return mToken.hasId();
        }

        @Override
        public boolean equals(@Nullable Object obj) {
            return obj instanceof CustomTab other && mToken.equals(other.mToken);
        }

        @Override
        public int hashCode() {
            return mToken.hashCode();
        }
    }

    /** Holds the session of an Auth Tab. */
    final class AuthTab implements SessionHolder {
        private final AuthTabSessionToken mToken;

        private AuthTab(AuthTabSessionToken token) {
            mToken = token;
        }

        /** Returns the token identifying the Auth Tab session. */
        public AuthTabSessionToken getToken() {
            return mToken;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeBoolean(true);
            dest.writeStrongBinder(mToken.getCallbackBinder());
            dest.writeTypedObject(mToken.getId(), flags);
        }

        @Override
        public boolean hasId() {
            return mToken.hasId();
        }

        @Override
        public boolean equals(@Nullable Object obj) {
            return obj instanceof AuthTab other && mToken.equals(other.mToken);
        }

        @Override
        public int hashCode() {
            return mToken.hashCode();
        }
    }
}
