// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_import;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.Intent;
import android.os.ParcelFileDescriptor;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;
import androidx.test.rule.ServiceTestRule;

import com.google.protobuf.ByteString;

import io.grpc.ManagedChannel;
import io.grpc.Metadata;

import io.grpc.binder.BinderChannelBuilder;
import io.grpc.stub.MetadataUtils;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.io.File;
import java.io.FileWriter;
import java.util.concurrent.TimeoutException;

/** End-to-end tests for {@link DataImporterServiceImpl}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@Features.EnableFeatures(ChromeFeatureList.ANDROID_DATA_IMPORTER_SERVICE)
public class DataImporterServiceImplTest {

    @Rule public final ServiceTestRule mServiceTestRule = new ServiceTestRule();

    private ManagedChannel mChannel;
    private TargetServiceGrpc.TargetServiceBlockingStub mStub;

    @Before
    public void setUp() throws TimeoutException {
        DataImporterServiceImpl.setSkipSecurityPolicyForTesting(true);
        Context appContext = ApplicationProvider.getApplicationContext();
        Intent intent = new Intent(appContext, DataImporterService.class);
        // ServiceTestRule will start the service and keep it running.
        mServiceTestRule.bindService(intent);
        io.grpc.binder.AndroidComponentAddress address =
                io.grpc.binder.AndroidComponentAddress.forComponent(
                        new android.content.ComponentName(appContext, DataImporterService.class));
        mChannel = BinderChannelBuilder.forAddress(address, appContext).build();
        mStub = TargetServiceGrpc.newBlockingStub(mChannel);
    }

    @After
    public void tearDown() {
        mChannel.shutdownNow();
    }

    @Test
    @SmallTest
    public void testImport() throws Exception {
        final String sessionId = "test_import_session";
        // 1. Handshake.
        TargetHandshakeRequest handshakeRequest =
                TargetHandshakeRequest.newBuilder()
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA)
                        .setSessionId(ByteString.copyFromUtf8(sessionId))
                        .build();
        TargetHandshakeResponse response = mStub.handshake(handshakeRequest);
        assertTrue(response.getSupported());
        assertEquals(1, response.getDataFormatVersion());

        // 2. Import bookmarks.
        File tempFile = File.createTempFile("bookmarks", ".html");
        try (FileWriter writer = new FileWriter(tempFile)) {
            writer.write(
                    "<html><body><h1>Bookmarks</h1><a"
                            + " href=\"http://google.com\">Google</a></body></html>");
        }
        ParcelFileDescriptor pfd =
                ParcelFileDescriptor.open(tempFile, ParcelFileDescriptor.MODE_READ_ONLY);

        BrowserFileMetadata metadata =
                BrowserFileMetadata.newBuilder()
                        .setFileType(BrowserFileType.BROWSER_FILE_TYPE_BOOKMARKS)
                        .build();
        ImportItemRequest importRequest =
                ImportItemRequest.newBuilder()
                        .setSessionId(ByteString.copyFromUtf8(sessionId))
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA)
                        .setFileMetadata(Proto3Any.newBuilder().setValue(metadata.toByteString()))
                        .build();

        Metadata headers = new Metadata();
        headers.put(DataImporterServiceImpl.ParcelableMetadataInterceptor.PFD_METADATA_KEY, pfd);
        mStub.withInterceptors(MetadataUtils.newAttachHeadersInterceptor(headers))
                .importItem(importRequest);

        // 3. Import done.
        ImportItemsDoneRequest doneRequest =
                ImportItemsDoneRequest.newBuilder()
                        .setSessionId(ByteString.copyFromUtf8(sessionId))
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA)
                        .build();
        ImportItemsDoneResponse doneResponse = mStub.importItemsDone(doneRequest);
        assertEquals(1, doneResponse.getSuccessItemCount());
    }
}
