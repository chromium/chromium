// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.components.browser_ui.settings.ImageButtonPreference;

/**
 * A Preference to represent a site using FLEDGE.
 */
public class FledgePreference extends ImageButtonPreference {
    private final @NonNull String mSite;

    // TODO(crbug.com/1334933): Add favicon.
    public FledgePreference(Context context, @NonNull String site) {
        super(context);
        mSite = site;
        setTitle(site);
    }

    @NonNull
    public String getSite() {
        return mSite;
    }
}
