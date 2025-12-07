// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.settings;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.components.embedder_support.util.PasswordEchoSettingObserver;
import org.chromium.components.embedder_support.util.PasswordEchoSettingState;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.BrowserContextHandle;

/** Manages the password echo (show last typed character) feature state for a given profile. */
@NullMarked
public class PasswordEchoSettingHandler implements Destroyable, PasswordEchoSettingObserver {
    private final BrowserContextHandle mBrowserContextHandle;
    private final PasswordEchoSettingState mState;

    public PasswordEchoSettingHandler(BrowserContextHandle browserContextHandle) {
        mBrowserContextHandle = browserContextHandle;

        mState = PasswordEchoSettingState.getInstance();
        mState.registerObserver(this);
    }

    @Override
    public void destroy() {
        mState.unregisterObserver(this);
        updatePasswordEchoState();
    }

    /**
     * Honor the Android system setting about showing the last character of a password for a short
     * period of time.
     */
    public void updatePasswordEchoState() {
        UserPrefs.get(mBrowserContextHandle)
                .setBoolean(
                        Pref.WEB_KIT_PASSWORD_ECHO_ENABLED_PHYSICAL,
                        mState.getPasswordEchoEnabledPhysical());
        UserPrefs.get(mBrowserContextHandle)
                .setBoolean(
                        Pref.WEB_KIT_PASSWORD_ECHO_ENABLED_TOUCH,
                        mState.getPasswordEchoEnabledTouch());
    }

    @Override
    public void onSettingChange() {
        updatePasswordEchoState();
    }
}
