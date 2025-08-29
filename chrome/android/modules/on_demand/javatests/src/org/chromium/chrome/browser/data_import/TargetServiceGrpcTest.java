// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_import;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import com.google.protobuf.ByteString;

import io.grpc.ManagedChannel;
import io.grpc.ServerInterceptors;
import io.grpc.inprocess.InProcessChannelBuilder;
import io.grpc.inprocess.InProcessServerBuilder;
import io.grpc.testing.GrpcCleanupRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.io.IOException;

/**
 * Tests for {@link DataImporterServiceImpl}. These tests use an in-process gRPC server to test the
 * service implementation, including interceptors.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
// TODO(crbug.com/436171659): Also add a simple test with the feature disabled, which will just
// verify that onCreate and onBind don't do anything
@Features.EnableFeatures(ChromeFeatureList.ANDROID_DATA_IMPORTER_SERVICE)
public class TargetServiceGrpcTest {

    @Rule public final GrpcCleanupRule mGrpcCleanup = new GrpcCleanupRule();

    @Mock private DataImporterBridge mBridge;

    private TargetServiceGrpc.TargetServiceBlockingStub mStub;
    private TargetService mTargetService;
    private static final ByteString SESSION_ID = ByteString.copyFromUtf8("test_session_id");

    @Before
    public void setUp() throws IOException {
        MockitoAnnotations.initMocks(this);
        mTargetService = new TargetService();
        mTargetService.mBridge = mBridge;

        String serverName = InProcessServerBuilder.generateName();
        mGrpcCleanup.register(
                InProcessServerBuilder.forName(serverName)
                        .directExecutor()
                        .addService(
                                ServerInterceptors.intercept(
                                        mTargetService,
                                        new DataImporterServiceImpl
                                                .ParcelableMetadataInterceptor()))
                        .build()
                        .start());

        ManagedChannel channel =
                mGrpcCleanup.register(
                        InProcessChannelBuilder.forName(serverName).directExecutor().build());

        mStub = TargetServiceGrpc.newBlockingStub(channel);
    }

    @Test
    @SmallTest
    public void testHandshake() {
        TargetHandshakeRequest request =
                TargetHandshakeRequest.newBuilder()
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA)
                        .setSessionId(SESSION_ID)
                        .build();
        TargetHandshakeResponse response = mStub.handshake(request);
        assertTrue(response.getSupported());
        assertEquals(1, response.getDataFormatVersion());
    }

    @Test
    @SmallTest
    public void testHandshake_unsupportedType() {
        TargetHandshakeRequest request =
                TargetHandshakeRequest.newBuilder()
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_UNSPECIFIED)
                        .setSessionId(SESSION_ID)
                        .build();
        TargetHandshakeResponse response = mStub.handshake(request);
        assertFalse(response.getSupported());
    }
}
