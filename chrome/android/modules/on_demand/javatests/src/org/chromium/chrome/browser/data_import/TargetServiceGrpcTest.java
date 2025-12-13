// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_import;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.os.ParcelFileDescriptor;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import com.google.protobuf.ByteString;

import io.grpc.ManagedChannel;
import io.grpc.Metadata;
import io.grpc.ServerInterceptors;
import io.grpc.inprocess.InProcessChannelBuilder;
import io.grpc.inprocess.InProcessServerBuilder;
import io.grpc.stub.MetadataUtils;
import io.grpc.testing.GrpcCleanupRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.url.GURL;

import java.io.File;
import java.io.IOException;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Integration tests for the {@link TargetService} gRPC service.
 *
 * <p>This test class sets up an in-process gRPC server with the {@link TargetService} and tests its
 * RPCs.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@Features.EnableFeatures(ChromeFeatureList.ANDROID_DATA_IMPORTER_SERVICE)
public class TargetServiceGrpcTest {

    @Rule public GrpcCleanupRule mGrpcCleanup = new GrpcCleanupRule();

    private static final ByteString SESSION_ID = ByteString.copyFromUtf8("session_id");

    private TargetService mTargetService;
    private TargetServiceGrpc.TargetServiceBlockingStub mStub;

    @Before
    public void setUp() throws IOException {
        runOnUiThreadBlocking(
                () -> ChromeBrowserInitializer.getInstance().handleSynchronousStartup());
        String serverName = InProcessServerBuilder.generateName();
        mTargetService = new TargetService();

        mGrpcCleanup.register(
                InProcessServerBuilder.forName(serverName)
                        .addService(
                                ServerInterceptors.intercept(
                                        mTargetService,
                                        new DataImporterServiceImpl
                                                .ParcelableMetadataInterceptor()))
                        .directExecutor()
                        .build()
                        .start());

        ManagedChannel channel =
                mGrpcCleanup.register(
                        InProcessChannelBuilder.forName(serverName).directExecutor().build());

        mStub = TargetServiceGrpc.newBlockingStub(channel);
    }

