// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.verification;

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsService;

import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.content_public.browser.WebContents;

/**
 * A factory that creates instances of {@link ChromeOriginVerifier}.
 *
 * Most classes that are Activity scoped should take an ChromeOriginVerifierFactory and create
 * OriginVerifiers as needed.
 */
public interface ChromeOriginVerifierFactory {
    /** Creates an {@link OriginVerifier}. */
    ChromeOriginVerifier create(
            String packageName,
            @CustomTabsService.Relation int relation,
            @Nullable WebContents webContents,
            @Nullable ExternalAuthUtils externalAuthUtils);
}
