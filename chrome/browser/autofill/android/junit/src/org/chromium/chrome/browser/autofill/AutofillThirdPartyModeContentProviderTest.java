// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.autofill.AutofillThirdPartyModeContentProvider.AUTOFILL_THIRD_PARTY_MODE_COLUMN;
import static org.chromium.chrome.browser.autofill.AutofillThirdPartyModeContentProvider.createContentUri;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.AUTOFILL_THIRD_PARTY_MODE_STATE;

import android.content.Context;
import android.database.Cursor;
import android.net.Uri;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;

/** Unit tests for {@link AutofillThirdPartyModeContentProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
public class AutofillThirdPartyModeContentProviderTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private AutofillThirdPartyModeContentProvider mProvider;
    @Mock private SharedPreferencesManager mPrefManager;

    @Mock private Context mContext;

    @Before
    public void setUp() {
        mProvider = new AutofillThirdPartyModeContentProvider();
        mProvider.setPerfManagerForTesting(mPrefManager);

        ContextUtils.initApplicationContextForTests(mContext);
        // Mock the package name for generating the URI
        when(mContext.getPackageName()).thenReturn("com.example.app");
    }

    @Test
    public void testQueryThirdPartyModeOff() {
        when(mPrefManager.readBoolean(AUTOFILL_THIRD_PARTY_MODE_STATE, false)).thenReturn(false);
        Uri uri = createContentUri();

        Cursor cursor = mProvider.query(uri, null, null, null, null);
        assertNotNull("Cursor should not be null", cursor);
        assertTrue("Cursor should have results", cursor.moveToFirst());

        int nameIndex = cursor.getColumnIndex(AUTOFILL_THIRD_PARTY_MODE_COLUMN);

        assertNotEquals("Column index should be valid", -1, nameIndex);
        assertEquals("Third party mode should be off", 0, cursor.getInt(nameIndex));
    }

    @Test
    public void testQueryThirdPartyModeOn() {
        when(mPrefManager.readBoolean(AUTOFILL_THIRD_PARTY_MODE_STATE, false)).thenReturn(true);
        Uri uri = createContentUri();

        Cursor cursor = mProvider.query(uri, null, null, null, null);
        assertNotNull("Cursor should not be null", cursor);
        assertTrue("Cursor should have results", cursor.moveToFirst());

        int nameIndex = cursor.getColumnIndex(AUTOFILL_THIRD_PARTY_MODE_COLUMN);

        assertNotEquals("Column index should be valid", -1, nameIndex);
        assertEquals("Third party mode should be off", 1, cursor.getInt(nameIndex));
    }

    @Test
    public void testQueryWrongUrl() {
        when(mPrefManager.readBoolean(AUTOFILL_THIRD_PARTY_MODE_STATE, false)).thenReturn(false);
        Uri uri = Uri.parse("https://google.com");

        Cursor cursor = mProvider.query(uri, null, null, null, null);
        assertNull("Cursor should be null", cursor);
    }
}
