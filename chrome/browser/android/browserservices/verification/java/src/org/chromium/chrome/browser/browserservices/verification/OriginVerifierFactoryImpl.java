// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.verification;

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsService;

import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.content_public.browser.WebContents;

import javax.inject.Inject;

import dagger.Reusable;

/**
 * The main implementation of {@link OriginVerifierFactory} used in production.
 */
@Reusable
public class OriginVerifierFactoryImpl implements OriginVerifierFactory {
    @Inject
    public OriginVerifierFactoryImpl() {}

    @Override
    public OriginVerifier create(String packageName, @CustomTabsService.Relation int relation,
            @Nullable WebContents webContents, @Nullable ExternalAuthUtils externalAuthUtils,
            OriginVerifier.MetricsListener metricsListener) {
        return new OriginVerifier(packageName, relation, webContents, externalAuthUtils,
                metricsListener, VerificationResultStore.getInstance());
    }

    @Override
    public OriginVerifier create(String packageName, @CustomTabsService.Relation int relation,
            @Nullable WebContents webContents, @Nullable ExternalAuthUtils externalAuthUtils) {
        return create(packageName, relation, webContents, externalAuthUtils,
                new OriginVerifier.MetricsListener() {});
    }
}
