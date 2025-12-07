// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_import;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.verify;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.os.ParcelFileDescriptor;

import androidx.test.filters.SmallTest;

import com.google.protobuf.ByteString;
import com.google.protobuf.InvalidProtocolBufferException;

import io.grpc.Context;
import io.grpc.Status;
import io.grpc.StatusRuntimeException;
import io.grpc.stub.StreamObserver;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/** Tests for {@link TargetService}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@Features.EnableFeatures(ChromeFeatureList.ANDROID_DATA_IMPORTER_SERVICE)
public class TargetServiceTest {

    private TargetService mService;

    @Mock private StreamObserver<TargetHandshakeResponse> mHandshakeResponseObserver;
    @Mock private StreamObserver<ImportItemResponse> mImportItemResponseObserver;
    @Mock private StreamObserver<ImportItemsDoneResponse> mImportItemsDoneResponseObserver;
    @Mock private DataImporterBridge mBridge;
    @Mock private ParcelFileDescriptor mMockPfd;

    @Captor private ArgumentCaptor<TargetHandshakeResponse> mHandshakeResponseCaptor;
    @Captor private ArgumentCaptor<ImportItemsDoneResponse> mImportItemsDoneResponseCaptor;
    @Captor private ArgumentCaptor<StatusRuntimeException> mErrorCaptor;
    @Captor private ArgumentCaptor<Callback<Integer>> mImportResultCallback;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mService = new TargetService();
        mService.mBridge = mBridge;
    }

    @Test
    @SmallTest
    public void testHandshake() {
        TargetHandshakeRequest request =
                TargetHandshakeRequest.newBuilder()
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA)
                        .setSessionId(ByteString.copyFromUtf8("test_session_id"))
                        .build();
        mService.handshake(request, mHandshakeResponseObserver);

        verify(mHandshakeResponseObserver).onNext(mHandshakeResponseCaptor.capture());
        verify(mHandshakeResponseObserver).onCompleted();
        TargetHandshakeResponse response = mHandshakeResponseCaptor.getValue();
        assertTrue(response.getSupported());
        assertEquals(1, response.getDataFormatVersion());
    }

    @Test
    @SmallTest
    public void testHandshake_unsupportedType() {
        TargetHandshakeRequest request =
                TargetHandshakeRequest.newBuilder()
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_UNSPECIFIED)
                        .setSessionId(ByteString.copyFromUtf8("test_session_id"))
                        .build();
        mService.handshake(request, mHandshakeResponseObserver);

        verify(mHandshakeResponseObserver).onNext(mHandshakeResponseCaptor.capture());
        verify(mHandshakeResponseObserver).onCompleted();
        TargetHandshakeResponse response = mHandshakeResponseCaptor.getValue();
        assertFalse(response.getSupported());
    }

    @Test
    @SmallTest
    public void testHandshake_missingSessionId() {
        TargetHandshakeRequest request =
                TargetHandshakeRequest.newBuilder()
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA)
                        .build();
        mService.handshake(request, mHandshakeResponseObserver);
        verify(mHandshakeResponseObserver).onError(mErrorCaptor.capture());
        StatusRuntimeException exception = mErrorCaptor.getValue();
        assertEquals(Status.INVALID_ARGUMENT.getCode(), exception.getStatus().getCode());
        assertEquals("Missing session_id", exception.getStatus().getDescription());
    }

    @Test
    @SmallTest
    public void testImportItem_unsupportedItemType() {
        ImportItemRequest request =
                ImportItemRequest.newBuilder()
                        .setSessionId(ByteString.copyFromUtf8("test_session_id"))
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_UNSPECIFIED)
                        .build();
        Context.current()
                .withValue(DataImporterServiceImpl.PFD_CONTEXT_KEY, mMockPfd)
                .run(() -> mService.importItem(request, mImportItemResponseObserver));
        verify(mImportItemResponseObserver).onError(mErrorCaptor.capture());
        assertEquals(
                Status.INVALID_ARGUMENT.getCode(), mErrorCaptor.getValue().getStatus().getCode());
        assertEquals(
                "Invalid or unsupported item type",
                mErrorCaptor.getValue().getStatus().getDescription());
    }

    @Test
    @SmallTest
    public void testImportItem_missingSessionId() {
        ImportItemRequest request =
                ImportItemRequest.newBuilder()
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA)
                        .build();
        Context.current()
                .withValue(DataImporterServiceImpl.PFD_CONTEXT_KEY, mMockPfd)
                .run(() -> mService.importItem(request, mImportItemResponseObserver));
        verify(mImportItemResponseObserver).onError(mErrorCaptor.capture());
        assertEquals(
                Status.INVALID_ARGUMENT.getCode(), mErrorCaptor.getValue().getStatus().getCode());
        assertEquals("Missing session_id", mErrorCaptor.getValue().getStatus().getDescription());
    }

    @Test
    @SmallTest
    public void testImportItem_missingPfd() {
        ImportItemRequest request =
                ImportItemRequest.newBuilder()
                        .setSessionId(ByteString.copyFromUtf8("test_session_id"))
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA)
                        .build();
        mService.importItem(request, mImportItemResponseObserver);
        verify(mImportItemResponseObserver).onError(mErrorCaptor.capture());
        assertEquals(
                Status.INVALID_ARGUMENT.getCode(), mErrorCaptor.getValue().getStatus().getCode());
        assertEquals(
                "Missing ParcelFileDescriptor",
                mErrorCaptor.getValue().getStatus().getDescription());
    }

    @Test
    @SmallTest
    public void testImportItem_invalidFileMetadata() {
        ImportItemRequest request =
                ImportItemRequest.newBuilder()
                        .setSessionId(ByteString.copyFromUtf8("test_session_id"))
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA)
                        .setFileMetadata(
                                Proto3Any.newBuilder()
                                        .setValue(ByteString.copyFromUtf8("invalid"))
                                        .build())
                        .build();
        Context.current()
                .withValue(DataImporterServiceImpl.PFD_CONTEXT_KEY, mMockPfd)
                .run(() -> mService.importItem(request, mImportItemResponseObserver));
        verify(mImportItemResponseObserver).onError(mErrorCaptor.capture());
        assertEquals(
                Status.INVALID_ARGUMENT.getCode(), mErrorCaptor.getValue().getStatus().getCode());
        assertEquals(
                "Invalid or missing file_metadata",
                mErrorCaptor.getValue().getStatus().getDescription());
    }

    @Test
    @SmallTest
    public void testImportItem_unsupportedFileType() throws InvalidProtocolBufferException {
        BrowserFileMetadata metadata =
                BrowserFileMetadata.newBuilder()
                        .setFileType(BrowserFileType.BROWSER_FILE_TYPE_UNSPECIFIED)
                        .build();
        ImportItemRequest request =
                ImportItemRequest.newBuilder()
                        .setSessionId(ByteString.copyFromUtf8("test_session_id"))
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA)
                        .setFileMetadata(
                                Proto3Any.newBuilder()
                                        .setValue(ByteString.copyFrom(metadata.toByteArray()))
                                        .build())
                        .build();
        Context.current()
                .withValue(DataImporterServiceImpl.PFD_CONTEXT_KEY, mMockPfd)
                .run(() -> mService.importItem(request, mImportItemResponseObserver));
        verify(mImportItemResponseObserver).onError(mErrorCaptor.capture());
        assertEquals(
                Status.INVALID_ARGUMENT.getCode(), mErrorCaptor.getValue().getStatus().getCode());
        assertEquals(
                "Invalid or unrecognized file type",
                mErrorCaptor.getValue().getStatus().getDescription());
    }

    @Test
    @SmallTest
    public void testImportItem_bookmarks() {
        TargetHandshakeRequest handshakeRequest =
                TargetHandshakeRequest.newBuilder()
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA)
                        .setSessionId(ByteString.copyFromUtf8("test_session_id"))
                        .build();
        mService.handshake(handshakeRequest, mHandshakeResponseObserver);

        BrowserFileMetadata metadata =
                BrowserFileMetadata.newBuilder()
                        .setFileType(BrowserFileType.BROWSER_FILE_TYPE_BOOKMARKS)
                        .build();
        ImportItemRequest request =
                ImportItemRequest.newBuilder()
                        .setSessionId(ByteString.copyFromUtf8("test_session_id"))
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA)
                        .setFileMetadata(
                                Proto3Any.newBuilder()
                                        .setValue(ByteString.copyFrom(metadata.toByteArray()))
                                        .build())
                        .build();

        Context.current()
                .withValue(DataImporterServiceImpl.PFD_CONTEXT_KEY, mMockPfd)
                .run(() -> mService.importItem(request, mImportItemResponseObserver));
        runOnUiThreadBlocking(
                () -> {
                    verify(mBridge).importBookmarks(anyInt(), mImportResultCallback.capture());
                    // Simulate the bridge returning a result.
                    mImportResultCallback.getValue().onResult(1);
                });

        verify(mImportItemResponseObserver).onNext(any(ImportItemResponse.class));
        verify(mImportItemResponseObserver).onCompleted();
    }

    @Test
    @SmallTest
    public void testImportItem_readingList() {
        TargetHandshakeRequest handshakeRequest =
                TargetHandshakeRequest.newBuilder()
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA)
                        .setSessionId(ByteString.copyFromUtf8("test_session_id"))
                        .build();
        mService.handshake(handshakeRequest, mHandshakeResponseObserver);

        BrowserFileMetadata metadata =
                BrowserFileMetadata.newBuilder()
                        .setFileType(BrowserFileType.BROWSER_FILE_TYPE_READING_LIST)
                        .build();
        ImportItemRequest request =
                ImportItemRequest.newBuilder()
                        .setSessionId(ByteString.copyFromUtf8("test_session_id"))
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA)
                        .setFileMetadata(
                                Proto3Any.newBuilder()
                                        .setValue(ByteString.copyFrom(metadata.toByteArray()))
                                        .build())
                        .build();

        Context.current()
                .withValue(DataImporterServiceImpl.PFD_CONTEXT_KEY, mMockPfd)
                .run(() -> mService.importItem(request, mImportItemResponseObserver));

        runOnUiThreadBlocking(
                () -> {
                    verify(mBridge).importReadingList(anyInt(), mImportResultCallback.capture());
                    // Simulate the bridge returning a result.
                    mImportResultCallback.getValue().onResult(1);
                });

        verify(mImportItemResponseObserver).onNext(any(ImportItemResponse.class));
        verify(mImportItemResponseObserver).onCompleted();
    }

    @Test
    @SmallTest
    public void testImportItem_history() {
        TargetHandshakeRequest handshakeRequest =
                TargetHandshakeRequest.newBuilder()
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA)
                        .setSessionId(ByteString.copyFromUtf8("test_session_id"))
                        .build();
        mService.handshake(handshakeRequest, mHandshakeResponseObserver);

        BrowserFileMetadata metadata =
                BrowserFileMetadata.newBuilder()
                        .setFileType(BrowserFileType.BROWSER_FILE_TYPE_BROWSING_HISTORY)
                        .build();
        ImportItemRequest request =
                ImportItemRequest.newBuilder()
                        .setSessionId(ByteString.copyFromUtf8("test_session_id"))
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA)
                        .setFileMetadata(
                                Proto3Any.newBuilder()
                                        .setValue(ByteString.copyFrom(metadata.toByteArray()))
                                        .build())
                        .build();

        Context.current()
                .withValue(DataImporterServiceImpl.PFD_CONTEXT_KEY, mMockPfd)
                .run(() -> mService.importItem(request, mImportItemResponseObserver));

        runOnUiThreadBlocking(
                () -> {
                    verify(mBridge).importHistory(anyInt(), mImportResultCallback.capture());
                    // Simulate the bridge returning a result.
                    mImportResultCallback.getValue().onResult(1);
                });

        verify(mImportItemResponseObserver).onNext(any(ImportItemResponse.class));
        verify(mImportItemResponseObserver).onCompleted();
    }

    @Test
    @SmallTest
    public void testImportItemsDone_unsupportedItemType() {
        ImportItemsDoneRequest request =
                ImportItemsDoneRequest.newBuilder()
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_UNSPECIFIED)
                        .build();
        mService.importItemsDone(request, mImportItemsDoneResponseObserver);
        verify(mImportItemsDoneResponseObserver).onError(mErrorCaptor.capture());
        assertEquals(
                Status.INVALID_ARGUMENT.getCode(), mErrorCaptor.getValue().getStatus().getCode());
    }

    @Test
    @SmallTest
    public void testImportItemsDone_missingSessionId() {
        ImportItemsDoneRequest request =
                ImportItemsDoneRequest.newBuilder()
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA)
                        .build();
        mService.importItemsDone(request, mImportItemsDoneResponseObserver);
        verify(mImportItemsDoneResponseObserver).onError(mErrorCaptor.capture());
        assertEquals(
                Status.INVALID_ARGUMENT.getCode(), mErrorCaptor.getValue().getStatus().getCode());
        assertEquals("Missing session_id", mErrorCaptor.getValue().getStatus().getDescription());
    }

    @Test
    @SmallTest
    public void testImportItemsDone_unknownSessionId() {
        ImportItemsDoneRequest request =
                ImportItemsDoneRequest.newBuilder()
                        .setSessionId(ByteString.copyFromUtf8("unknown_session_id"))
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA)
                        .build();
        mService.importItemsDone(request, mImportItemsDoneResponseObserver);
        verify(mImportItemsDoneResponseObserver).onError(mErrorCaptor.capture());
        assertEquals(
                Status.INVALID_ARGUMENT.getCode(), mErrorCaptor.getValue().getStatus().getCode());
        assertEquals("Unknown session_id", mErrorCaptor.getValue().getStatus().getDescription());
    }

    @Test
    @SmallTest
    public void testImportItemsDone_success() {
        TargetHandshakeRequest handshakeRequest =
                TargetHandshakeRequest.newBuilder()
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA)
                        .setSessionId(ByteString.copyFromUtf8("test_session_id"))
                        .build();
        mService.handshake(handshakeRequest, mHandshakeResponseObserver);

        BrowserFileMetadata metadata =
                BrowserFileMetadata.newBuilder()
                        .setFileType(BrowserFileType.BROWSER_FILE_TYPE_BOOKMARKS)
                        .build();
        ImportItemRequest importRequest =
                ImportItemRequest.newBuilder()
                        .setSessionId(ByteString.copyFromUtf8("test_session_id"))
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA)
                        .setFileMetadata(
                                Proto3Any.newBuilder()
                                        .setValue(ByteString.copyFrom(metadata.toByteArray()))
                                        .build())
                        .build();

        Context.current()
                .withValue(DataImporterServiceImpl.PFD_CONTEXT_KEY, mMockPfd)
                .run(() -> mService.importItem(importRequest, mImportItemResponseObserver));

        // Simulate the bridge returning a result.
        runOnUiThreadBlocking(
                () -> {
                    verify(mBridge).importBookmarks(anyInt(), mImportResultCallback.capture());
                    mImportResultCallback.getValue().onResult(5);
                });

        ImportItemsDoneRequest doneRequest =
                ImportItemsDoneRequest.newBuilder()
                        .setSessionId(ByteString.copyFromUtf8("test_session_id"))
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA)
                        .build();
        mService.importItemsDone(doneRequest, mImportItemsDoneResponseObserver);

        verify(mImportItemsDoneResponseObserver).onNext(mImportItemsDoneResponseCaptor.capture());
        verify(mImportItemsDoneResponseObserver).onCompleted();

        ImportItemsDoneResponse doneResponse = mImportItemsDoneResponseCaptor.getValue();
        assertEquals(1, doneResponse.getSuccessItemCount());
        assertEquals(0, doneResponse.getFailedItemCount());
        assertEquals(0, doneResponse.getIgnoredItemCount());

        runOnUiThreadBlocking(
                () -> {
                    verify(mBridge).destroy();
                });
    }

    @Test
    @SmallTest
    public void testImportItemsDone_failedImport() {
        TargetHandshakeRequest handshakeRequest =
                TargetHandshakeRequest.newBuilder()
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA)
                        .setSessionId(ByteString.copyFromUtf8("test_session_id"))
                        .build();
        mService.handshake(handshakeRequest, mHandshakeResponseObserver);

        BrowserFileMetadata metadata =
                BrowserFileMetadata.newBuilder()
                        .setFileType(BrowserFileType.BROWSER_FILE_TYPE_BOOKMARKS)
                        .build();
        ImportItemRequest importRequest =
                ImportItemRequest.newBuilder()
                        .setSessionId(ByteString.copyFromUtf8("test_session_id"))
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA)
                        .setFileMetadata(
                                Proto3Any.newBuilder()
                                        .setValue(ByteString.copyFrom(metadata.toByteArray()))
                                        .build())
                        .build();

        Context.current()
                .withValue(DataImporterServiceImpl.PFD_CONTEXT_KEY, mMockPfd)
                .run(() -> mService.importItem(importRequest, mImportItemResponseObserver));

        // Simulate the bridge returning a failed result (-1).
        runOnUiThreadBlocking(
                () -> {
                    verify(mBridge).importBookmarks(anyInt(), mImportResultCallback.capture());
                    mImportResultCallback.getValue().onResult(-1);
                });

        ImportItemsDoneRequest doneRequest =
                ImportItemsDoneRequest.newBuilder()
                        .setSessionId(ByteString.copyFromUtf8("test_session_id"))
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA)
                        .build();
        mService.importItemsDone(doneRequest, mImportItemsDoneResponseObserver);

        verify(mImportItemsDoneResponseObserver).onNext(mImportItemsDoneResponseCaptor.capture());
        verify(mImportItemsDoneResponseObserver).onCompleted();

        ImportItemsDoneResponse doneResponse = mImportItemsDoneResponseCaptor.getValue();
        assertEquals(0, doneResponse.getSuccessItemCount());
        assertEquals(1, doneResponse.getFailedItemCount());
        assertEquals(0, doneResponse.getIgnoredItemCount());

        runOnUiThreadBlocking(
                () -> {
                    verify(mBridge).destroy();
                });
    }
}
