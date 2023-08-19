// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.optimization_guide;

import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.util.Base64;

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
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.optimization_guide.proto.CommonTypesProto.Any;
import org.chromium.components.optimization_guide.proto.HintsProto.KeyRepresentation;
import org.chromium.components.optimization_guide.proto.HintsProto.OptimizationType;
import org.chromium.components.optimization_guide.proto.PushNotificationProto.HintNotificationPayload;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
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

        ChromeFeatureList.sOptimizationGuidePushNotifications.setForTesting(isEnabled);
    }

    @Before
    public void setUp() {
        resetFeatureFlags();

        MockitoAnnotations.initMocks(this);
        mocker.mock(OptimizationGuideBridgeJni.TEST_HOOKS, mOptimizationGuideBridgeJniMock);
        when(mOptimizationGuideBridgeJniMock.init()).thenReturn(1L);

        Profile.setLastUsedProfileForTesting(mProfile);

        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @After
    public void tearDown() {
        resetFeatureFlags();
    }

    public void resetFeatureFlags() {
        CachedFeatureFlags.resetFlagsForTesting();
        OptimizationGuidePushNotificationManager.clearCacheForAllTypes();
        FeatureList.setTestFeatures(null);
    }

    @Test
    @SmallTest
    public void testBasicSuccessCaseNoNative() {
        OptimizationGuidePushNotificationManager.setNativeIsInitializedForTesting(false);
        setFeatureStatusForTest(true);

        OptimizationGuidePushNotificationManager.onPushNotification(NOTIFICATION_WITH_PAYLOAD);
        Assert.assertEquals(new ArrayList<OptimizationType>(),
                OptimizationGuidePushNotificationManager
                        .getOptTypesThatOverflowedPushNotifications());

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

        Assert.assertEquals(Arrays.asList(OptimizationType.PERFORMANCE_HINTS),
                OptimizationGuidePushNotificationManager.getOptTypesWithPushNotifications());

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

        Assert.assertEquals(new ArrayList<OptimizationType>(),
                OptimizationGuidePushNotificationManager.getOptTypesWithPushNotifications());

        verify(mOptimizationGuideBridgeJniMock, times(1))
                .onNewPushNotification(anyLong(), eq(NOTIFICATION_WITHOUT_PAYLOAD.toByteArray()));
    }

    @Test
    @SmallTest
    public void testFeatureDisabled() {
        setFeatureStatusForTest(false);
        OptimizationGuidePushNotificationManager.setNativeIsInitializedForTesting(false);

        OptimizationGuidePushNotificationManager.onPushNotification(NOTIFICATION_WITH_PAYLOAD);
        Assert.assertEquals(new ArrayList<OptimizationType>(),
                OptimizationGuidePushNotificationManager
                        .getOptTypesThatOverflowedPushNotifications());

        HintNotificationPayload[] cached =
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                        OptimizationType.PERFORMANCE_HINTS);
        Assert.assertNotNull(cached);
        Assert.assertEquals(0, cached.length);

        Assert.assertEquals(new ArrayList<OptimizationType>(),
                OptimizationGuidePushNotificationManager.getOptTypesWithPushNotifications());
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

        Assert.assertEquals(Arrays.asList(OptimizationType.LITE_PAGE, OptimizationType.LITE_VIDEO),
                OptimizationGuidePushNotificationManager.getOptTypesWithPushNotifications());

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

        Assert.assertEquals(new ArrayList<OptimizationType>(),
                OptimizationGuidePushNotificationManager.getOptTypesWithPushNotifications());
    }

    @Test
    @SmallTest
    public void testOverflow() {
        OptimizationGuidePushNotificationManager.setNativeIsInitializedForTesting(false);
        setFeatureStatusForTest(true);

        final int overflowSize = 5;
        OptimizationGuidePushNotificationManager.MAX_CACHE_SIZE.setForTesting(overflowSize);

        for (int i = 1; i <= overflowSize; i++) {
            Assert.assertEquals(String.format("Iteration %d", i), new ArrayList<OptimizationType>(),
                    OptimizationGuidePushNotificationManager
                            .getOptTypesThatOverflowedPushNotifications());
            OptimizationGuidePushNotificationManager.onPushNotification(
                    HintNotificationPayload.newBuilder(NOTIFICATION_WITH_PAYLOAD)
                            .setHintKey("hint " + i)
                            .build());
        }

        Assert.assertEquals(Arrays.asList(OptimizationType.PERFORMANCE_HINTS),
                OptimizationGuidePushNotificationManager
                        .getOptTypesThatOverflowedPushNotifications());

        HintNotificationPayload[] cached =
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                        OptimizationType.PERFORMANCE_HINTS);
        Assert.assertNull(cached);

        Assert.assertEquals(new ArrayList<OptimizationType>(),
                OptimizationGuidePushNotificationManager.getOptTypesWithPushNotifications());

        OptimizationGuidePushNotificationManager.clearCacheForOptimizationType(
                OptimizationType.PERFORMANCE_HINTS);
        cached = OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                OptimizationType.PERFORMANCE_HINTS);
        Assert.assertNotNull(cached);
        Assert.assertEquals(0, cached.length);

        Assert.assertEquals(new ArrayList<OptimizationType>(),
                OptimizationGuidePushNotificationManager.getOptTypesWithPushNotifications());
    }

    @Test
    @SmallTest
    public void testIdenticalDeduplicated() {
        OptimizationGuidePushNotificationManager.setNativeIsInitializedForTesting(false);
        setFeatureStatusForTest(true);

        for (int i = 0; i < 10; i++) {
            OptimizationGuidePushNotificationManager.onPushNotification(NOTIFICATION_WITH_PAYLOAD);
        }

        Assert.assertEquals(new ArrayList<OptimizationType>(),
                OptimizationGuidePushNotificationManager
                        .getOptTypesThatOverflowedPushNotifications());

        HintNotificationPayload[] cached =
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                        OptimizationType.PERFORMANCE_HINTS);
        Assert.assertNotNull(cached);
        Assert.assertEquals(1, cached.length);
        Assert.assertEquals(NOTIFICATION_WITHOUT_PAYLOAD, cached[0]);

        Assert.assertEquals(Arrays.asList(OptimizationType.PERFORMANCE_HINTS),
                OptimizationGuidePushNotificationManager.getOptTypesWithPushNotifications());
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

        Assert.assertEquals(new ArrayList<OptimizationType>(),
                OptimizationGuidePushNotificationManager.getOptTypesWithPushNotifications());
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

        Assert.assertEquals(Arrays.asList(OptimizationType.PERFORMANCE_HINTS),
                OptimizationGuidePushNotificationManager.getOptTypesWithPushNotifications());
    }

    @Test
    @SmallTest
    public void testCacheDecodingErrors_Success() {
        OptimizationGuidePushNotificationManager.setNativeIsInitializedForTesting(false);
        setFeatureStatusForTest(true);

        int startSuccessErrorCount = RecordHistogram.getHistogramValueCountForTesting(
                "OptimizationGuide.PushNotifications.ReadCacheResult", /*SUCCESS=*/1);
        int startTotalCount = RecordHistogram.getHistogramTotalCountForTesting(
                "OptimizationGuide.PushNotifications.ReadCacheResult");

        OptimizationGuidePushNotificationManager.onPushNotification(NOTIFICATION_WITHOUT_PAYLOAD);

        HintNotificationPayload[] cached =
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                        OptimizationType.PERFORMANCE_HINTS);
        Assert.assertNotNull(cached);
        Assert.assertEquals(1, cached.length);
        Assert.assertEquals(NOTIFICATION_WITHOUT_PAYLOAD, cached[0]);

        int afterSuccessErrorCount = RecordHistogram.getHistogramValueCountForTesting(
                "OptimizationGuide.PushNotifications.ReadCacheResult", /*SUCCESS=*/1);
        int afterTotalCount = RecordHistogram.getHistogramTotalCountForTesting(
                "OptimizationGuide.PushNotifications.ReadCacheResult");

        Assert.assertEquals(1, afterSuccessErrorCount - startSuccessErrorCount);
        Assert.assertEquals(1, afterTotalCount - startTotalCount);
    }

    @Test
    @SmallTest
    public void testCacheDecodingErrors_InvalidProtobuf() {
        OptimizationGuidePushNotificationManager.setNativeIsInitializedForTesting(false);
        setFeatureStatusForTest(true);

        int startPBErrorCount = RecordHistogram.getHistogramValueCountForTesting(
                "OptimizationGuide.PushNotifications.ReadCacheResult", /*INVALID_PROTOBUF=*/2);
        int startTotalCount = RecordHistogram.getHistogramTotalCountForTesting(
                "OptimizationGuide.PushNotifications.ReadCacheResult");

        SharedPreferencesManager.getInstance().writeStringSet(
                OptimizationGuidePushNotificationManager.cacheKey(
                        OptimizationType.PERFORMANCE_HINTS),
                new HashSet<String>(Arrays.asList(
                        Base64.encodeToString(new byte[] {1, 2, 3}, Base64.DEFAULT))));

        HintNotificationPayload[] cached =
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                        OptimizationType.PERFORMANCE_HINTS);
        Assert.assertNotNull(cached);
        Assert.assertEquals(0, cached.length);

        int afterPBErrorCount = RecordHistogram.getHistogramValueCountForTesting(
                "OptimizationGuide.PushNotifications.ReadCacheResult", /*INVALID_PROTOBUF=*/2);
        int afterTotalCount = RecordHistogram.getHistogramTotalCountForTesting(
                "OptimizationGuide.PushNotifications.ReadCacheResult");

        Assert.assertEquals(1, afterPBErrorCount - startPBErrorCount);
        Assert.assertEquals(1, afterTotalCount - startTotalCount);
    }

    @Test
    @SmallTest
    public void testCacheDecodingErrors_Base64Error() {
        OptimizationGuidePushNotificationManager.setNativeIsInitializedForTesting(false);
        setFeatureStatusForTest(true);

        int startB64ErrorCount = RecordHistogram.getHistogramValueCountForTesting(
                "OptimizationGuide.PushNotifications.ReadCacheResult", /*BASE64_ERROR=*/3);
        int startTotalCount = RecordHistogram.getHistogramTotalCountForTesting(
                "OptimizationGuide.PushNotifications.ReadCacheResult");

        SharedPreferencesManager.getInstance().writeStringSet(
                OptimizationGuidePushNotificationManager.cacheKey(
                        OptimizationType.PERFORMANCE_HINTS),
                new HashSet<String>(Arrays.asList("=")));

        HintNotificationPayload[] cached =
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                        OptimizationType.PERFORMANCE_HINTS);
        Assert.assertNotNull(cached);
        Assert.assertEquals(0, cached.length);

        int afterB64ErrorCount = RecordHistogram.getHistogramValueCountForTesting(
                "OptimizationGuide.PushNotifications.ReadCacheResult", /*BASE64_ERROR=*/3);
        int afterTotalCount = RecordHistogram.getHistogramTotalCountForTesting(
                "OptimizationGuide.PushNotifications.ReadCacheResult");

        Assert.assertEquals(1, afterB64ErrorCount - startB64ErrorCount);
        Assert.assertEquals(1, afterTotalCount - startTotalCount);
    }
}
