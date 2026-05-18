// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.junit.Assert.assertEquals;

import androidx.appsearch.builtintypes.WebPage;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for AuxiliarySearchDonationServiceBridge. */
@RunWith(BaseRobolectricTestRunner.class)
public class AuxiliarySearchDonationServiceBridgeUnitTest {
    private static final String TEST_ID = "123";
    private static final String TEST_URL = "https://example.com";
    private static final String TEST_TITLE = "Example";
    private static final long TEST_LAST_VISITED = 1000L;

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
}
