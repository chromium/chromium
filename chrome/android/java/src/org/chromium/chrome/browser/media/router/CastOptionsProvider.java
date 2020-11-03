// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router;

import android.content.Context;

import com.google.android.gms.cast.LaunchOptions;
import com.google.android.gms.cast.framework.CastOptions;
import com.google.android.gms.cast.framework.OptionsProvider;
import com.google.android.gms.cast.framework.SessionProvider;

import java.util.List;

/**
 * {@link OptionsProvider} implementation for Chrome media router.
 *
 * This has to live in the base java module for it to be loaded under isolated splits.
 */
public class CastOptionsProvider implements OptionsProvider {
    @Override
    public CastOptions getCastOptions(Context context) {
        return new CastOptions.Builder()
                .setCastMediaOptions(null)
                .setEnableReconnectionService(false)
                .setLaunchOptions(new LaunchOptions.Builder().setRelaunchIfRunning(true).build())
                .setResumeSavedSession(false)
                .setStopReceiverApplicationWhenEndingSession(true)
                .build();
    }

    @Override
    public List<SessionProvider> getAdditionalSessionProviders(Context context) {
        return null;
    }
}
