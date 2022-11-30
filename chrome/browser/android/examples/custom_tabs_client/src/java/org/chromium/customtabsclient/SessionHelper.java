// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.customtabsclient;

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsSession;

import java.lang.ref.WeakReference;

/**
 * A class that keeps tracks of the current {@link CustomTabsSession} and helps other components of
 * the app to get access to the current session.
 */
public class SessionHelper {
    private static WeakReference<CustomTabsSession> sCurrentSession;

    /**
     * @return The current {@link CustomTabsSession} object.
     */
    public static @Nullable CustomTabsSession getCurrentSession() {
        return sCurrentSession == null ? null : sCurrentSession.get();
    }

    /**
     * Sets the current session to the given one.
     * @param session The current session.
     */
    public static void setCurrentSession(CustomTabsSession session) {
        sCurrentSession = new WeakReference<CustomTabsSession>(session);
    }
}
