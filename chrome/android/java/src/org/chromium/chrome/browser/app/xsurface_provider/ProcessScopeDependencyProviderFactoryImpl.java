// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.xsurface_provider;

import org.chromium.build.annotations.ServiceImpl;
import org.chromium.chrome.GoogleAPIKeys;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.xsurface.ProcessScopeDependencyProvider;
import org.chromium.chrome.browser.xsurface_provider.ProcessScopeDependencyProviderFactory;
import org.chromium.chrome.browser.xsurface_provider.ProcessScopeDependencyProviderImpl;

/** Implements the provider factory. */
@ServiceImpl(ProcessScopeDependencyProviderFactory.class)
public class ProcessScopeDependencyProviderFactoryImpl
        implements ProcessScopeDependencyProviderFactory {

    @Override
    public ProcessScopeDependencyProvider create() {
        return new ProcessScopeDependencyProviderImpl(
                GoogleAPIKeys.GOOGLE_API_KEY, PrivacyPreferencesManagerImpl.getInstance());
    }
}
