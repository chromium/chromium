// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.partnercustomizations.TestPartnerBrowserCustomizationsProvider;

/** Basic shared functionality for partner customization integration tests. */
public class BasePartnerBrowserCustomizationIntegrationTestRule
        extends ChromeTabbedActivityTestRule {
    public BasePartnerBrowserCustomizationIntegrationTestRule() {}

    @Override
    protected void before() throws Throwable {
        CustomizationProviderDelegateUpstreamImpl.ignoreBrowserProviderSystemPackageCheckForTesting(
                true);
        CustomizationProviderDelegateUpstreamImpl.setProviderAuthorityForTesting(
                TestPartnerBrowserCustomizationsProvider.class.getName());
        super.before();
    }
}
