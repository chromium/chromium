// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.google_apis.gaia.GaiaId;

import java.util.List;

/** Unit tests for {@link ProfileAttributesStorage} and {@link ProfileAttributesEntry}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ProfileAttributesStorageTest {
    private static final String SIGNED_IN_PATH = "signed_in";
    private static final String SIGNED_OUT_PATH = "signed_out";
    private static final String OMITTED_PATH = "omitted";
    private static final GaiaId GAIA_ID = new GaiaId("gaia_id");
    private static final GaiaId OMITTED_GAIA_ID = new GaiaId("omitted_gaia_id");

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ProfileAttributesStorage.Natives mStorageNativesMock;
    @Mock private ProfileAttributesEntry.Natives mEntryNativesMock;
    @Mock private ProfileAttributesStorage.Observer mObserverMock;

    private final ProfileAttributesStorage mStorage = new ProfileAttributesStorage();

    @Before
    public void setUp() {
        ProfileAttributesStorageJni.setInstanceForTesting(mStorageNativesMock);
        ProfileAttributesEntryJni.setInstanceForTesting(mEntryNativesMock);
        when(mStorageNativesMock.getAllProfilesAttributes())
                .thenReturn(
                        List.of(
                                new ProfileAttributesEntry(SIGNED_IN_PATH),
                                new ProfileAttributesEntry(SIGNED_OUT_PATH),
                                new ProfileAttributesEntry(OMITTED_PATH)));
        when(mEntryNativesMock.getGaiaId(SIGNED_IN_PATH)).thenReturn(GAIA_ID);
        when(mEntryNativesMock.getGaiaId(OMITTED_PATH)).thenReturn(OMITTED_GAIA_ID);
        when(mEntryNativesMock.isOmitted(OMITTED_PATH)).thenReturn(true);
    }

    @Test
    public void getAllProfilesAttributes_returnsEntriesWithNativeAttributes() {
        List<ProfileAttributesEntry> entries = mStorage.getAllProfilesAttributes();

        assertEquals(3, entries.size());
        assertEquals(SIGNED_IN_PATH, entries.get(0).getPath());
        assertEquals(GAIA_ID, entries.get(0).getGaiaId());
        assertNull(entries.get(1).getGaiaId());
        assertTrue(entries.get(2).isOmitted());
    }

    @Test
    public void getPrimaryAccountIdsForAllProfiles_skipsSignedOutAndOmittedProfiles() {
        assertEquals(
                List.of(new CoreAccountId(GAIA_ID)), mStorage.getPrimaryAccountIdsForAllProfiles());
    }

    @Test
    public void isPrimaryAccountForAnyProfile_checksSignedInProfiles() {
        assertTrue(mStorage.isPrimaryAccountForAnyProfile(new CoreAccountId(GAIA_ID)));
        assertFalse(mStorage.isPrimaryAccountForAnyProfile(new CoreAccountId(OMITTED_GAIA_ID)));
        assertFalse(mStorage.isPrimaryAccountForAnyProfile(new CoreAccountId(new GaiaId("other"))));
    }

    @Test
    public void nativeEvents_notifyObservers() {
        mStorage.addObserver(mObserverMock);

        mStorage.onProfileAdded("added");
        mStorage.onProfileWasRemoved("removed");
        mStorage.onProfileAuthInfoChanged("auth");
        mStorage.onProfileIsOmittedChanged("omitted");

        verify(mObserverMock).onProfileAdded("added");
        verify(mObserverMock).onProfileWasRemoved("removed");
        verify(mObserverMock).onProfileAuthInfoChanged("auth");
        verify(mObserverMock).onProfileIsOmittedChanged("omitted");
    }

    @Test
    public void nativeEvents_doNotNotifyRemovedObservers() {
        mStorage.addObserver(mObserverMock);
        mStorage.removeObserver(mObserverMock);

        mStorage.onProfileAuthInfoChanged(SIGNED_IN_PATH);

        verify(mObserverMock, never()).onProfileAuthInfoChanged(SIGNED_IN_PATH);
    }
}
