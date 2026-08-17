// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.pm.Signature;

import androidx.appsearch.app.AppSearchBatchResult;
import androidx.appsearch.app.AppSearchResult;
import androidx.appsearch.app.AppSearchSchema;
import androidx.appsearch.app.AppSearchSession;
import androidx.appsearch.app.GenericDocument;
import androidx.appsearch.app.PackageIdentifier;
import androidx.appsearch.app.PutDocumentsRequest;
import androidx.appsearch.app.SetSchemaRequest;
import androidx.appsearch.builtintypes.Account;
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
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.Log;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.google_apis.gaia.GaiaId;

import java.util.List;
import java.util.Set;

/** Unit tests for AuxiliarySearchDonationServiceBridge. */
@RunWith(BaseRobolectricTestRunner.class)
public class AuxiliarySearchDonationServiceBridgeUnitTest {
    private static final String EXPECTED_LOG_TAG = "cr_" + AuxiliarySearchDonationServiceBridge.TAG;
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
    public void testIsBrowsingDataDonationSupported_hooksNull() {
        ServiceLoaderUtil.setInstanceForTesting(AuxiliarySearchHooks.class, null);
        assertFalse(AuxiliarySearchDonationServiceBridge.isBrowsingDataDonationSupported());
    }

    @Test
    public void testIsBrowsingDataDonationSupported_hooksFalse() {
        when(mMockHooks.isBrowsingDataDonationSupported()).thenReturn(false);
        assertFalse(AuxiliarySearchDonationServiceBridge.isBrowsingDataDonationSupported());
    }

