// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.verification;

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsService.Relation;

import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.content_public.browser.WebContents;

/**
 * TODO(swestphal): Temporary class for compatibility with downstream targets. Remove after
 * dependencies are migrated.
 */
public class OriginVerifier extends ChromeOriginVerifier {
    public OriginVerifier(String packageName, @Relation int relation,
            @Nullable WebContents webContents, @Nullable ExternalAuthUtils externalAuthUtils,
            ChromeVerificationResultStore verificationResultStore) {
        super(packageName, relation, webContents, externalAuthUtils, verificationResultStore);
    }
}