    @Test
    @SmallTest
    public void testHandshake_supportedType() {
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

    @Test
    @MediumTest
    public void testImportBookmarks_Basic() throws Exception {
        String bookmarksFilePath = UrlUtils.getTestFilePath("android/bookmarks/Valid_entries.html");
        ParcelFileDescriptor file =
                ParcelFileDescriptor.open(
                        new File(bookmarksFilePath), ParcelFileDescriptor.MODE_READ_ONLY);

        importBrowserFile(file, BrowserFileType.BROWSER_FILE_TYPE_BOOKMARKS);

        final BookmarkModel bookmarkModel = waitForBookmarkModelLoaded();
        runOnUiThreadBlocking(
                () -> {
                    GURL url = new GURL("https://www.google.com/");
                    assertTrue("Bookmark should be present", bookmarkModel.isBookmarked(url));
                    List<BookmarkId> bookmarkIds = bookmarkModel.searchBookmarks(url.getSpec(), 2);
                    assertEquals("Exactly one bookmark should be found", 1, bookmarkIds.size());
                    BookmarkId bookmarkId = bookmarkIds.get(0);
                    assertEquals(
                            "Bookmark title should match",
                            "Google",
                            bookmarkModel.getBookmarkById(bookmarkId).getTitle());

                    url = new GURL("https://login.live.com/");
                    assertTrue("Bookmark should be present", bookmarkModel.isBookmarked(url));
                    bookmarkIds = bookmarkModel.searchBookmarks(url.getSpec(), 2);
                    assertEquals("Exactly one bookmark should be found", 1, bookmarkIds.size());
                    bookmarkId = bookmarkIds.get(0);
                    assertEquals(
                            "Bookmark title should match",
                            "Outlook",
                            bookmarkModel.getBookmarkById(bookmarkId).getTitle());

                    url = new GURL("http://www.speedtest.net/");
                    assertTrue("Bookmark should be present", bookmarkModel.isBookmarked(url));
                    bookmarkIds = bookmarkModel.searchBookmarks(url.getSpec(), 2);
                    assertEquals("Exactly one bookmark should be found", 1, bookmarkIds.size());
                    bookmarkId = bookmarkIds.get(0);
                    assertEquals(
                            "Bookmark title should match",
                            "Speed Test",
                            bookmarkModel.getBookmarkById(bookmarkId).getTitle());
                });
    }

    @Test
    @MediumTest
    public void testImportReadingList() throws Exception {
        String readinglistFilePath =
                UrlUtils.getTestFilePath("android/bookmarks/Valid_reading_list_entry.html");
        ParcelFileDescriptor file =
                ParcelFileDescriptor.open(
                        new File(readinglistFilePath), ParcelFileDescriptor.MODE_READ_ONLY);

        importBrowserFile(file, BrowserFileType.BROWSER_FILE_TYPE_READING_LIST);

        runOnUiThreadBlocking(
                () -> ChromeBrowserInitializer.getInstance().handleSynchronousStartup());
        final BookmarkModel bookmarkModel = waitForBookmarkModelLoaded();
        runOnUiThreadBlocking(
                () -> {
                    BookmarkId readingListFolder =
                            bookmarkModel.getLocalOrSyncableReadingListFolder();
                    assertFalse(
                            "Reading list should not be empty",
                            bookmarkModel.getChildIds(readingListFolder).isEmpty());
                    GURL url = new GURL("https://www.chromium.org/");
                    assertTrue(
                            "Reading list item should be present", bookmarkModel.isBookmarked(url));
                    List<BookmarkId> bookmarkIds = bookmarkModel.searchBookmarks(url.getSpec(), 1);
                    assertFalse("Bookmark ID list should not be empty", bookmarkIds.isEmpty());
                    BookmarkId bookmarkId = bookmarkIds.get(0);
                    assertEquals(
                            "Reading list item title should match",
                            "Chromium",
                            bookmarkModel.getBookmarkById(bookmarkId).getTitle());
                    assertEquals(
                            "Item should be in reading list folder",
                            readingListFolder,
                            bookmarkModel.getBookmarkById(bookmarkId).getParentId());
                });
    }

    private void importBrowserFile(ParcelFileDescriptor readSide, BrowserFileType fileType) {
        Metadata headers = new Metadata();
        headers.put(
                DataImporterServiceImpl.ParcelableMetadataInterceptor.PFD_METADATA_KEY, readSide);

        BrowserFileMetadata fileMetadata =
                BrowserFileMetadata.newBuilder().setFileType(fileType).build();
        ImportItemRequest request =
                ImportItemRequest.newBuilder()
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA)
                        .setSessionId(SESSION_ID)
                        .setFileMetadata(
                                Proto3Any.newBuilder().setValue(fileMetadata.toByteString()))
                        .build();

        TargetServiceGrpc.TargetServiceBlockingStub stubWithHeaders =
                mStub.withInterceptors(MetadataUtils.newAttachHeadersInterceptor(headers));
        ImportItemResponse response = stubWithHeaders.importItem(request);
        assertNotNull(response);
        assertEquals(
                ImportItemResponse.TransferError.TRANSFER_ERROR_UNSPECIFIED,
                response.getTransferError());

        ImportItemsDoneRequest doneRequest =
                ImportItemsDoneRequest.newBuilder()
                        .setItemType(SystemAppApiItemType.SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA)
                        .setSessionId(SESSION_ID)
                        .setStatus(ImportItemsDoneRequest.CompleteStatus.COMPLETE_STATUS_SUCCESS)
                        .build();
        ImportItemsDoneResponse doneResponse = mStub.importItemsDone(doneRequest);
        assertEquals(
                /* amount of successful imported files */ 1, doneResponse.getSuccessItemCount());
    }

    /**
     * Waits until the bookmark model is loaded, i.e. until {@link
     * BookmarkModel#isBookmarkModelLoaded()} is true.
     */
    public static BookmarkModel waitForBookmarkModelLoaded() throws TimeoutException {
        final CallbackHelper loadedCallback = new CallbackHelper();
        final BookmarkModel bookmarkModel =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            BookmarkModel model =
                                    BookmarkModel.getForProfile(
                                            ProfileManager.getLastUsedRegularProfile());
                            if (model.isBookmarkModelLoaded()) {
                                loadedCallback.notifyCalled();
                            } else {
                                model.finishLoadingBookmarkModel(loadedCallback::notifyCalled);
                            }
                            return model;
                        });

        loadedCallback.waitForCallback(0);
        return bookmarkModel;
    }
}
