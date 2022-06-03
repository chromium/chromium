// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.support.test.InstrumentationRegistry;

import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;

class AutofillAssistantCustomTabTestRule
        extends AutofillAssistantTestRule<CustomTabActivityTestRule> {
    private static final String HTML_DIRECTORY = "/components/test/data/autofill_assistant/html/";

    private final String mTestPage;

    AutofillAssistantCustomTabTestRule(CustomTabActivityTestRule testRule, String testPage) {
        super(testRule);
        mTestPage = testPage;
    }

    @Override
    public void startActivity() {
        getTestRule().startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(),
                        getTestRule().getTestServer().getURL(HTML_DIRECTORY + mTestPage)));
    }

    @Override
    public void cleanupAfterTest() {}
}
