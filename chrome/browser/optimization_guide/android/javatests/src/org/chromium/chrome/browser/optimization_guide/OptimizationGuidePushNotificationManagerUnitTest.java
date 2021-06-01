// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.optimization_guide;

import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import com.google.protobuf.ByteString;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.optimization_guide.proto.CommonTypesProto.Any;
import org.chromium.components.optimization_guide.proto.HintsProto.KeyRepresentation;
import org.chromium.components.optimization_guide.proto.HintsProto.OptimizationType;
import org.chromium.components.optimization_guide.proto.PushNotificationProto.HintNotificationPayload;

import java.util.HashMap;
import java.util.Map;

/**
 * Unit tests for OptimizationGuidePushNotificationManager.
 */
@RunWith(BaseJUnit4ClassRunner.class)
// Batch this per class since the test is setting global feature state.
@Batch(Batch.PER_CLASS)
public class OptimizationGuidePushNotificationManagerUnitTest {
    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    private Profile mProfile;

    @Mock
    OptimizationGuideBridge.Natives mOptimizationGuideBridgeJniMock;

    private static final String TEST_URL = "https://testurl.com/";

    private static final HintNotificationPayload NOTIFICATION_WITH_PAYLOAD =
            HintNotificationPayload.newBuilder()
                    .setOptimizationType(OptimizationType.PERFORMANCE_HINTS)
                    .setKeyRepresentation(KeyRepresentation.FULL_URL)
                    .setHintKey("Testing")
                    .setPayload(Any.newBuilder()
                                        .setTypeUrl("com.testing")
                                        .setValue(ByteString.copyFrom(new byte[] {0, 1, 2, 3, 4}))
                                        .build())
                    .build();

    private static final HintNotificationPayload NOTIFICATION_WITHOUT_PAYLOAD =
            HintNotificationPayload.newBuilder(NOTIFICATION_WITH_PAYLOAD).clearPayload().build();

    private void setFeatureStatusForTest(boolean isEnabled) {
        Map<String, Boolean> testFeatures = new HashMap<String, Boolean>();
        testFeatures.put(ChromeFeatureList.OPTIMIZATION_GUIDE_PUSH_NOTIFICATIONS, isEnabled);
        FeatureList.setTestFeatures(testFeatures);

        CachedFeatureFlags.setForTesting(
                ChromeFeatureList.OPTIMIZATION_GUIDE_PUSH_NOTIFICATIONS, isEnabled);
    }

    @Before
    public void setUp() {
        resetFeatureFlags();

        MockitoAnnotations.initMocks(this);
        mocker.mock(OptimizationGuideBridgeJni.TEST_HOOKS, mOptimizationGuideBridgeJniMock);
        when(mOptimizationGuideBridgeJniMock.init()).thenReturn(1L);

        Profile.setLastUsedProfileForTesting(mProfile);
    }

    @After
    public void tearDown() {
        resetFeatureFlags();
    }

    public void resetFeatureFlags() {
        CachedFeatureFlags.resetFlagsForTesting();
        OptimizationGuidePushNotificationManager.clearCacheForAllTypes();
        OptimizationGuidePushNotificationManager.setNativeIsInitializedForTesting(null);
        FeatureList.setTestFeatures(null);
    }

    @Test
    @SmallTest
    public void testBasicSuccessCaseNoNative() {
        OptimizationGuidePushNotificationManager.setNativeIsInitializedForTesting(false);
        setFeatureStatusForTest(true);

        OptimizationGuidePushNotificationManager.onPushNotification(NOTIFICATION_WITH_PAYLOAD);
        Assert.assertFalse(OptimizationGuidePushNotificationManager
                                   .didNotificationCacheOverflowForOptimizationType(
                                           OptimizationType.PERFORMANCE_HINTS));

        HintNotificationPayload[] cached =
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                        OptimizationType.PERFORMANCE_HINTS);
        Assert.assertNotNull(cached);
        Assert.assertEquals(1, cached.length);
        Assert.assertEquals(NOTIFICATION_WITHOUT_PAYLOAD, cached[0]);

