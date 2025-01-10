// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;

import androidx.appsearch.app.SetSchemaResponse;
import androidx.appsearch.app.SetSchemaResponse.MigrationFailure;
import androidx.appsearch.builtintypes.WebPage;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Unit tests for AuxiliarySearchDonor. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
public class AuxiliarySearchDonorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private MigrationFailure mMigrationFailure;
    @Mock private AuxiliarySearchHooks mHooks;

    private AuxiliarySearchDonor mAuxiliarySearchDonor;

    @Before
    public void setUp() {
        when(mHooks.isEnabled()).thenReturn(true);
        when(mHooks.isSettingDefaultEnabledByOs()).thenReturn(true);
        AuxiliarySearchControllerFactory.getInstance().setHooksForTesting(mHooks);
        assertTrue(AuxiliarySearchControllerFactory.getInstance().isSettingDefaultEnabledByOs());
        assertTrue(AuxiliarySearchUtils.isShareTabsWithOsEnabled());

        AuxiliarySearchDonor.setSkipInitializationForTesting(true);
        mAuxiliarySearchDonor = AuxiliarySearchDonor.getInstance();
    }

    @Test
    @SmallTest
    public void testDefaultTtlIsNotZero() {
        assertNotEquals(0L, mAuxiliarySearchDonor.getDocumentTtlMs());
        assertEquals(
                ((long) AuxiliarySearchUtils.DEFAULT_TTL_HOURS) * 60 * 60 * 1000,
                mAuxiliarySearchDonor.getDocumentTtlMs());
    }

    @Test
    @SmallTest
    @EnableFeatures("AndroidAppIntegration:content_ttl_hours/0")
    public void testConfiguredTtlCannotBeZero() {
        assertNotEquals(0L, mAuxiliarySearchDonor.getDocumentTtlMs());
        assertEquals(
                ((long) AuxiliarySearchUtils.DEFAULT_TTL_HOURS) * 60 * 60 * 1000,
                mAuxiliarySearchDonor.getDocumentTtlMs());
    }

    @Test
    @SmallTest
    public void testBuildDocument() {
        int id = 10;
        String url = "Url";
        String title = "Title";
        long lastAccessTimeStamp = 100;
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Config.RGB_565);
        String documentId = "Tab-10";
        assertEquals(documentId, AuxiliarySearchDonor.getDocumentId(id));

        WebPage webPage =
                mAuxiliarySearchDonor.buildDocument(id, url, title, lastAccessTimeStamp, bitmap);

        assertEquals(documentId, webPage.getId());
        assertEquals(url, webPage.getUrl());
        assertEquals(title, webPage.getName());
        assertEquals(lastAccessTimeStamp, webPage.getCreationTimestampMillis());
        assertEquals(mAuxiliarySearchDonor.getDocumentTtlMs(), webPage.getDocumentTtlMillis());
        assertTrue(
                Arrays.equals(
                        AuxiliarySearchUtils.bitmapToBytes(bitmap),
                        webPage.getFavicon().getBytes()));
    }

    @Test
    @SmallTest
    public void testOnSetSchemaResponseAvailable() {
        List<MigrationFailure> migrationFailures = new ArrayList<MigrationFailure>();
        migrationFailures.add(mMigrationFailure);
        SetSchemaResponse setSchemaResponse =
                new SetSchemaResponse.Builder().addMigrationFailures(migrationFailures).build();

        List<WebPage> pendingDocs = new ArrayList<WebPage>();
        WebPage webPage = new WebPage.Builder("namespace", "Id1").setUrl("Url1").build();
        pendingDocs.add(webPage);
        mAuxiliarySearchDonor.setPendingDocumentsForTesting(pendingDocs);
        mAuxiliarySearchDonor.resetSchemaSetForTesting();

        // Verifies that the pending donation isn't executed if setSchema is failed.
        mAuxiliarySearchDonor.onSetSchemaResponseAvailable(setSchemaResponse);
        assertNotNull(mAuxiliarySearchDonor.getPendingDocumentsForTesting());
        assertFalse(mAuxiliarySearchDonor.getIsSchemaSetForTesting());

        migrationFailures.clear();
        setSchemaResponse =
                new SetSchemaResponse.Builder().addMigrationFailures(migrationFailures).build();

        // Verifies that the pending donation will be executed if setSchema succeeds.
        mAuxiliarySearchDonor.onSetSchemaResponseAvailable(setSchemaResponse);
        assertNull(mAuxiliarySearchDonor.getPendingDocumentsForTesting());
        assertTrue(mAuxiliarySearchDonor.getIsSchemaSetForTesting());
    }

    @Test
    @SmallTest
    public void testSharedPreferenceKeyIsUpdated() {
        SetSchemaResponse setSchemaResponse = new SetSchemaResponse.Builder().build();
        assertTrue(setSchemaResponse.getMigrationFailures().isEmpty());

        SharedPreferencesManager chromeSharedPreferences = ChromeSharedPreferences.getInstance();
        assertFalse(mAuxiliarySearchDonor.getIsSchemaSetForTesting());
        assertFalse(
                chromeSharedPreferences.readBoolean(
                        ChromePreferenceKeys.AUXILIARY_SEARCH_IS_SCHEMA_SET, false));

        // Verifies that ChromePreferenceKeys.AUXILIARY_SEARCH_IS_SCHEMA_SET is set to true after
        // the schema is set successful.
        mAuxiliarySearchDonor.onSetSchemaResponseAvailable(setSchemaResponse);
        assertTrue(mAuxiliarySearchDonor.getIsSchemaSetForTesting());
        assertTrue(
                chromeSharedPreferences.readBoolean(
                        ChromePreferenceKeys.AUXILIARY_SEARCH_IS_SCHEMA_SET, false));

        chromeSharedPreferences.removeKey(ChromePreferenceKeys.AUXILIARY_SEARCH_IS_SCHEMA_SET);
    }

    @Test
    @SmallTest
    public void testDoNotSetSchemaAgain() {
        mAuxiliarySearchDonor.resetSchemaSetForTesting();
        SharedPreferencesManager chromeSharedPreferences = ChromeSharedPreferences.getInstance();
        chromeSharedPreferences.writeBoolean(
                ChromePreferenceKeys.AUXILIARY_SEARCH_IS_SCHEMA_SET, true);
        assertFalse(mAuxiliarySearchDonor.getIsSchemaSetForTesting());

        // Verifies not to set the schema again if it has been set.
        assertFalse(mAuxiliarySearchDonor.maySetSchema());
        assertTrue(mAuxiliarySearchDonor.getIsSchemaSetForTesting());

        chromeSharedPreferences.removeKey(ChromePreferenceKeys.AUXILIARY_SEARCH_IS_SCHEMA_SET);
    }

    @Test
    @SmallTest
    public void testOnConfigChanged() {
        Callback<Boolean> callback = Mockito.mock(Callback.class);
        assertTrue(mAuxiliarySearchDonor.getSharedTabsWithOsStateForTesting());
        assertTrue(AuxiliarySearchUtils.isShareTabsWithOsEnabled());

        mAuxiliarySearchDonor.onConfigChanged(false, callback);
        assertFalse(mAuxiliarySearchDonor.getSharedTabsWithOsStateForTesting());
        assertFalse(AuxiliarySearchUtils.isShareTabsWithOsEnabled());

        mAuxiliarySearchDonor.onConfigChanged(true, callback);
        assertTrue(mAuxiliarySearchDonor.getSharedTabsWithOsStateForTesting());
        assertTrue(AuxiliarySearchUtils.isShareTabsWithOsEnabled());
    }
}