    @Test
    public void testIsBrowsingDataDonationSupported_hooksTrue() {
        when(mMockHooks.isBrowsingDataDonationSupported()).thenReturn(true);
        assertTrue(AuxiliarySearchDonationServiceBridge.isBrowsingDataDonationSupported());
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
    public void testConstructor_noConsumerPackages() {
        when(mMockHooks.getPackagesForBrowsingDataVisibility()).thenReturn(Set.of());
        var bridge =
                new AuxiliarySearchDonationServiceBridge(/* isBrowsingDataDonationEnabled= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        assertNull(bridge.mSessionFuture);
        verify(mMockFactory, never()).createSearchSessionAsync(any());
    }

    @Test
    public void testConstructor_unsupportedAndroidVersion() {
        // mMockFactory.createSearchSessionAsync returns null by default.
        var bridge =
                new AuxiliarySearchDonationServiceBridge(/* isBrowsingDataDonationEnabled= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        assertNull(bridge.mSessionFuture);
    }

    @Test
    public void testConstructor_setsSchema() {
        when(mMockFactory.createSearchSessionAsync(anyString()))
                .thenReturn(Futures.immediateFuture(mMockSession));
        when(mMockSession.setSchemaAsync(any())).thenReturn(Futures.immediateFuture(null));

        var bridge =
                new AuxiliarySearchDonationServiceBridge(/* isBrowsingDataDonationEnabled= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        assertTrue(bridge.mSessionFuture.isDone());
        verify(mMockFactory)
                .createSearchSessionAsync(eq(AuxiliarySearchDonationServiceBridge.DATABASE_NAME));
        verify(mMockSession).setSchemaAsync(mSetSchemaRequestCaptor.capture());
        SetSchemaRequest request = mSetSchemaRequestCaptor.getValue();
        assertTrue(request.isForceOverride());
        assertTrue(
                request.getSchemasNotDisplayedBySystem()
                        .contains(
                                AuxiliarySearchDonationServiceBridge.CHROME_WEB_PAGE_SCHEMA_NAME));

        assertTrue(
                request.getSchemas().stream()
                        .anyMatch(s -> s.getSchemaType().equals(WebPage.SCHEMA_NAME)));
        assertTrue(
                request.getSchemas().stream()
                        .anyMatch(
                                s ->
                                        s.getSchemaType()
                                                .equals(
                                                        AuxiliarySearchDonationServiceBridge
                                                                .BUILTIN_ACCOUNT_SCHEMA_NAME)));
        AppSearchSchema extendedWebPageSchema =
                request.getSchemas().stream()
                        .filter(
                                s ->
                                        s.getSchemaType()
                                                .equals(
                                                        AuxiliarySearchDonationServiceBridge
                                                                .CHROME_WEB_PAGE_SCHEMA_NAME))
                        .findFirst()
                        .orElse(null);
        assertNotNull(extendedWebPageSchema);
        assertEquals(List.of(WebPage.SCHEMA_NAME), extendedWebPageSchema.getParentTypes());

        AppSearchSchema.PropertyConfig accountProperty =
                extendedWebPageSchema.getProperties().stream()
                        .filter(
                                p ->
                                        p.getName()
                                                .equals(
                                                        AuxiliarySearchDonationServiceBridge
                                                                .ACCOUNT_PROPERTY_NAME))
                        .findFirst()
                        .orElse(null);
        assertNotNull(accountProperty);
        assertEquals(
                AppSearchSchema.PropertyConfig.CARDINALITY_OPTIONAL,
                accountProperty.getCardinality());
        assertTrue(accountProperty instanceof AppSearchSchema.DocumentPropertyConfig);
        assertEquals(
                AuxiliarySearchDonationServiceBridge.BUILTIN_ACCOUNT_SCHEMA_NAME,
                ((AppSearchSchema.DocumentPropertyConfig) accountProperty).getSchemaType());
        assertTrue(
                ((AppSearchSchema.DocumentPropertyConfig) accountProperty)
                        .shouldIndexNestedProperties());

        assertEquals(
                TEST_INTELLIGENCE_PACKAGES,
                request.getSchemasVisibleToPackages()
                        .get(AuxiliarySearchDonationServiceBridge.CHROME_WEB_PAGE_SCHEMA_NAME));
    }

    @Test
    public void testConstructor_setsSchema_browsingDataDonationDisabled() {
        when(mMockFactory.createSearchSessionAsync(anyString()))
                .thenReturn(Futures.immediateFuture(mMockSession));
        when(mMockSession.setSchemaAsync(any())).thenReturn(Futures.immediateFuture(null));

        var bridge =
                new AuxiliarySearchDonationServiceBridge(
                        /* isBrowsingDataDonationEnabled= */ false);
        RobolectricUtil.runAllBackgroundAndUi();

        assertTrue(bridge.mSessionFuture.isDone());
        verify(mMockFactory)
                .createSearchSessionAsync(eq(AuxiliarySearchDonationServiceBridge.DATABASE_NAME));
        verify(mMockSession).setSchemaAsync(mSetSchemaRequestCaptor.capture());
        SetSchemaRequest request = mSetSchemaRequestCaptor.getValue();
        assertTrue(
                request.getSchemasVisibleToPackages()
                        .getOrDefault(
                                AuxiliarySearchDonationServiceBridge.CHROME_WEB_PAGE_SCHEMA_NAME,
                                Set.of())
                        .isEmpty());
    }

    @Test
    public void testSetSchema_updatesSchema() {
        when(mMockFactory.createSearchSessionAsync(anyString()))
                .thenReturn(Futures.immediateFuture(mMockSession));
        when(mMockSession.setSchemaAsync(any())).thenReturn(Futures.immediateFuture(null));

        var bridge =
                new AuxiliarySearchDonationServiceBridge(/* isBrowsingDataDonationEnabled= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        bridge.setSchema(/* isBrowsingDataDonationEnabled= */ false);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mMockSession, times(2)).setSchemaAsync(mSetSchemaRequestCaptor.capture());
        SetSchemaRequest updatedRequest = mSetSchemaRequestCaptor.getValue();
        assertTrue(
                updatedRequest
                        .getSchemasVisibleToPackages()
                        .getOrDefault(
                                AuxiliarySearchDonationServiceBridge.CHROME_WEB_PAGE_SCHEMA_NAME,
                                Set.of())
                        .isEmpty());

        bridge.setSchema(/* isBrowsingDataDonationEnabled= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mMockSession, times(3)).setSchemaAsync(mSetSchemaRequestCaptor.capture());
        SetSchemaRequest reenabledRequest = mSetSchemaRequestCaptor.getValue();
        assertEquals(
                TEST_INTELLIGENCE_PACKAGES,
                reenabledRequest
                        .getSchemasVisibleToPackages()
                        .get(AuxiliarySearchDonationServiceBridge.CHROME_WEB_PAGE_SCHEMA_NAME));
    }

    @Test
    public void testSetSchema_noConsumerPackages() {
        when(mMockFactory.createSearchSessionAsync(anyString()))
                .thenReturn(Futures.immediateFuture(mMockSession));
        when(mMockSession.setSchemaAsync(any())).thenReturn(Futures.immediateFuture(null));
        var bridge =
                new AuxiliarySearchDonationServiceBridge(/* isBrowsingDataDonationEnabled= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        // Consumer packages become unavailable.
        when(mMockHooks.getPackagesForBrowsingDataVisibility()).thenReturn(Set.of());
        bridge.setSchema(/* isBrowsingDataDonationEnabled= */ false);
        RobolectricUtil.runAllBackgroundAndUi();

        // Should only have been called once during construction, not for the second setSchema call.
        verify(mMockSession, times(1)).setSchemaAsync(any());
    }

