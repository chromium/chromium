// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.provider;

import android.content.ContentProvider;
import android.content.ContentResolver;
import android.content.Context;
import android.content.pm.ProviderInfo;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.AdvancedMockContext;

/**
 * Base class for Chrome's ContentProvider tests. Sets up a local ChromeBrowserProvider associated
 * to a mock resolver in an isolated context.
 */
public class ProviderTestRule implements TestRule {
    private AdvancedMockContext mContext;

    public ProviderTestRule() {}

    @Override
    public Statement apply(final Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                setUp();
            }
        };
    }

    private void setUp() throws Exception {
        Context context = ContextUtils.getApplicationContext();
        mContext = new AdvancedMockContext(context);

        final ContentProvider provider = new ChromeBrowserProvider();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ProviderInfo providerInfo = new ProviderInfo();
                    providerInfo.authority = ChromeBrowserProviderImpl.getApiAuthority(context);
                    provider.attachInfo(context, providerInfo);
                });

        mContext.getMockContentResolver()
                .addProvider(ChromeBrowserProviderImpl.getApiAuthority(context), provider);
    }

    protected ContentResolver getContentResolver() {
        return mContext.getContentResolver();
    }
}
