// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.messages;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessagesMetrics;

/**
 * Unit tests for {@link ChromeMessageAutodismissDurationProvider}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChromeMessageAutodismissDurationProviderTest {
    private TestValues mFeatureTestValues;

    @Before
    public void setUp() {
        mFeatureTestValues = new TestValues();
        mFeatureTestValues.addFeatureFlagOverride(
                ChromeFeatureList.MESSAGES_FOR_ANDROID_INFRASTRUCTURE, true);

        FeatureList.setTestValues(mFeatureTestValues);
    }

    @Test
    public void testDefaultNonA11yDuration() {
        ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(false);
        ChromeMessageAutodismissDurationProvider provider =
                new ChromeMessageAutodismissDurationProvider();
        provider.setDefaultAutodismissDurationMsForTesting(500);
        provider.setDefaultAutodismissDurationWithA11yMsForTesting(1000);
        Assert.assertEquals("Provider should return default non-a11y duration if a11y is off", 500,
                provider.get(MessageIdentifier.TEST_MESSAGE, 0));
    }

    @Test
    public void testA11yDuration() {
        ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(true);
        ChromeMessageAutodismissDurationProvider provider =
                new ChromeMessageAutodismissDurationProvider();
        provider.setDefaultAutodismissDurationMsForTesting(500);
        provider.setDefaultAutodismissDurationWithA11yMsForTesting(1000);
        Assert.assertEquals("Provider should return default a11y duration if a11y is on", 1000,
                provider.get(MessageIdentifier.TEST_MESSAGE, 0));
    }

    @Test
    public void testCustomDuration() {
        ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(false);
        ChromeMessageAutodismissDurationProvider provider =
                new ChromeMessageAutodismissDurationProvider();
        provider.setDefaultAutodismissDurationMsForTesting(500);
        provider.setDefaultAutodismissDurationWithA11yMsForTesting(1000);
        Assert.assertEquals("Provider should return custom non-a11y duration if a11y is off", 1500,
                provider.get(MessageIdentifier.TEST_MESSAGE, 1500));
        Assert.assertEquals(
                "Provider should return default non-a11y duration if custom duration is too short",
                500, provider.get(MessageIdentifier.TEST_MESSAGE, 250));
        ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(true);
        Assert.assertEquals("Provider should return custom a11y duration if a11y is on", 1500,
                provider.get(MessageIdentifier.TEST_MESSAGE, 1500));
        Assert.assertEquals(
                "Provider should return default a11y duration if custom duration is too short",
                1000, provider.get(MessageIdentifier.TEST_MESSAGE, 250));
    }

    @Test
    public void testFeatureCustomDuration() {
        ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(false);

        mFeatureTestValues.addFieldTrialParamOverride(
                ChromeFeatureList.MESSAGES_FOR_ANDROID_INFRASTRUCTURE,
                ChromeMessageAutodismissDurationProvider
                                .FEATURE_SPECIFIC_FINCH_CONTROLLED_DURATION_PREFIX
                        + MessagesMetrics.messageIdentifierToHistogramSuffix(
                                MessageIdentifier.TEST_MESSAGE),
                "2000");
        FeatureList.setTestValues(mFeatureTestValues);
        ChromeMessageAutodismissDurationProvider provider =
                new ChromeMessageAutodismissDurationProvider();
        provider.setDefaultAutodismissDurationMsForTesting(500);
        provider.setDefaultAutodismissDurationWithA11yMsForTesting(1000);
        Assert.assertEquals("Provider should return finch custom non-a11y duration if a11y is off",
                2000, provider.get(MessageIdentifier.TEST_MESSAGE, 1500));

        Assert.assertEquals(
                "Provider should return default non-a11y duration if finch parameter is not set",
                1000, provider.get(MessageIdentifier.INVALID_MESSAGE, 1000));
    }
}
