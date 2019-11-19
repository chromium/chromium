// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.provider;

import android.content.ContentProvider;
import android.content.ContentResolver;
import android.content.pm.ProviderInfo;
import android.test.IsolatedContext;
import android.test.mock.MockContentResolver;

import org.junit.Assert;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Base class for Chrome's ContentProvider tests.
 * Sets up a local ChromeBrowserProvider associated to a mock resolver in an isolated context.
 */
public class ProviderTestRule extends ChromeActivityTestRule<ChromeActivity> {
    private IsolatedContext mContext;

    public ProviderTestRule() {
        super(ChromeActivity.class);
    }

    @Override
    public Statement apply(final Statement base, Description description) {
        return super.apply(new Statement() {
            @Override
            public void evaluate() throws Throwable {
                setUp();
                base.evaluate();
            }
        }, description);
    }

    private void setUp() throws Exception {
        startMainActivityOnBlankPage();

        final ChromeActivity activity = getActivity();
        Assert.assertNotNull(activity);

        final ContentProvider provider = new ChromeBrowserProvider();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ProviderInfo providerInfo = new ProviderInfo();
            providerInfo.authority = ChromeBrowserProvider.getApiAuthority(activity);
            provider.attachInfo(activity, providerInfo);
        });

        MockContentResolver resolver = new MockContentResolver();
        resolver.addProvider(ChromeBrowserProvider.getApiAuthority(activity), provider);

        mContext = new IsolatedContext(resolver, activity);
        Assert.assertTrue(getContentResolver() instanceof MockContentResolver);
    }

    protected ContentResolver getContentResolver() {
        return mContext.getContentResolver();
    }
}
