// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import org.chromium.base.annotations.IdentifierNameString;
import org.chromium.chrome.browser.base.SplitCompatContentProvider;

/** See {@link AttributionReportingProviderImpl}. */
public class AttributionReportingProvider extends SplitCompatContentProvider {
    @IdentifierNameString
    private static final String IMPL_CLASS = "org.chromium.chrome.browser.attribution_reporting"
            + ".AttributionReportingProviderImpl";

    public AttributionReportingProvider() {
        super(IMPL_CLASS);
    }
}
