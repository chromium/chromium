// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.sessionrestore;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityContentTestEnvironment;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Unit test related to {@link SessionRestoreManager}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class SessionRestoreManagerUnitTest {
    @Rule
    public CustomTabActivityContentTestEnvironment env =
            new CustomTabActivityContentTestEnvironment();
    @Rule
    public TestRule mFeatureProcessor = new Features.JUnitProcessor();

    @Before
    public void setup() {
        Mockito.doCallRealMethod().when(env.connection).getSessionRestoreManager();
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.CCT_RETAINING_STATE_IN_MEMORY)
    public void getSessionManagerWithFeature() {
        Assert.assertNotNull(
                "SessionRestoreManager is null.", env.connection.getSessionRestoreManager());
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.CCT_RETAINING_STATE_IN_MEMORY)
    public void nullSessionManagerWithoutFeature() {
        Assert.assertNull(
                "SessionRestoreManager should be null.", env.connection.getSessionRestoreManager());
    }
}
