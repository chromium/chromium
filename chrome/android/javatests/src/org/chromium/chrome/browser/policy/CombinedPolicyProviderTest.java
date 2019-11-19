// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.policy.CombinedPolicyProvider;
import org.chromium.policy.PolicyProvider;

/** Instrumentation tests for {@link CombinedPolicyProvider} */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class CombinedPolicyProviderTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String DATA_URI = "data:text/plain;charset=utf-8;base64,dGVzdA==";

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    /**
     * Checks that the {@link CombinedPolicyProvider} properly notifies tabs when incognito mode is
     * disabled.
     */
    @Test
    @Feature({"Policy"})
    @SmallTest
    @RetryOnFailure
    public void testTerminateIncognitoSon() {
        final boolean incognitoMode = true;

        TabModel incognitoTabModel =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(incognitoMode);
        mActivityTestRule.loadUrlInNewTab(DATA_URI, incognitoMode);
        mActivityTestRule.loadUrlInNewTab(DATA_URI, incognitoMode);
        Assert.assertEquals(2, incognitoTabModel.getCount());

        final CombinedPolicyProvider provider = CombinedPolicyProvider.get();
        TestThreadUtils.runOnUiThreadBlocking(() -> provider.registerProvider(new PolicyProvider() {
            @Override
            public void refresh() {
                terminateIncognitoSession();
            }
        }));

        Assert.assertEquals(0, incognitoTabModel.getCount());
    }
}
