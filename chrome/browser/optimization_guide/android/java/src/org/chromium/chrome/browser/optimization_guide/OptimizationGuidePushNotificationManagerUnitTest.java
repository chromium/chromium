// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.optimization_guide;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.util.Base64;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import com.google.protobuf.ByteString;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.optimization_guide.proto.CommonTypesProto.Any;
import org.chromium.components.optimization_guide.proto.HintsProto.KeyRepresentation;
import org.chromium.components.optimization_guide.proto.HintsProto.OptimizationType;
import org.chromium.components.optimization_guide.proto.PushNotificationProto.HintNotificationPayload;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;

/** Unit tests for OptimizationGuidePushNotificationManager. */
@RunWith(BaseJUnit4ClassRunner.class)
// Batch this per class since the test is setting global feature state.
@Batch(Batch.PER_CLASS)
@EnableFeatures(ChromeFeatureList.OPTIMIZATION_GUIDE_PUSH_NOTIFICATIONS)
public class OptimizationGuidePushNotificationManagerUnitTest {
    @Rule public JniMocker mocker = new JniMocker();
    @Rule public MockitoRule mMockitoJUnit = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private OptimizationGuideBridgeFactory.Natives mOptimizationGuideBridgeFactoryJniMock;
    @Mock private OptimizationGuideBridge mOptimizationGuideBridge;

    private static final HintNotificationPayload NOTIFICATION_WITH_PAYLOAD =
            HintNotificationPayload.newBuilder()
                    .setOptimizationType(OptimizationType.PERFORMANCE_HINTS)
                    .setKeyRepresentation(KeyRepresentation.FULL_URL)
                    .setHintKey("Testing")
                    .setPayload(
                            Any.newBuilder()
                                    .setTypeUrl("com.testing")
                                    .setValue(ByteString.copyFrom(new byte[] {0, 1, 2, 3, 4}))
                                    .build())
                    .build();

    private static final HintNotificationPayload NOTIFICATION_WITHOUT_PAYLOAD =
            HintNotificationPayload.newBuilder(NOTIFICATION_WITH_PAYLOAD).clearPayload().build();

    @Before
    public void setUp() {
        mocker.mock(
                org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeFactoryJni
                        .TEST_HOOKS,
                mOptimizationGuideBridgeFactoryJniMock);
        doReturn(mOptimizationGuideBridge)
                .when(mOptimizationGuideBridgeFactoryJniMock)
                .getForProfile(mProfile);

        ProfileManager.setLastUsedProfileForTesting(mProfile);

        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @Test
    @SmallTest
    public void testBasicSuccessCaseNoNative() {
        OptimizationGuidePushNotificationManager.setNativeIsInitializedForTesting(false);

        OptimizationGuidePushNotificationManager.onPushNotification(NOTIFICATION_WITH_PAYLOAD);
        Assert.assertEquals(
                new ArrayList<OptimizationType>(),
                OptimizationGuidePushNotificationManager
                        .getOptTypesThatOverflowedPushNotifications());

        HintNotificationPayload[] cached =
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                        OptimizationType.PERFORMANCE_HINTS);
        Assert.assertNotNull(cached);
        Assert.assertEquals(1, cached.length);
        Assert.assertEquals(NOTIFICATION_WITHOUT_PAYLOAD, cached[0]);

        // There should not be notifications for other types.
        Assert.assertEquals(
                0,
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                                OptimizationType.LITE_PAGE)
                        .length);

        Assert.assertEquals(
                Arrays.asList(OptimizationType.PERFORMANCE_HINTS),
                OptimizationGuidePushNotificationManager.getOptTypesWithPushNotifications());