        // There should not be notifications for other types.
        Assert.assertEquals(0,
                OptimizationGuidePushNotificationManager
                        .getNotificationCacheForOptimizationType(OptimizationType.LITE_PAGE)
                        .length);

        OptimizationGuidePushNotificationManager.clearCacheForOptimizationType(
                OptimizationType.PERFORMANCE_HINTS);
        cached = OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                OptimizationType.PERFORMANCE_HINTS);
        Assert.assertNotNull(cached);
        Assert.assertEquals(0, cached.length);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testNativeCalled() {
        setFeatureStatusForTest(true);
        OptimizationGuidePushNotificationManager.setNativeIsInitializedForTesting(true);

        OptimizationGuidePushNotificationManager.onPushNotification(NOTIFICATION_WITHOUT_PAYLOAD);

        HintNotificationPayload[] cached =
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                        OptimizationType.PERFORMANCE_HINTS);
        Assert.assertNotNull(cached);
        Assert.assertEquals(0, cached.length);

        verify(mOptimizationGuideBridgeJniMock, times(1))
                .onNewPushNotification(anyLong(), eq(NOTIFICATION_WITHOUT_PAYLOAD.toByteArray()));
    }

    @Test
    @SmallTest
    public void testFeatureDisabled() {
        setFeatureStatusForTest(false);
        OptimizationGuidePushNotificationManager.setNativeIsInitializedForTesting(false);

        OptimizationGuidePushNotificationManager.onPushNotification(NOTIFICATION_WITH_PAYLOAD);
        Assert.assertFalse(OptimizationGuidePushNotificationManager
                                   .didNotificationCacheOverflowForOptimizationType(
                                           OptimizationType.PERFORMANCE_HINTS));

        HintNotificationPayload[] cached =
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                        OptimizationType.PERFORMANCE_HINTS);
        Assert.assertNotNull(cached);
        Assert.assertEquals(0, cached.length);
    }

    @Test
    @SmallTest
    public void testClearAllOnFeatureOff() {
        OptimizationGuidePushNotificationManager.setNativeIsInitializedForTesting(false);

        setFeatureStatusForTest(true);
        OptimizationGuidePushNotificationManager.onPushNotification(
                HintNotificationPayload.newBuilder(NOTIFICATION_WITH_PAYLOAD)
                        .setOptimizationType(OptimizationType.LITE_PAGE)
                        .build());
        OptimizationGuidePushNotificationManager.onPushNotification(
                HintNotificationPayload.newBuilder(NOTIFICATION_WITH_PAYLOAD)
                        .setOptimizationType(OptimizationType.LITE_VIDEO)
                        .build());

        Assert.assertEquals(1,
                OptimizationGuidePushNotificationManager
                        .getNotificationCacheForOptimizationType(OptimizationType.LITE_PAGE)
                        .length);
        Assert.assertEquals(1,
                OptimizationGuidePushNotificationManager
                        .getNotificationCacheForOptimizationType(OptimizationType.LITE_VIDEO)
                        .length);

        setFeatureStatusForTest(false);
        // Push another notification to trigger the clear.
        OptimizationGuidePushNotificationManager.onPushNotification(NOTIFICATION_WITH_PAYLOAD);

        Assert.assertEquals(0,
                OptimizationGuidePushNotificationManager
                        .getNotificationCacheForOptimizationType(OptimizationType.LITE_PAGE)
                        .length);
        Assert.assertEquals(0,
                OptimizationGuidePushNotificationManager
                        .getNotificationCacheForOptimizationType(OptimizationType.LITE_VIDEO)
                        .length);
    }

    @Test
    @SmallTest
    public void testOverflow() {
        OptimizationGuidePushNotificationManager.setNativeIsInitializedForTesting(false);
        setFeatureStatusForTest(true);

        final int overflowSize = 5;
        OptimizationGuidePushNotificationManager.MAX_CACHE_SIZE.setForTesting(overflowSize);

        for (int i = 1; i <= overflowSize; i++) {
            Assert.assertFalse(String.format("Iteration %d", i),
                    OptimizationGuidePushNotificationManager
                            .didNotificationCacheOverflowForOptimizationType(
                                    OptimizationType.PERFORMANCE_HINTS));
            OptimizationGuidePushNotificationManager.onPushNotification(
                    HintNotificationPayload.newBuilder(NOTIFICATION_WITH_PAYLOAD)
                            .setHintKey("hint " + i)
                            .build());
        }

        Assert.assertTrue(OptimizationGuidePushNotificationManager
                                  .didNotificationCacheOverflowForOptimizationType(
                                          OptimizationType.PERFORMANCE_HINTS));

        HintNotificationPayload[] cached =
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                        OptimizationType.PERFORMANCE_HINTS);
        Assert.assertNull(cached);

        OptimizationGuidePushNotificationManager.clearCacheForOptimizationType(
                OptimizationType.PERFORMANCE_HINTS);
        cached = OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                OptimizationType.PERFORMANCE_HINTS);
        Assert.assertNotNull(cached);
        Assert.assertEquals(0, cached.length);
    }

    @Test
    @SmallTest
    public void testIdenticalDeduplicated() {
        OptimizationGuidePushNotificationManager.setNativeIsInitializedForTesting(false);
        setFeatureStatusForTest(true);

        for (int i = 0; i < 10; i++) {
            OptimizationGuidePushNotificationManager.onPushNotification(NOTIFICATION_WITH_PAYLOAD);
        }

        Assert.assertFalse(OptimizationGuidePushNotificationManager
                                   .didNotificationCacheOverflowForOptimizationType(
                                           OptimizationType.PERFORMANCE_HINTS));

        HintNotificationPayload[] cached =
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                        OptimizationType.PERFORMANCE_HINTS);
        Assert.assertNotNull(cached);
        Assert.assertEquals(1, cached.length);
        Assert.assertEquals(NOTIFICATION_WITHOUT_PAYLOAD, cached[0]);
    }

    @Test
    @SmallTest
    public void testIncompleteNotPersisted() {
        OptimizationGuidePushNotificationManager.setNativeIsInitializedForTesting(false);
        setFeatureStatusForTest(true);

        // No optimization type.
        OptimizationGuidePushNotificationManager.onPushNotification(
                HintNotificationPayload.newBuilder(NOTIFICATION_WITH_PAYLOAD)
                        .clearOptimizationType()
                        .build());

        // No key representation.
        OptimizationGuidePushNotificationManager.onPushNotification(
                HintNotificationPayload.newBuilder(NOTIFICATION_WITH_PAYLOAD)
                        .clearKeyRepresentation()
                        .build());

        // No hint key.
        OptimizationGuidePushNotificationManager.onPushNotification(
                HintNotificationPayload.newBuilder(NOTIFICATION_WITH_PAYLOAD)
                        .clearHintKey()
                        .build());

        HintNotificationPayload[] cached =
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                        OptimizationType.PERFORMANCE_HINTS);
        Assert.assertNotNull(cached);
        Assert.assertEquals(0, cached.length);
    }

    @Test
    @SmallTest
    public void testPayloadOptional() {
        OptimizationGuidePushNotificationManager.setNativeIsInitializedForTesting(false);
        setFeatureStatusForTest(true);

        OptimizationGuidePushNotificationManager.onPushNotification(NOTIFICATION_WITHOUT_PAYLOAD);

        HintNotificationPayload[] cached =
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                        OptimizationType.PERFORMANCE_HINTS);
        Assert.assertNotNull(cached);
        Assert.assertEquals(1, cached.length);
        Assert.assertEquals(NOTIFICATION_WITHOUT_PAYLOAD, cached[0]);
    }
}
