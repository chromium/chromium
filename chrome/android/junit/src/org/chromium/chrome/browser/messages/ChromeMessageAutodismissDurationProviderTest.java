// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.messages;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.content.Context;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.accessibility.AccessibilityStateJUnitTestHelper;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link ChromeMessageAutodismissDurationProvider}. */
@Config(shadows = {AccessibilityStateJUnitTestHelper.ShadowAccessibilityServiceInfo.class})
@RunWith(BaseRobolectricTestRunner.class)
public class ChromeMessageAutodismissDurationProviderTest {
    private Context mContext;

    @Before
    public void setUp() throws Exception {
        AccessibilityStateJUnitTestHelper.mockAccessibilityStateJni();
        mContext = RuntimeEnvironment.getApplication();

        AccessibilityStateJUnitTestHelper.setEnabledAccessibilityServiceList(
                mContext, new ArrayList<>());
        AccessibilityStateJUnitTestHelper.updateAccessibilityServices();
    }

    @After
    public void tearDown() {
        AccessibilityState.uninitializeForTesting();
    }

    private void setAccessibilityServices(int capabilities) {
        AccessibilityServiceInfo service =
                new AccessibilityStateJUnitTestHelper.BuilderForTests()
                        .setCapabilities(capabilities)
                        .build();
        AccessibilityStateJUnitTestHelper.setEnabledAccessibilityServiceList(
                mContext, List.of(service));
        AccessibilityStateJUnitTestHelper.updateAccessibilityServices();
    }

    @Test
    public void testDefaultNonA11yDuration() {
        ChromeMessageAutodismissDurationProvider provider =
                new ChromeMessageAutodismissDurationProvider();
        provider.setDefaultAutodismissDurationMsForTesting(500);
        provider.setDefaultAutodismissDurationWithA11yMsForTesting(1000);
        Assert.assertEquals(
                "Provider should return default non-a11y duration if no gesture performing "
                        + "a11y services are running.",
                500,
                provider.get(MessageIdentifier.TEST_MESSAGE, 0));
    }

    @Test
    public void testA11yDuration() {
        setAccessibilityServices(AccessibilityServiceInfo.CAPABILITY_CAN_PERFORM_GESTURES);

        ChromeMessageAutodismissDurationProvider provider =
                new ChromeMessageAutodismissDurationProvider();
        provider.setDefaultAutodismissDurationMsForTesting(500);
        provider.setDefaultAutodismissDurationWithA11yMsForTesting(1000);
        Assert.assertEquals(
                "Provider should return default a11y duration if any gesture performing "
                        + "a11y services are running.",
                1000,
                provider.get(MessageIdentifier.TEST_MESSAGE, 0));
    }

    @Test
    public void testCustomDuration() {
        setAccessibilityServices(/* capabilities= */ 0);

        ChromeMessageAutodismissDurationProvider provider =
                new ChromeMessageAutodismissDurationProvider();
        provider.setDefaultAutodismissDurationMsForTesting(500);
        provider.setDefaultAutodismissDurationWithA11yMsForTesting(1000);
        Assert.assertEquals(
                "Provider should return custom non-a11y duration if no gesture performing "
                        + "a11y services are running.",
                1500,
                provider.get(MessageIdentifier.TEST_MESSAGE, 1500));
        Assert.assertEquals(
                "Provider should return default non-a11y duration if custom duration is too short",
                500,
                provider.get(MessageIdentifier.TEST_MESSAGE, 250));

        setAccessibilityServices(AccessibilityServiceInfo.CAPABILITY_CAN_PERFORM_GESTURES);

        Assert.assertEquals(
                "Provider should return custom a11y duration if any gesture performing "
                        + "a11y services are running.",
                1500,
                provider.get(MessageIdentifier.TEST_MESSAGE, 1500));
        Assert.assertEquals(
                "Provider should return default a11y duration if custom duration is too short "
                        + "and any gesture performing a11y services are running.",
                1000,
                provider.get(MessageIdentifier.TEST_MESSAGE, 250));
    }
}
