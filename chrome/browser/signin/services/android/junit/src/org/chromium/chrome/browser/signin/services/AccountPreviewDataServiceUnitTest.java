// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.protocol.SyncEnums.DeviceFormFactor;
import org.chromium.google_apis.gaia.GaiaId;

import java.util.List;

/** Unit tests for {@link AccountPreviewDataService} and {@link AccountPreviewPreference}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(SigninFeatures.ENABLE_ACCOUNT_PREVIEW_PREFERRED_ACCOUNT)
public class AccountPreviewDataServiceUnitTest {
    private static final long NATIVE_SERVICE_PTR = 12345L;
    private static final GaiaId GAIA_ID = new GaiaId("gaia-id-1");
    private static final @DataType int[] DATA_TYPES =
            new int[] {DataType.BOOKMARKS, DataType.PASSWORDS};
    private static final DeviceFormFactor OTHER_DEVICE_FORM_FACTOR =
            DeviceFormFactor.DEVICE_FORM_FACTOR_TABLET;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AccountPreviewDataService.Natives mNativeMock;
    private AccountPreviewDataService mService;

    @Before
    public void setUp() {
        AccountPreviewDataServiceJni.setInstanceForTesting(mNativeMock);
        mService = new AccountPreviewDataService(NATIVE_SERVICE_PTR);
    }

    @Test
    public void testPreferenceGetters() {
        AccountPreviewPreference preference =
                new AccountPreviewPreference(GAIA_ID, DATA_TYPES, OTHER_DEVICE_FORM_FACTOR);
        assertEquals(GAIA_ID, preference.getGaiaId());
        assertEquals(DATA_TYPES.length, preference.getPreferredDataTypes().length);
        assertEquals(DataType.BOOKMARKS, preference.getPreferredDataTypes()[0]);
        assertEquals(DataType.PASSWORDS, preference.getPreferredDataTypes()[1]);
        assertEquals(OTHER_DEVICE_FORM_FACTOR, preference.getOtherDeviceFormFactor());
    }

    @Test
    public void testPreferenceImmutability() {
        @DataType int[] input = new int[] {DataType.BOOKMARKS};
        AccountPreviewPreference preference =
                new AccountPreviewPreference(GAIA_ID, input, OTHER_DEVICE_FORM_FACTOR);

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
        AccountPreviewPreference pref =
                new AccountPreviewPreference(GAIA_ID, DATA_TYPES, OTHER_DEVICE_FORM_FACTOR);
        AccountPreviewPreference prefSameValues =
                new AccountPreviewPreference(
                        new GaiaId("gaia-id-1"),
                        new int[] {DataType.BOOKMARKS, DataType.PASSWORDS},
                        OTHER_DEVICE_FORM_FACTOR);
        AccountPreviewPreference prefDifferentGaia =
                new AccountPreviewPreference(
                        new GaiaId("gaia-id-2"),
                        new int[] {DataType.BOOKMARKS, DataType.PASSWORDS},
                        OTHER_DEVICE_FORM_FACTOR);
        AccountPreviewPreference prefDifferentDataTypes =
                new AccountPreviewPreference(
                        GAIA_ID, new int[] {DataType.BOOKMARKS}, OTHER_DEVICE_FORM_FACTOR);
        AccountPreviewPreference prefDifferentFormFactor =
                new AccountPreviewPreference(
                        GAIA_ID, DATA_TYPES, DeviceFormFactor.DEVICE_FORM_FACTOR_PHONE);

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

        assertFalse(pref.equals(prefDifferentFormFactor));
        assertNotEquals(pref.hashCode(), prefDifferentFormFactor.hashCode());
    }

    @Test
    public void testGetPreferredAccountOrDefault_withMatchingPreference() {
        AccountPreviewPreference preference =
                new AccountPreviewPreference(
                        TestAccounts.ACCOUNT2.getGaiaId(), new int[0], OTHER_DEVICE_FORM_FACTOR);
        doReturn(preference).when(mNativeMock).getPreferredAccountForPromo(NATIVE_SERVICE_PTR);

        List<AccountInfo> accounts = List.of(TestAccounts.ACCOUNT1, TestAccounts.ACCOUNT2);
        AccountInfo selected = mService.getPreferredAccountOrDefault(accounts);
        assertEquals(TestAccounts.ACCOUNT2, selected);
    }

    @Test
    public void testGetPreferredAccountOrDefault_withNoMatchingPreference() {
        doReturn(null).when(mNativeMock).getPreferredAccountForPromo(NATIVE_SERVICE_PTR);

        List<AccountInfo> accounts = List.of(TestAccounts.ACCOUNT1, TestAccounts.ACCOUNT2);
        AccountInfo selected = mService.getPreferredAccountOrDefault(accounts);
        assertEquals(TestAccounts.ACCOUNT1, selected);
    }

    @Test
    public void testGetPreferredAccountOrDefault_preferenceNotFoundInAccounts() {
        AccountPreviewPreference preference =
                new AccountPreviewPreference(
                        new GaiaId("unknown-gaia-id"), new int[0], OTHER_DEVICE_FORM_FACTOR);
        doReturn(preference).when(mNativeMock).getPreferredAccountForPromo(NATIVE_SERVICE_PTR);

        List<AccountInfo> accounts = List.of(TestAccounts.ACCOUNT1, TestAccounts.ACCOUNT2);
        AccountInfo selected = mService.getPreferredAccountOrDefault(accounts);
        assertEquals(TestAccounts.ACCOUNT1, selected);
    }
}
