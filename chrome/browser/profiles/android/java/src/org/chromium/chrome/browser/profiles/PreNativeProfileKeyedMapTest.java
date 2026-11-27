// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link PreNativeProfileKeyedMap}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PreNativeProfileKeyedMapTest {
    private static final String DEFAULT_PROFILE = "Default";
    private static final String OTHER_PROFILE = "Profile 1";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;
    @Mock private Profile mOtherProfile;
    @Mock private Destroyable mDestroyable;

    private final List<Object> mDestroyedObjects = new ArrayList<>();
    private final List<@Nullable Profile> mFactoryProfiles = new ArrayList<>();

    private PreNativeProfileKeyedMap<Object> mMap;

    @Before
    public void setUp() {
        ProfileManager.resetForTesting();
        setLastUsedProfile(DEFAULT_PROFILE);
        when(mProfile.getBaseName()).thenReturn(DEFAULT_PROFILE);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mIncognitoProfile.getBaseName()).thenReturn(DEFAULT_PROFILE);
        when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);
        when(mIncognitoProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mOtherProfile.getBaseName()).thenReturn(OTHER_PROFILE);
        when(mOtherProfile.getOriginalProfile()).thenReturn(mOtherProfile);
        mMap = new PreNativeProfileKeyedMap<>(this::createObject, mDestroyedObjects::add);
    }

    @After
    public void tearDown() {
        mMap.destroy();
        ProfileManager.resetForTesting();
    }

    @Test
    public void testPreNativeInstanceMovesToLastUsedProfile() {
        testPreNativeInstanceImpl(/* lastUsedProfile= */ DEFAULT_PROFILE, /* expectMoved= */ true);
    }

    @Test
    public void testPreNativeInstanceIsDestroyedForOtherProfile() {
        testPreNativeInstanceImpl(/* lastUsedProfile= */ OTHER_PROFILE, /* expectMoved= */ false);
    }

    @Test
    public void testPreNativeInstanceIsDestroyedWithoutRecordedLastUsedProfile() {
        testPreNativeInstanceImpl(/* lastUsedProfile= */ null, /* expectMoved= */ false);
    }

    @Test
    public void testPreNativeInstanceKeepsLastUsedProfileFromCreation() {
        setLastUsedProfile(OTHER_PROFILE);
        Object preNativeInstance = mMap.getForProfile(null);
        setLastUsedProfile(DEFAULT_PROFILE);

        ProfileManager.onProfileAdded(mProfile);

        assertEquals(List.of(preNativeInstance), mDestroyedObjects);
    }

    @Test
    public void testPreNativeInstanceIsCreatedOnce() {
        Object preNativeInstance = mMap.getForProfile(null);

        assertSame(preNativeInstance, mMap.getForProfile(null));
        assertEquals(1, mFactoryProfiles.size());
        assertNull(mFactoryProfiles.get(0));
    }

    @Test
    public void testPreNativeInstanceIgnoresOffTheRecordProfile() {
        Object preNativeInstance = mMap.getForProfile(null);

        ProfileManager.onProfileAdded(mIncognitoProfile);
        assertTrue(mDestroyedObjects.isEmpty());
        ProfileManager.onProfileAdded(mProfile);

        assertSame(preNativeInstance, mMap.getForProfile(mProfile));
        assertTrue(mDestroyedObjects.isEmpty());
    }

    @Test
    public void testPreNativeInstanceMovesWhenEarlierObserverRequestsProfile() {
        testEarlierObserverImpl(/* addedProfile= */ mProfile, /* expectMoved= */ true);
    }

    @Test
    public void testPreNativeInstanceIsNotMovedToOtherRequestedProfile() {
        testEarlierObserverImpl(/* addedProfile= */ mOtherProfile, /* expectMoved= */ false);
    }

    @Test
    public void testMovedPreNativeInstanceIsSharedWithOffTheRecordProfile() {
        recreateMap(ProfileKeyedMap.ProfileSelection.REDIRECTED_TO_ORIGINAL);
        Object preNativeInstance = mMap.getForProfile(null);

        ProfileManager.onProfileAdded(mProfile);

        assertSame(preNativeInstance, mMap.getForProfile(mIncognitoProfile));
        assertTrue(mDestroyedObjects.isEmpty());
    }

    @Test
    public void testPreNativeInstanceIsSharedWithRequestedOffTheRecordProfile() {
        testOffTheRecordProfileRequestImpl(
                ProfileKeyedMap.ProfileSelection.REDIRECTED_TO_ORIGINAL, /* expectShared= */ true);
    }

    @Test
    public void testPreNativeInstanceMovesWhenOffTheRecordProfileIsRequested() {
        testOffTheRecordProfileRequestImpl(
                ProfileKeyedMap.ProfileSelection.OWN_INSTANCE, /* expectShared= */ false);
    }

    @Test
    public void testObjectIsCreatedLazilyWithoutPreNativeInstance() {
        ProfileManager.onProfileAdded(mProfile);
        assertTrue(mFactoryProfiles.isEmpty());

        Object object = mMap.getForProfile(mProfile);

        assertSame(object, mMap.getForProfile(mProfile));
        assertEquals(List.of(mProfile), mFactoryProfiles);
    }

    @Test
    public void testMovedPreNativeInstanceIsDestroyedWithProfile() {
        Object preNativeInstance = mMap.getForProfile(null);
        ProfileManager.onProfileAdded(mProfile);
        assertTrue(mDestroyedObjects.isEmpty());

        ProfileManager.onProfileDestroyed(mProfile);

        assertEquals(List.of(preNativeInstance), mDestroyedObjects);
    }

    @Test
    public void testLaterProfileIsIgnoredOncePreNativeInstanceMoved() {
        Object preNativeInstance = mMap.getForProfile(null);
        ProfileManager.onProfileAdded(mProfile);

        ProfileManager.onProfileAdded(mOtherProfile);

        assertTrue(mDestroyedObjects.isEmpty());
        assertSame(preNativeInstance, mMap.getForProfile(mProfile));
        // The map stopped observing the ProfileManager.
        verifyNoInteractions(mOtherProfile);
    }

    @Test
    public void testDestroyDestroysPreNativeInstanceAndProfileObjects() {
        Object preNativeInstance = mMap.getForProfile(null);
        Object otherObject = mMap.getForProfile(mOtherProfile);

        mMap.destroy();
        ProfileManager.onProfileAdded(mProfile);

        assertEquals(2, mDestroyedObjects.size());
        assertTrue(mDestroyedObjects.contains(preNativeInstance));
        assertTrue(mDestroyedObjects.contains(otherObject));
        // The map stopped observing the ProfileManager.
        verifyNoInteractions(mProfile);
    }

    @Test
    public void testMapOfDestroyables() {
        PreNativeProfileKeyedMap<Destroyable> map =
                PreNativeProfileKeyedMap.createMapOfDestroyablesForTesting(profile -> mDestroyable);
        map.getForProfile(null);

        map.destroy();

        verify(mDestroyable).destroy();
    }

    @Test
    public void testNullProfileIsNotAllowedOnceFirstProfileIsAdded() {
        ProfileManager.onProfileAdded(mProfile);

        assertThrows(IllegalStateException.class, () -> mMap.getForProfile(null));
    }

    private void testPreNativeInstanceImpl(@Nullable String lastUsedProfile, boolean expectMoved) {
        setLastUsedProfile(lastUsedProfile);
        Object preNativeInstance = mMap.getForProfile(null);

        ProfileManager.onProfileAdded(mProfile);

        assertEquals(expectMoved ? List.of() : List.of(preNativeInstance), mDestroyedObjects);
        Object profileObject = mMap.getForProfile(mProfile);
        if (expectMoved) {
            assertSame(preNativeInstance, profileObject);
        } else {
            assertNotSame(preNativeInstance, profileObject);
        }
    }

    private void testOffTheRecordProfileRequestImpl(
            @ProfileKeyedMap.ProfileSelection int profileSelection, boolean expectShared) {
        recreateMap(profileSelection);
        Object preNativeInstance = mMap.getForProfile(null);

        Object incognitoObject = mMap.getForProfile(mIncognitoProfile);
        ProfileManager.onProfileAdded(mProfile);

        if (expectShared) {
            assertSame(preNativeInstance, incognitoObject);
        } else {
            assertNotSame(preNativeInstance, incognitoObject);
        }
        assertSame(preNativeInstance, mMap.getForProfile(mProfile));
        assertTrue(mDestroyedObjects.isEmpty());
    }

    private void testEarlierObserverImpl(Profile addedProfile, boolean expectMoved) {
        List<Object> requestedObjects = new ArrayList<>();
        ProfileManager.addObserver(
                new ProfileManager.Observer() {
                    @Override
                    public void onProfileAdded(Profile profile) {
                        requestedObjects.add(mMap.getForProfile(profile));
                    }

                    @Override
                    public void onProfileDestroyed(Profile profile) {}
                });
        Object preNativeInstance = mMap.getForProfile(null);

        ProfileManager.onProfileAdded(addedProfile);

        assertEquals(1, requestedObjects.size());
        if (expectMoved) {
            assertSame(preNativeInstance, requestedObjects.get(0));
            assertTrue(mDestroyedObjects.isEmpty());
        } else {
            assertNotSame(preNativeInstance, requestedObjects.get(0));
            assertEquals(List.of(preNativeInstance), mDestroyedObjects);
        }
    }

    private void recreateMap(@ProfileKeyedMap.ProfileSelection int profileSelection) {
        mMap.destroy();
        mMap =
                new PreNativeProfileKeyedMap<>(
                        profileSelection, this::createObject, mDestroyedObjects::add);
    }

    private Object createObject(@Nullable Profile profile) {
        mFactoryProfiles.add(profile);
        return new Object();
    }

    private void setLastUsedProfile(@Nullable String baseName) {
        ChromeSharedPreferences.getInstance()
                .writeString(ChromePreferenceKeys.PROFILES_LAST_USED_PROFILE_BASE_NAME, baseName);
    }
}
