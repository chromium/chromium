// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.pm.Signature;

import androidx.appsearch.app.AppSearchSession;
import androidx.appsearch.app.GenericDocument;
import androidx.appsearch.app.PackageIdentifier;
import androidx.appsearch.app.PutDocumentsRequest;
import androidx.appsearch.app.SetSchemaRequest;
import androidx.appsearch.builtintypes.WebPage;
import androidx.appsearch.exceptions.AppSearchException;

import com.google.common.util.concurrent.Futures;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;

import java.util.List;
import java.util.Set;

/** Unit tests for AuxiliarySearchDonationServiceBridge. */
@RunWith(BaseRobolectricTestRunner.class)
public class AuxiliarySearchDonationServiceBridgeUnitTest {
    private static final String TEST_ID = "123";
    private static final String TEST_URL = "https://example.com";
    private static final String TEST_TITLE = "Example";
    private static final long TEST_LAST_VISITED = 1000L;
    private static final byte[] TEST_SHA256 =
            new Signature("495761734e65766572426f6f6b536d6172742c49276d4d6f6e6579536d617274")
                    .toByteArray();
    private static final Set<PackageIdentifier> TEST_INTELLIGENCE_PACKAGES =
            Set.of(new PackageIdentifier("org.chromium.test.intelligence", TEST_SHA256));

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AuxiliarySearchHooks mMockHooks;
    @Mock private AppSearchStorageFactory mMockFactory;
    @Mock private AppSearchSession mMockSession;

    @Captor private ArgumentCaptor<SetSchemaRequest> mSetSchemaRequestCaptor;
    @Captor private ArgumentCaptor<PutDocumentsRequest> mPutDocumentsRequestCaptor;

    @Before
    public void setUp() {
        when(mMockHooks.getPackagesForBrowsingDataVisibility())
                .thenReturn(TEST_INTELLIGENCE_PACKAGES);
        ServiceLoaderUtil.setInstanceForTesting(AuxiliarySearchHooks.class, mMockHooks);
        AppSearchStorageFactory.setInstanceForTesting(mMockFactory);
    }

    @Test
    public void testCreateHistoryDocument() {
        WebPage webPage =
                AuxiliarySearchDonationServiceBridge.createHistoryDocument(
                        TEST_ID, TEST_URL, TEST_TITLE, TEST_LAST_VISITED);

        assertEquals(TEST_ID, webPage.getId());
        assertEquals(
                AuxiliarySearchDonationServiceBridge.HISTORY_NAMESPACE, webPage.getNamespace());
        assertEquals(TEST_URL, webPage.getUrl());
        assertEquals(TEST_TITLE, webPage.getName());
        assertEquals(TEST_LAST_VISITED, webPage.getCreationTimestampMillis());
        assertEquals(
                AuxiliarySearchDonationServiceBridge.HISTORY_DOCUMENT_TTL_MILLIS,
                webPage.getDocumentTtlMillis());
    }

    @Test
    public void testConstructor_unsupportedAndroidVersion() {
        // mMockFactory.createSearchSessionAsync returns null by default.
        var bridge = new AuxiliarySearchDonationServiceBridge();
        RobolectricUtil.runAllBackgroundAndUi();

        assertNull(bridge.mSessionFuture);
    }

    @Test
    public void testConstructor_setsSchema() {
        when(mMockFactory.createSearchSessionAsync(anyString()))
                .thenReturn(Futures.immediateFuture(mMockSession));
        when(mMockSession.setSchemaAsync(any())).thenReturn(Futures.immediateFuture(null));

        var bridge = new AuxiliarySearchDonationServiceBridge();
        RobolectricUtil.runAllBackgroundAndUi();

        assertTrue(bridge.mSessionFuture.isDone());
        verify(mMockFactory)
                .createSearchSessionAsync(eq(AuxiliarySearchDonationServiceBridge.DATABASE_NAME));
        verify(mMockSession).setSchemaAsync(mSetSchemaRequestCaptor.capture());
        SetSchemaRequest request = mSetSchemaRequestCaptor.getValue();
        assertTrue(request.isForceOverride());
        assertTrue(request.getSchemasNotDisplayedBySystem().contains(WebPage.SCHEMA_NAME));
        assertTrue(
                request.getSchemas().stream()
                        .anyMatch(s -> s.getSchemaType().equals(WebPage.SCHEMA_NAME)));
        assertEquals(
                TEST_INTELLIGENCE_PACKAGES,
                request.getSchemasVisibleToPackages().get(WebPage.SCHEMA_NAME));
    }

    @Test
    public void testDonateHistory() throws AppSearchException {
        when(mMockFactory.createSearchSessionAsync(anyString()))
                .thenReturn(Futures.immediateFuture(mMockSession));
        when(mMockSession.setSchemaAsync(any())).thenReturn(Futures.immediateFuture(null));
        when(mMockSession.putAsync(any())).thenReturn(Futures.immediateFuture(null));
        var bridge = new AuxiliarySearchDonationServiceBridge();
        RobolectricUtil.runAllBackgroundAndUi();
        WebPage page =
                AuxiliarySearchDonationServiceBridge.createHistoryDocument(
                        TEST_ID, TEST_URL, TEST_TITLE, TEST_LAST_VISITED);

        bridge.donateHistory(List.of(page));
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mMockSession).putAsync(mPutDocumentsRequestCaptor.capture());
        PutDocumentsRequest request = mPutDocumentsRequestCaptor.getValue();
        List<GenericDocument> documents = request.getGenericDocuments();
        assertEquals(1, documents.size());
        WebPage webPage = documents.get(0).toDocumentClass(WebPage.class);
        assertEquals(TEST_ID, webPage.getId());
        assertEquals(
                AuxiliarySearchDonationServiceBridge.HISTORY_NAMESPACE, webPage.getNamespace());
        assertEquals(TEST_URL, webPage.getUrl());
        assertEquals(TEST_TITLE, webPage.getName());
        assertEquals(TEST_LAST_VISITED, webPage.getCreationTimestampMillis());
        assertEquals(
                AuxiliarySearchDonationServiceBridge.HISTORY_DOCUMENT_TTL_MILLIS,
                webPage.getDocumentTtlMillis());
    }

    @Test
    public void testDonateHistory_unsupportedAndroidVersion() {
        // mMockFactory.createSearchSessionAsync returns null by default.
        var bridge = new AuxiliarySearchDonationServiceBridge();
        RobolectricUtil.runAllBackgroundAndUi();
        WebPage page =
                AuxiliarySearchDonationServiceBridge.createHistoryDocument(
                        TEST_ID, TEST_URL, TEST_TITLE, TEST_LAST_VISITED);

        bridge.donateHistory(List.of(page));
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mMockSession, never()).putAsync(any());
    }

    @Test
    public void testDonateHistory_emptyPages() {
        when(mMockFactory.createSearchSessionAsync(anyString()))
                .thenReturn(Futures.immediateFuture(mMockSession));
        var bridge = new AuxiliarySearchDonationServiceBridge();
        RobolectricUtil.runAllBackgroundAndUi();

        bridge.donateHistory(List.of());
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mMockSession, never()).putAsync(any());
    }
}