    @Test
    public void testSetSchema_unsupportedAndroidVersion() {
        // mMockFactory.createSearchSessionAsync returns null by default.
        var bridge =
                new AuxiliarySearchDonationServiceBridge(/* isBrowsingDataDonationEnabled= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        bridge.setSchema(/* isBrowsingDataDonationEnabled= */ false);
        RobolectricUtil.runAllBackgroundAndUi();

        // Should not crash.
    }

    @Test
    public void testSetSchema_futureFailure_logsWarning() {
        when(mMockFactory.createSearchSessionAsync(anyString()))
                .thenReturn(Futures.immediateFuture(mMockSession));
        when(mMockSession.setSchemaAsync(any()))
                .thenReturn(Futures.immediateFuture(null))
                .thenReturn(Futures.immediateFailedFuture(new RuntimeException("IPC error")));
        // Suppress log spam in tests.
        ShadowLog.stream = null;
        var bridge =
                new AuxiliarySearchDonationServiceBridge(/* isBrowsingDataDonationEnabled= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        bridge.setSchema(/* isBrowsingDataDonationEnabled= */ false);
        RobolectricUtil.runAllBackgroundAndUi();

        assertTrue(
                "Expected schema warning log was not found",
                ShadowLog.getLogs().stream()
                        .anyMatch(
                                item ->
                                        EXPECTED_LOG_TAG.equals(item.tag)
                                                && item.type == Log.WARN
                                                && item.throwable instanceof RuntimeException));
    }

