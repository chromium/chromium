// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.customtabsclient;

import androidx.browser.auth.AuthTabSession;
import androidx.browser.customtabs.CustomTabsSession;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.ref.WeakReference;

/**
 * A class that keeps tracks of the current {@link CustomTabsSession} or {@link AuthTabSession} and
 * helps other components of the app to get access to the current session.
 */
@NullMarked
public class SessionHelper {
    private static @Nullable WeakReference<CustomTabsSession> sCurrentSession;
    private static @Nullable WeakReference<AuthTabSession> sCurrentAuthTabSession;

    /**
     * @return The current {@link CustomTabsSession} object.
     */
    public static @Nullable CustomTabsSession getCurrentSession() {
        return sCurrentSession == null ? null : sCurrentSession.get();
    }

    /**
     * Sets the current session to the given one.
     *
     * @param session The current session.
     */
    public static void setCurrentSession(@Nullable CustomTabsSession session) {
        sCurrentSession = new WeakReference<CustomTabsSession>(session);
    }

    /** Returns the current {@link AuthTabSession} object. */
    public static @Nullable AuthTabSession getCurrentAuthSession() {
        return sCurrentAuthTabSession == null ? null : sCurrentAuthTabSession.get();
    }

    /**
     * Sets the current session to the given one.
     *
     * @param session The current session.
     */
    public static void setCurrentAuthSession(@Nullable AuthTabSession session) {
        sCurrentAuthTabSession = new WeakReference<AuthTabSession>(session);
    }
}
