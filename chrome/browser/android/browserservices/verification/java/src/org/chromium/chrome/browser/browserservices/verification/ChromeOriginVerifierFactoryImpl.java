// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.verification;

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsService;

import dagger.Reusable;

import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.content_public.browser.WebContents;

import javax.inject.Inject;

/** The main implementation of {@link ChromeOriginVerifierFactory} used in production. */
@Reusable
public class ChromeOriginVerifierFactoryImpl implements ChromeOriginVerifierFactory {
    @Inject
    public ChromeOriginVerifierFactoryImpl() {}

    @Override
    public ChromeOriginVerifier create(
            String packageName,
            @CustomTabsService.Relation int relation,
            @Nullable WebContents webContents,
            @Nullable ExternalAuthUtils externalAuthUtils) {
        return new ChromeOriginVerifier(
                packageName,
                relation,
                webContents,
                externalAuthUtils,
                ChromeVerificationResultStore.getInstance());
    }
}