    @Test
    public void testSetSchema_sessionFailure_doesNotLogWarning() {
        when(mMockFactory.createSearchSessionAsync(anyString()))
                .thenReturn(Futures.immediateFailedFuture(new RuntimeException("Session error")));
        // Suppress log spam in tests.
        ShadowLog.stream = null;
        var bridge =
                new AuxiliarySearchDonationServiceBridge(/* isBrowsingDataDonationEnabled= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        bridge.setSchema(/* isBrowsingDataDonationEnabled= */ false);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mMockSession, never()).setSchemaAsync(any());
        assertTrue(
                "Warning log was unexpectedly found when session failed",
                ShadowLog.getLogs().stream().noneMatch(item -> EXPECTED_LOG_TAG.equals(item.tag)));
    }

    @Test
    public void testDonateHistory() throws AppSearchException {
        when(mMockFactory.createSearchSessionAsync(anyString()))
                .thenReturn(Futures.immediateFuture(mMockSession));
        when(mMockSession.setSchemaAsync(any())).thenReturn(Futures.immediateFuture(null));
        when(mMockSession.putAsync(any()))
                .thenReturn(
                        Futures.immediateFuture(
                                new AppSearchBatchResult.Builder<String, Void>().build()));
        var bridge =
                new AuxiliarySearchDonationServiceBridge(/* isBrowsingDataDonationEnabled= */ true);
        RobolectricUtil.runAllBackgroundAndUi();
        WebPage page =
                AuxiliarySearchDonationServiceBridge.createHistoryDocument(
                        TEST_ID, TEST_URL, TEST_TITLE, TEST_LAST_VISITED);

        bridge.donateHistory(List.of(page), /* coreAccountInfo= */ null);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mMockSession).putAsync(mPutDocumentsRequestCaptor.capture());
        PutDocumentsRequest request = mPutDocumentsRequestCaptor.getValue();
        List<GenericDocument> documents = request.getGenericDocuments();
        assertEquals(1, documents.size());
        GenericDocument actualDoc = documents.get(0);
        assertEquals(
                AuxiliarySearchDonationServiceBridge.CHROME_WEB_PAGE_SCHEMA_NAME,
                actualDoc.getSchemaType());
        WebPage webPage = actualDoc.toDocumentClass(WebPage.class);
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
        var bridge =
                new AuxiliarySearchDonationServiceBridge(/* isBrowsingDataDonationEnabled= */ true);
        RobolectricUtil.runAllBackgroundAndUi();
        WebPage page =
                AuxiliarySearchDonationServiceBridge.createHistoryDocument(
                        TEST_ID, TEST_URL, TEST_TITLE, TEST_LAST_VISITED);

        bridge.donateHistory(List.of(page), /* coreAccountInfo= */ null);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mMockSession, never()).putAsync(any());
    }

    @Test
    public void testDonateHistory_emptyPages() {
        when(mMockFactory.createSearchSessionAsync(anyString()))
                .thenReturn(Futures.immediateFuture(mMockSession));
        var bridge =
                new AuxiliarySearchDonationServiceBridge(/* isBrowsingDataDonationEnabled= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        bridge.donateHistory(List.of(), /* coreAccountInfo= */ null);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mMockSession, never()).putAsync(any());
    }

    @Test
    public void testDonateHistory_withAccount() throws AppSearchException {
        when(mMockFactory.createSearchSessionAsync(anyString()))
                .thenReturn(Futures.immediateFuture(mMockSession));
        when(mMockSession.setSchemaAsync(any())).thenReturn(Futures.immediateFuture(null));
        when(mMockSession.putAsync(any()))
                .thenReturn(
                        Futures.immediateFuture(
                                new AppSearchBatchResult.Builder<String, Void>().build()));
        var bridge =
                new AuxiliarySearchDonationServiceBridge(/* isBrowsingDataDonationEnabled= */ true);
        RobolectricUtil.runAllBackgroundAndUi();
        WebPage page =
                AuxiliarySearchDonationServiceBridge.createHistoryDocument(
                        TEST_ID, TEST_URL, TEST_TITLE, TEST_LAST_VISITED);
        CoreAccountInfo coreAccountInfo =
                CoreAccountInfo.createFromEmailAndGaiaId(
                        "test_email@gmail.com", new GaiaId("test_gaia_id"));

        bridge.donateHistory(List.of(page), coreAccountInfo);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mMockSession).putAsync(mPutDocumentsRequestCaptor.capture());
        PutDocumentsRequest request = mPutDocumentsRequestCaptor.getValue();
        List<GenericDocument> documents = request.getGenericDocuments();
        assertEquals(1, documents.size());
        GenericDocument actualDoc = documents.get(0);
        assertEquals(
                AuxiliarySearchDonationServiceBridge.CHROME_WEB_PAGE_SCHEMA_NAME,
                actualDoc.getSchemaType());
        assertEquals(TEST_ID, actualDoc.getId());
        WebPage webPage = actualDoc.toDocumentClass(WebPage.class);
        assertEquals(TEST_ID, webPage.getId());
        GenericDocument nestedAccountDoc =
                actualDoc.getPropertyDocument(
                        AuxiliarySearchDonationServiceBridge.ACCOUNT_PROPERTY_NAME);
        assertNotNull(nestedAccountDoc);
        Account actualAccount = nestedAccountDoc.toDocumentClass(Account.class);
        assertEquals("test_gaia_id", actualAccount.getId());
        assertEquals("test_gaia_id", actualAccount.getAccountId());
        assertEquals("test_email@gmail.com", actualAccount.getAccountName());
        assertEquals(
                AuxiliarySearchDonationServiceBridge.ACCOUNT_TYPE_GOOGLE,
                actualAccount.getAccountType());
    }

    @Test
    public void testDonateHistory_futureFailure_logsWarning() {
        when(mMockFactory.createSearchSessionAsync(anyString()))
                .thenReturn(Futures.immediateFuture(mMockSession));
        when(mMockSession.setSchemaAsync(any())).thenReturn(Futures.immediateFuture(null));
        when(mMockSession.putAsync(any()))
                .thenReturn(Futures.immediateFailedFuture(new RuntimeException("IPC error")));
        // Suppress log spam in tests.
        ShadowLog.stream = null;
        var bridge =
                new AuxiliarySearchDonationServiceBridge(/* isBrowsingDataDonationEnabled= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        WebPage page =
                AuxiliarySearchDonationServiceBridge.createHistoryDocument(
                        TEST_ID, TEST_URL, TEST_TITLE, TEST_LAST_VISITED);
        bridge.donateHistory(List.of(page), /* coreAccountInfo= */ null);
        RobolectricUtil.runAllBackgroundAndUi();

        assertTrue(
                "Expected pipeline warning log was not found",
                ShadowLog.getLogs().stream()
                        .anyMatch(
                                item ->
                                        EXPECTED_LOG_TAG.equals(item.tag)
                                                && item.type == Log.WARN
                                                && item.throwable instanceof RuntimeException));
    }

    @Test
    public void testDonateHistory_sessionFailure_doesNotLogWarning() {
        when(mMockFactory.createSearchSessionAsync(anyString()))
                .thenReturn(Futures.immediateFailedFuture(new RuntimeException("Session error")));
        // Suppress log spam in tests.
        ShadowLog.stream = null;
        var bridge =
                new AuxiliarySearchDonationServiceBridge(/* isBrowsingDataDonationEnabled= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        WebPage page =
                AuxiliarySearchDonationServiceBridge.createHistoryDocument(
                        TEST_ID, TEST_URL, TEST_TITLE, TEST_LAST_VISITED);
        bridge.donateHistory(List.of(page), /* coreAccountInfo= */ null);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mMockSession, never()).putAsync(any());
        assertTrue(
                "Warning log was unexpectedly found when session failed",
                ShadowLog.getLogs().stream().noneMatch(item -> EXPECTED_LOG_TAG.equals(item.tag)));
    }

    @Test
    public void testDonateHistory_schemaUpdateFailure_doesNotDonate() {
        when(mMockFactory.createSearchSessionAsync(anyString()))
                .thenReturn(Futures.immediateFuture(mMockSession));
        when(mMockSession.setSchemaAsync(any()))
                .thenReturn(Futures.immediateFuture(null))
                .thenReturn(
                        Futures.immediateFailedFuture(new RuntimeException("Schema update error")));
        // Suppress log spam in tests.
        ShadowLog.stream = null;
        var bridge =
                new AuxiliarySearchDonationServiceBridge(/* isBrowsingDataDonationEnabled= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        bridge.setSchema(/* isBrowsingDataDonationEnabled= */ false);
        RobolectricUtil.runAllBackgroundAndUi();

        WebPage page =
                AuxiliarySearchDonationServiceBridge.createHistoryDocument(
                        TEST_ID, TEST_URL, TEST_TITLE, TEST_LAST_VISITED);
        bridge.donateHistory(List.of(page), /* coreAccountInfo= */ null);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mMockSession, never()).putAsync(any());
    }

    @Test
    public void testDonateHistory_batchFailure_logsWarning() {
        when(mMockFactory.createSearchSessionAsync(anyString()))
                .thenReturn(Futures.immediateFuture(mMockSession));
        when(mMockSession.setSchemaAsync(any())).thenReturn(Futures.immediateFuture(null));
        // Suppress log spam in tests.
        ShadowLog.stream = null;
        AppSearchBatchResult<String, Void> failedBatchResult =
                new AppSearchBatchResult.Builder<String, Void>()
                        .setFailure(TEST_ID, AppSearchResult.RESULT_INTERNAL_ERROR, "Disk full")
                        .build();
        when(mMockSession.putAsync(any())).thenReturn(Futures.immediateFuture(failedBatchResult));
        var bridge =
                new AuxiliarySearchDonationServiceBridge(/* isBrowsingDataDonationEnabled= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        WebPage page =
                AuxiliarySearchDonationServiceBridge.createHistoryDocument(
                        TEST_ID, TEST_URL, TEST_TITLE, TEST_LAST_VISITED);
        bridge.donateHistory(List.of(page), /* coreAccountInfo= */ null);
        RobolectricUtil.runAllBackgroundAndUi();

        assertTrue(
                "Expected batch failure warning log was not found",
                ShadowLog.getLogs().stream()
                        .anyMatch(
                                item ->
                                        EXPECTED_LOG_TAG.equals(item.tag)
                                                && item.type == Log.WARN
                                                && "Failed to donate documents: 1 failure(s)."
                                                        .equals(item.msg)));
    }

    @Test
    public void testClose() {
        when(mMockFactory.createSearchSessionAsync(anyString()))
                .thenReturn(Futures.immediateFuture(mMockSession));
        when(mMockSession.setSchemaAsync(any())).thenReturn(Futures.immediateFuture(null));
        var bridge =
                new AuxiliarySearchDonationServiceBridge(/* isBrowsingDataDonationEnabled= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        bridge.close();
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mMockSession).close();
    }

    @Test
    public void testClose_unsupportedAndroidVersion() {
        // mMockFactory.createSearchSessionAsync returns null by default.
        var bridge =
                new AuxiliarySearchDonationServiceBridge(/* isBrowsingDataDonationEnabled= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        bridge.close();
        RobolectricUtil.runAllBackgroundAndUi();

        // Should not crash.
    }
}
