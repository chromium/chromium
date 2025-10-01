// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.database.ContentObserver;
import android.net.Uri;
import android.os.Handler;
import android.provider.Settings;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Password echo setting controller for Webview.
 *
 * <p>The password echo setting (aka. "Show passwords" setting) is an Android setting that controls
 * whether the last character typed in a password field is momentarily visible before being replaced
 * by a mask (dot or asterisk).
 *
 * <p>This class syncs the Android setting with Webview's setting.
 */
@NullMarked
public class AwPasswordEchoSettingController extends ContentObserver {
    private final AwSettings mSettings;
    private final Context mContext;

    public AwPasswordEchoSettingController(AwSettings settings, Context context) {
        super(new Handler());
        mSettings = settings;
        mContext = context;
    }

    @Override
    public void onChange(boolean selfChange) {
        onChange(selfChange, null);
    }

    @Override
    public void onChange(boolean selfChange, @Nullable Uri uri) {
        updatePasswordEchoState();
    }

    public void onAttachedToWindow() {
        mContext.getContentResolver()
                .registerContentObserver(
                        Settings.System.getUriFor(Settings.System.TEXT_SHOW_PASSWORD), false, this);
        updatePasswordEchoState();
    }

    public void onDetachedFromWindow() {
        mContext.getContentResolver().unregisterContentObserver(this);
    }

    private void updatePasswordEchoState() {
        boolean enabled =
                Settings.System.getInt(
                                mContext.getContentResolver(),
                                Settings.System.TEXT_SHOW_PASSWORD,
                                1)
                        == 1;
        mSettings.setPasswordEchoEnabled(enabled);
    }
}
