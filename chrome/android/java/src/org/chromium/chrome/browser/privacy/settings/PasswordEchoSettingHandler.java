// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.settings;

import android.database.ContentObserver;
import android.net.Uri;
import android.os.Handler;
import android.provider.Settings;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.BrowserContextHandle;

/** Manages the password echo (show last typed character) feature state for a given profile. */
@NullMarked
public class PasswordEchoSettingHandler implements Destroyable {
    private final BrowserContextHandle mBrowserContextHandle;
    private final PasswordEchoSettingObserver mPasswordEchoSettingObserver;

    public PasswordEchoSettingHandler(BrowserContextHandle browserContextHandle) {
        mBrowserContextHandle = browserContextHandle;

        mPasswordEchoSettingObserver = new PasswordEchoSettingObserver();
        ContextUtils.getApplicationContext()
                .getContentResolver()
                .registerContentObserver(
                        Settings.System.getUriFor(Settings.System.TEXT_SHOW_PASSWORD),
                        false,
                        mPasswordEchoSettingObserver);
    }

    @Override
    public void destroy() {
        ContextUtils.getApplicationContext()
                .getContentResolver()
                .unregisterContentObserver(mPasswordEchoSettingObserver);
    }

    /**
     * Honor the Android system setting about showing the last character of a password for a short
     * period of time.
     */
    public void updatePasswordEchoState() {
        boolean systemEnabled =
                Settings.System.getInt(
                                ContextUtils.getApplicationContext().getContentResolver(),
                                Settings.System.TEXT_SHOW_PASSWORD,
                                1)
                        == 1;
        UserPrefs.get(mBrowserContextHandle)
                .setBoolean(Pref.WEB_KIT_PASSWORD_ECHO_ENABLED, systemEnabled);
    }

    private class PasswordEchoSettingObserver extends ContentObserver {
        public PasswordEchoSettingObserver() {
            super(new Handler());
        }

        @Override
        public void onChange(boolean selfChange) {
            onChange(selfChange, null);
        }

        @Override
        public void onChange(boolean selfChange, @Nullable Uri uri) {
            updatePasswordEchoState();
        }
    }

    @VisibleForTesting
    ContentObserver getPasswordEchoSettingObserver() {
        return mPasswordEchoSettingObserver;
    }
}
