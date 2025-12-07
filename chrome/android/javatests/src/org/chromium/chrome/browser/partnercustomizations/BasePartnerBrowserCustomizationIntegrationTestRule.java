// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import org.junit.rules.ExternalResource;

import org.chromium.chrome.test.partnercustomizations.TestPartnerBrowserCustomizationsProvider;

/** Basic shared functionality for partner customization integration tests. */
public class BasePartnerBrowserCustomizationIntegrationTestRule extends ExternalResource {
    public BasePartnerBrowserCustomizationIntegrationTestRule() {}

    @Override
    protected void before() throws Throwable {
        CustomizationProviderDelegateUpstreamImpl.ignoreBrowserProviderSystemPackageCheckForTesting(
                true);
        CustomizationProviderDelegateUpstreamImpl.setProviderAuthorityForTesting(
                TestPartnerBrowserCustomizationsProvider.class.getName());
    }
}
