// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static android.content.ContentResolver.SCHEME_CONTENT;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import static org.chromium.chrome.browser.autofill.AutofillThirdPartyModeContentProvider.AUTOFILL_THIRD_PARTY_MODE_ACTIONS_URI_PATH;
import static org.chromium.chrome.browser.autofill.AutofillThirdPartyModeContentProvider.AUTOFILL_THIRD_PARTY_MODE_COLUMN;
import static org.chromium.chrome.browser.autofill.AutofillThirdPartyModeContentProvider.createContentUri;

import android.database.Cursor;
import android.net.Uri;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/** Integration tests for the {@link AutofillThirdPartyModeContentProvider}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AutofillThirdPartyModeContentProviderIntegrationTest {
    @Test
    @MediumTest
    public void testQueryThirdPartyModeOff() {
        AutofillClientProviderUtils.setThirdPartyModePref(false);

        Cursor cursor =
                ApplicationProvider.getApplicationContext()
                        .getContentResolver()
                        .query(
                                createContentUri(),
                                new String[] {AUTOFILL_THIRD_PARTY_MODE_COLUMN},
                                null,
                                null,
                                null);

        cursor.moveToFirst();
        int index = cursor.getColumnIndex(AUTOFILL_THIRD_PARTY_MODE_COLUMN);

        // 0 means that the third party mode was off.
        assertEquals(0, cursor.getInt(index));
    }

    @Test
    @MediumTest
    public void testQueryThirdPartyModeOn() {
        AutofillClientProviderUtils.setThirdPartyModePref(true);

        Cursor cursor =
                ApplicationProvider.getApplicationContext()
                        .getContentResolver()
                        .query(
                                createContentUri(),
                                new String[] {AUTOFILL_THIRD_PARTY_MODE_COLUMN},
                                null,
                                null,
                                null);

        cursor.moveToFirst();
        int index = cursor.getColumnIndex(AUTOFILL_THIRD_PARTY_MODE_COLUMN);

        // 1 means that the third party mode was off.
        assertEquals(1, cursor.getInt(index));
    }

    @Test
    @MediumTest
    public void testQueryWrongUrl() {
        AutofillClientProviderUtils.setThirdPartyModePref(false);
        Uri uri =
                new Uri.Builder()
                        .scheme(SCHEME_CONTENT)
                        .authority(
                                ApplicationProvider.getApplicationContext().getPackageName()
                                        + ".NonExistentContentProvider")
                        .path(AUTOFILL_THIRD_PARTY_MODE_ACTIONS_URI_PATH)
                        .build();

        Cursor cursor =
                ApplicationProvider.getApplicationContext()
                        .getContentResolver()
                        .query(uri, null, null, null, null);
        assertNull("Cursor should be null", cursor);
    }
}
