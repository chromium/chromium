// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.sync.DataType;
import org.chromium.google_apis.gaia.GaiaId;

/** Unit tests for {@link AccountPreviewDataService} and {@link AccountPreviewPreference}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AccountPreviewDataServiceUnitTest {
    private static final long NATIVE_SERVICE_PTR = 12345L;
    private static final GaiaId GAIA_ID = new GaiaId("gaia-id-1");
    private static final @DataType int[] DATA_TYPES =
            new int[] {DataType.BOOKMARKS, DataType.PASSWORDS};

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AccountPreviewDataService.Natives mNativeMock;

    @Before
    public void setUp() {
        AccountPreviewDataServiceJni.setInstanceForTesting(mNativeMock);
    }

    @Test
    public void testPreferenceGetters() {
        AccountPreviewPreference preference = new AccountPreviewPreference(GAIA_ID, DATA_TYPES);
        assertEquals(GAIA_ID, preference.getGaiaId());
        assertEquals(DATA_TYPES.length, preference.getPreferredDataTypes().length);
        assertEquals(DataType.BOOKMARKS, preference.getPreferredDataTypes()[0]);
        assertEquals(DataType.PASSWORDS, preference.getPreferredDataTypes()[1]);
    }

    @Test
    public void testPreferenceImmutability() {
        @DataType int[] input = new int[] {DataType.BOOKMARKS};
        AccountPreviewPreference preference = new AccountPreviewPreference(GAIA_ID, input);

        // Mutate input array
        input[0] = DataType.PASSWORDS;
        assertEquals(DataType.BOOKMARKS, preference.getPreferredDataTypes()[0]);

        // Mutate returned array from getter
        @DataType int[] returned = preference.getPreferredDataTypes();
        returned[0] = DataType.PASSWORDS;
        assertEquals(DataType.BOOKMARKS, preference.getPreferredDataTypes()[0]);
    }

    @Test
    public void testPreferenceEqualsAndHashCode() {
        AccountPreviewPreference pref = new AccountPreviewPreference(GAIA_ID, DATA_TYPES);
        AccountPreviewPreference prefSameValues =
                new AccountPreviewPreference(
                        new GaiaId("gaia-id-1"),
                        new int[] {DataType.BOOKMARKS, DataType.PASSWORDS});
        AccountPreviewPreference prefDifferentGaia =
                new AccountPreviewPreference(
                        new GaiaId("gaia-id-2"),
                        new int[] {DataType.BOOKMARKS, DataType.PASSWORDS});
        AccountPreviewPreference prefDifferentDataTypes =
                new AccountPreviewPreference(GAIA_ID, new int[] {DataType.BOOKMARKS});

        // Reflexive
        assertTrue(pref.equals(pref));

        // Symmetric and equal
        assertTrue(pref.equals(prefSameValues));
        assertTrue(prefSameValues.equals(pref));
        assertEquals(pref.hashCode(), prefSameValues.hashCode());

        // Unequal fields
        assertFalse(pref.equals(prefDifferentGaia));
        assertNotEquals(pref.hashCode(), prefDifferentGaia.hashCode());

        assertFalse(pref.equals(prefDifferentDataTypes));
        assertNotEquals(pref.hashCode(), prefDifferentDataTypes.hashCode());
    }
}