        OptimizationGuidePushNotificationManager.clearCacheForOptimizationType(
                OptimizationType.PERFORMANCE_HINTS);
        cached =
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                        OptimizationType.PERFORMANCE_HINTS);
        Assert.assertNotNull(cached);
        Assert.assertEquals(0, cached.length);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testNativeCalled() {
        OptimizationGuidePushNotificationManager.setNativeIsInitializedForTesting(true);

        OptimizationGuidePushNotificationManager.onPushNotification(NOTIFICATION_WITHOUT_PAYLOAD);

        HintNotificationPayload[] cached =
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                        OptimizationType.PERFORMANCE_HINTS);
        Assert.assertNotNull(cached);
        Assert.assertEquals(0, cached.length);

        Assert.assertEquals(
                new ArrayList<OptimizationType>(),
                OptimizationGuidePushNotificationManager.getOptTypesWithPushNotifications());

        verify(mOptimizationGuideBridge, times(1))
                .onNewPushNotification(eq(NOTIFICATION_WITHOUT_PAYLOAD));
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.OPTIMIZATION_GUIDE_PUSH_NOTIFICATIONS)
    public void testFeatureDisabled() {
        OptimizationGuidePushNotificationManager.setNativeIsInitializedForTesting(false);

        OptimizationGuidePushNotificationManager.onPushNotification(NOTIFICATION_WITH_PAYLOAD);
        Assert.assertEquals(
                new ArrayList<OptimizationType>(),
                OptimizationGuidePushNotificationManager
                        .getOptTypesThatOverflowedPushNotifications());

        HintNotificationPayload[] cached =
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                        OptimizationType.PERFORMANCE_HINTS);
        Assert.assertNotNull(cached);
        Assert.assertEquals(0, cached.length);

        Assert.assertEquals(
                new ArrayList<OptimizationType>(),
                OptimizationGuidePushNotificationManager.getOptTypesWithPushNotifications());
    }

    @Test
    @SmallTest
    public void testClearAllOnFeatureOff() {
        OptimizationGuidePushNotificationManager.setNativeIsInitializedForTesting(false);

        OptimizationGuidePushNotificationManager.onPushNotification(
                HintNotificationPayload.newBuilder(NOTIFICATION_WITH_PAYLOAD)
                        .setOptimizationType(OptimizationType.LITE_PAGE)
                        .build());
        OptimizationGuidePushNotificationManager.onPushNotification(
                HintNotificationPayload.newBuilder(NOTIFICATION_WITH_PAYLOAD)
                        .setOptimizationType(OptimizationType.LITE_VIDEO)
                        .build());

        Assert.assertEquals(
                1,
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                                OptimizationType.LITE_PAGE)
                        .length);
        Assert.assertEquals(
                1,
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                                OptimizationType.LITE_VIDEO)
                        .length);

        Assert.assertEquals(
                Arrays.asList(OptimizationType.LITE_PAGE, OptimizationType.LITE_VIDEO),
                OptimizationGuidePushNotificationManager.getOptTypesWithPushNotifications());

        // Flag state cannot change within the same process instance, so this  behavior does not
        // actually get triggered in real usage.
        ChromeFeatureList.sOptimizationGuidePushNotifications.setForTesting(false);

        // Push another notification to trigger the clear.
        OptimizationGuidePushNotificationManager.onPushNotification(NOTIFICATION_WITH_PAYLOAD);

        Assert.assertEquals(
                0,
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                                OptimizationType.LITE_PAGE)
                        .length);
        Assert.assertEquals(
                0,
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                                OptimizationType.LITE_VIDEO)
                        .length);

        Assert.assertEquals(
                new ArrayList<OptimizationType>(),
                OptimizationGuidePushNotificationManager.getOptTypesWithPushNotifications());
    }

    @Test
    @SmallTest
    public void testOverflow() {
        OptimizationGuidePushNotificationManager.setNativeIsInitializedForTesting(false);

        final int overflowSize = 5;
        OptimizationGuidePushNotificationManager.MAX_CACHE_SIZE.setForTesting(overflowSize);

        for (int i = 1; i <= overflowSize; i++) {
            Assert.assertEquals(
                    String.format("Iteration %d", i),
                    new ArrayList<OptimizationType>(),
                    OptimizationGuidePushNotificationManager
                            .getOptTypesThatOverflowedPushNotifications());
            OptimizationGuidePushNotificationManager.onPushNotification(
                    HintNotificationPayload.newBuilder(NOTIFICATION_WITH_PAYLOAD)
                            .setHintKey("hint " + i)
                            .build());
        }

        Assert.assertEquals(
                Arrays.asList(OptimizationType.PERFORMANCE_HINTS),
                OptimizationGuidePushNotificationManager
                        .getOptTypesThatOverflowedPushNotifications());

        HintNotificationPayload[] cached =
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                        OptimizationType.PERFORMANCE_HINTS);
        Assert.assertNull(cached);

        Assert.assertEquals(
                new ArrayList<OptimizationType>(),
                OptimizationGuidePushNotificationManager.getOptTypesWithPushNotifications());

        OptimizationGuidePushNotificationManager.clearCacheForOptimizationType(
                OptimizationType.PERFORMANCE_HINTS);
        cached =
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                        OptimizationType.PERFORMANCE_HINTS);
        Assert.assertNotNull(cached);
        Assert.assertEquals(0, cached.length);

        Assert.assertEquals(
                new ArrayList<OptimizationType>(),
                OptimizationGuidePushNotificationManager.getOptTypesWithPushNotifications());
    }

    @Test
    @SmallTest
    public void testIdenticalDeduplicated() {
        OptimizationGuidePushNotificationManager.setNativeIsInitializedForTesting(false);

        for (int i = 0; i < 10; i++) {
            OptimizationGuidePushNotificationManager.onPushNotification(NOTIFICATION_WITH_PAYLOAD);
        }

        Assert.assertEquals(
                new ArrayList<OptimizationType>(),
                OptimizationGuidePushNotificationManager
                        .getOptTypesThatOverflowedPushNotifications());

        HintNotificationPayload[] cached =
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                        OptimizationType.PERFORMANCE_HINTS);
        Assert.assertNotNull(cached);
        Assert.assertEquals(1, cached.length);
        Assert.assertEquals(NOTIFICATION_WITHOUT_PAYLOAD, cached[0]);

        Assert.assertEquals(
                Arrays.asList(OptimizationType.PERFORMANCE_HINTS),
                OptimizationGuidePushNotificationManager.getOptTypesWithPushNotifications());
    }

    @Test
    @SmallTest
    public void testIncompleteNotPersisted() {
        OptimizationGuidePushNotificationManager.setNativeIsInitializedForTesting(false);

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

        Assert.assertEquals(
                new ArrayList<OptimizationType>(),
                OptimizationGuidePushNotificationManager.getOptTypesWithPushNotifications());
    }

    @Test
    @SmallTest
    public void testPayloadOptional() {
        OptimizationGuidePushNotificationManager.setNativeIsInitializedForTesting(false);

        OptimizationGuidePushNotificationManager.onPushNotification(NOTIFICATION_WITHOUT_PAYLOAD);

        HintNotificationPayload[] cached =
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                        OptimizationType.PERFORMANCE_HINTS);
        Assert.assertNotNull(cached);
        Assert.assertEquals(1, cached.length);
        Assert.assertEquals(NOTIFICATION_WITHOUT_PAYLOAD, cached[0]);

        Assert.assertEquals(
                Arrays.asList(OptimizationType.PERFORMANCE_HINTS),
                OptimizationGuidePushNotificationManager.getOptTypesWithPushNotifications());
    }

    @Test
    @SmallTest
    public void testCacheDecodingErrors_Success() {
        OptimizationGuidePushNotificationManager.setNativeIsInitializedForTesting(false);

        int startSuccessErrorCount =
                RecordHistogram.getHistogramValueCountForTesting(
                        "OptimizationGuide.PushNotifications.ReadCacheResult", /* SUCCESS= */ 1);
        int startTotalCount =
                RecordHistogram.getHistogramTotalCountForTesting(
                        "OptimizationGuide.PushNotifications.ReadCacheResult");

        OptimizationGuidePushNotificationManager.onPushNotification(NOTIFICATION_WITHOUT_PAYLOAD);

        HintNotificationPayload[] cached =
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                        OptimizationType.PERFORMANCE_HINTS);
        Assert.assertNotNull(cached);
        Assert.assertEquals(1, cached.length);
        Assert.assertEquals(NOTIFICATION_WITHOUT_PAYLOAD, cached[0]);

        int afterSuccessErrorCount =
                RecordHistogram.getHistogramValueCountForTesting(
                        "OptimizationGuide.PushNotifications.ReadCacheResult", /* SUCCESS= */ 1);
        int afterTotalCount =
                RecordHistogram.getHistogramTotalCountForTesting(
                        "OptimizationGuide.PushNotifications.ReadCacheResult");

        Assert.assertEquals(1, afterSuccessErrorCount - startSuccessErrorCount);
        Assert.assertEquals(1, afterTotalCount - startTotalCount);
    }

    @Test
    @SmallTest
    public void testCacheDecodingErrors_InvalidProtobuf() {
        OptimizationGuidePushNotificationManager.setNativeIsInitializedForTesting(false);

        int startPbErrorCount =
                RecordHistogram.getHistogramValueCountForTesting(
                        "OptimizationGuide.PushNotifications.ReadCacheResult",
                        /* INVALID_PROTOBUF= */ 2);
        int startTotalCount =
                RecordHistogram.getHistogramTotalCountForTesting(
                        "OptimizationGuide.PushNotifications.ReadCacheResult");

        ChromeSharedPreferences.getInstance()
                .writeStringSet(
                        OptimizationGuidePushNotificationManager.cacheKey(
                                OptimizationType.PERFORMANCE_HINTS),
                        new HashSet<String>(
                                Arrays.asList(
                                        Base64.encodeToString(
                                                new byte[] {1, 2, 3}, Base64.DEFAULT))));

        HintNotificationPayload[] cached =
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                        OptimizationType.PERFORMANCE_HINTS);
        Assert.assertNotNull(cached);
        Assert.assertEquals(0, cached.length);

        int afterPbErrorCount =
                RecordHistogram.getHistogramValueCountForTesting(
                        "OptimizationGuide.PushNotifications.ReadCacheResult",
                        /* INVALID_PROTOBUF= */ 2);
        int afterTotalCount =
                RecordHistogram.getHistogramTotalCountForTesting(
                        "OptimizationGuide.PushNotifications.ReadCacheResult");

        Assert.assertEquals(1, afterPbErrorCount - startPbErrorCount);
        Assert.assertEquals(1, afterTotalCount - startTotalCount);
    }

    @Test
    @SmallTest
    public void testCacheDecodingErrors_Base64Error() {
        OptimizationGuidePushNotificationManager.setNativeIsInitializedForTesting(false);

        int startB64ErrorCount =
                RecordHistogram.getHistogramValueCountForTesting(
                        "OptimizationGuide.PushNotifications.ReadCacheResult",
                        /* BASE64_ERROR= */ 3);
        int startTotalCount =
                RecordHistogram.getHistogramTotalCountForTesting(
                        "OptimizationGuide.PushNotifications.ReadCacheResult");

        ChromeSharedPreferences.getInstance()
                .writeStringSet(
                        OptimizationGuidePushNotificationManager.cacheKey(
                                OptimizationType.PERFORMANCE_HINTS),
                        new HashSet<String>(Arrays.asList("=")));

        HintNotificationPayload[] cached =
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                        OptimizationType.PERFORMANCE_HINTS);
        Assert.assertNotNull(cached);
        Assert.assertEquals(0, cached.length);

        int afterB64ErrorCount =
                RecordHistogram.getHistogramValueCountForTesting(
                        "OptimizationGuide.PushNotifications.ReadCacheResult",
                        /* BASE64_ERROR= */ 3);
        int afterTotalCount =
                RecordHistogram.getHistogramTotalCountForTesting(
                        "OptimizationGuide.PushNotifications.ReadCacheResult");

        Assert.assertEquals(1, afterB64ErrorCount - startB64ErrorCount);
        Assert.assertEquals(1, afterTotalCount - startTotalCount);
    }
}
