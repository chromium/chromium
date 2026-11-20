// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.AdditionalMatchers.aryEq;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;

import java.nio.charset.StandardCharsets;
import java.util.concurrent.atomic.AtomicReference;

/** Tests for {@link ProfileTokenUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ProfileTokenUtilsTest {
    // The token of a "Default" Profile (see ProfileResolverTest), and its encoded form.
    private static final String LEGACY_TOKEN = "\n\u0007Default";
    private static final byte[] TOKEN_BYTES = LEGACY_TOKEN.getBytes(StandardCharsets.UTF_8);
    private static final String ENCODED_TOKEN = "CgdEZWZhdWx0";
    private static final byte[] OTHER_TOKEN_BYTES =
            "\n\tProfile 1".getBytes(StandardCharsets.UTF_8);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ProfileResolver.Natives mProfileResolverNatives;
    @Mock private Profile mProfile;
    @Mock private Profile mOtherProfile;
    @Captor private ArgumentCaptor<Callback<Profile>> mCallbackArgumentCaptor;

    @Before
    public void setUp() {
        ProfileResolverJni.setInstanceForTesting(mProfileResolverNatives);
        when(mProfileResolverNatives.tokenizeProfileToBytes(mProfile)).thenReturn(TOKEN_BYTES);
        when(mProfileResolverNatives.tokenizeProfileToBytes(mOtherProfile))
                .thenReturn(OTHER_TOKEN_BYTES);
    }

    private static Intent createIntent() {
        Intent intent = new Intent();
        intent.setClass(ContextUtils.getApplicationContext(), TestActivity.class);
        return intent;
    }

    @Test
    public void testHasProfileToken() {
        Intent intent = createIntent();
        assertFalse(ProfileTokenUtils.hasProfileToken(intent));

        intent.putExtra(ProfileTokenUtils.PROFILE_TOKEN_EXTRA, ENCODED_TOKEN);
        assertFalse(ProfileTokenUtils.hasProfileToken(intent));

        IntentUtils.addTrustedIntentExtras(intent);
        assertTrue(ProfileTokenUtils.hasProfileToken(intent));
    }

    @Test
    public void testHasProfileToken_LegacyToken() {
        Intent intent = createIntent();
        IntentUtils.addTrustedIntentExtras(intent);
        intent.putExtra(ProfileTokenUtils.LEGACY_PROFILE_TOKEN_EXTRA, LEGACY_TOKEN);
        assertTrue(ProfileTokenUtils.hasProfileToken(intent));
    }

    @Test
    public void testAddProfileToIntent() {
        Intent intent = createIntent();
        assertFalse(IntentUtils.isTrustedIntentFromSelf(intent));

        ProfileTokenUtils.addProfileToIntent(mProfile, intent);
        assertTrue(ProfileTokenUtils.hasProfileToken(intent));
        assertTrue(IntentUtils.isTrustedIntentFromSelf(intent));
        // Encoded tokens are persisted, so the encoding must not change.
        assertEquals(ENCODED_TOKEN, intent.getStringExtra(ProfileTokenUtils.PROFILE_TOKEN_EXTRA));
        assertEquals(ENCODED_TOKEN, intent.getIdentifier());
        assertFalse(intent.hasExtra(ProfileTokenUtils.LEGACY_PROFILE_TOKEN_EXTRA));
    }

    @Test(expected = AssertionError.class)
    public void testRetrieveProfileFromIntent_UntrustedIntent() {
        Intent intent = createIntent();
        intent.putExtra(ProfileTokenUtils.PROFILE_TOKEN_EXTRA, ENCODED_TOKEN);
        assertFalse(ProfileTokenUtils.hasProfileToken(intent));

        ProfileTokenUtils.retrieveProfileFromIntent(intent, profile -> {});
    }

    @Test
    public void testRetrieveProfileFromIntent_TrustedIntent_NoToken() {
        Intent intent = createIntent();
        IntentUtils.addTrustedIntentExtras(intent);
        assertFalse(ProfileTokenUtils.hasProfileToken(intent));

        AtomicReference<Profile> retrievedProfile = new AtomicReference<>(mProfile);
        ProfileTokenUtils.retrieveProfileFromIntent(intent, retrievedProfile::set);
        assertNull(retrievedProfile.get());
        verify(mProfileResolverNatives, never()).resolveProfileFromBytes(any(), any());
        verify(mProfileResolverNatives, never()).resolveProfile(any(), any());
    }

    @Test
    public void testRetrieveProfileFromIntent_InvalidProfile() {
        Intent intent = createIntent();
        ProfileTokenUtils.addProfileToIntent(mProfile, intent);

        AtomicReference<Profile> retrievedProfile = new AtomicReference<>(mProfile);
        ProfileTokenUtils.retrieveProfileFromIntent(intent, retrievedProfile::set);
        verify(mProfileResolverNatives)
                .resolveProfileFromBytes(aryEq(TOKEN_BYTES), mCallbackArgumentCaptor.capture());
        mCallbackArgumentCaptor.getValue().onResult(null);
        assertNull(retrievedProfile.get());
    }

    @Test
    public void testRetrieveProfileFromIntent_ValidProfile() {
        Intent intent = createIntent();
        ProfileTokenUtils.addProfileToIntent(mProfile, intent);

        AtomicReference<Profile> retrievedProfile = new AtomicReference<>();
        ProfileTokenUtils.retrieveProfileFromIntent(intent, retrievedProfile::set);
        // The encoded token is decoded back to the token bytes.
        verify(mProfileResolverNatives)
                .resolveProfileFromBytes(aryEq(TOKEN_BYTES), mCallbackArgumentCaptor.capture());
        mCallbackArgumentCaptor.getValue().onResult(mProfile);
        assertEquals(mProfile, retrievedProfile.get());
    }

    @Test
    public void testRetrieveProfileFromIntent_NonUtf8Bytes() {
        // Protobuf wire bytes are not generally valid UTF-8 (for example, varint lengths >= 128).
        byte[] nonUtf8Token = new byte[] {0x0a, (byte) 0x80, 0x01, 0x00, (byte) 0xff};
        when(mProfileResolverNatives.tokenizeProfileToBytes(mProfile)).thenReturn(nonUtf8Token);

        Intent intent = createIntent();
        ProfileTokenUtils.addProfileToIntent(mProfile, intent);

        AtomicReference<Profile> retrievedProfile = new AtomicReference<>();
        ProfileTokenUtils.retrieveProfileFromIntent(intent, retrievedProfile::set);
        verify(mProfileResolverNatives)
                .resolveProfileFromBytes(aryEq(nonUtf8Token), mCallbackArgumentCaptor.capture());
        mCallbackArgumentCaptor.getValue().onResult(mProfile);
        assertEquals(mProfile, retrievedProfile.get());
    }

    @Test
    public void testRetrieveProfileFromIntent_LegacyToken() {
        Intent intent = createIntent();
        IntentUtils.addTrustedIntentExtras(intent);
        intent.putExtra(ProfileTokenUtils.LEGACY_PROFILE_TOKEN_EXTRA, LEGACY_TOKEN);

        AtomicReference<Profile> retrievedProfile = new AtomicReference<>();
        ProfileTokenUtils.retrieveProfileFromIntent(intent, retrievedProfile::set);
        verify(mProfileResolverNatives)
                .resolveProfile(eq(LEGACY_TOKEN), mCallbackArgumentCaptor.capture());
        mCallbackArgumentCaptor.getValue().onResult(mProfile);
        assertEquals(mProfile, retrievedProfile.get());
    }

    @Test
    public void testRetrieveProfileFromIntent_MalformedToken() {
        Intent intent = createIntent();
        IntentUtils.addTrustedIntentExtras(intent);
        intent.putExtra(ProfileTokenUtils.PROFILE_TOKEN_EXTRA, "not base64!");

        AtomicReference<Profile> retrievedProfile = new AtomicReference<>(mProfile);
        ProfileTokenUtils.retrieveProfileFromIntent(intent, retrievedProfile::set);
        assertNull(retrievedProfile.get());
        verify(mProfileResolverNatives, never()).resolveProfileFromBytes(any(), any());
    }

    @Test
    public void testAddProfileToIntent_ProfileScopedIdentity() {
        Intent intent = createIntent();
        ProfileTokenUtils.addProfileToIntent(mProfile, intent);
        Intent sameIntent = createIntent();
        ProfileTokenUtils.addProfileToIntent(mProfile, sameIntent);
        Intent otherProfileIntent = createIntent();
        ProfileTokenUtils.addProfileToIntent(mOtherProfile, otherProfileIntent);

        assertEquals(ENCODED_TOKEN, intent.getIdentifier());
        assertTrue(intent.filterEquals(sameIntent));
        assertFalse(intent.filterEquals(otherProfileIntent));
        assertFalse(intent.filterEquals(createIntent()));
    }

    @Test(expected = AssertionError.class)
    public void testAddProfileToIntent_ExistingIdentifierAsserts() {
        Intent intent = createIntent();
        intent.setIdentifier("custom_identifier");
        ProfileTokenUtils.addProfileToIntent(mProfile, intent);
    }

    @Test(expected = AssertionError.class)
    public void testAddProfileToIntent_CalledTwiceAsserts() {
        Intent intent = createIntent();
        ProfileTokenUtils.addProfileToIntent(mProfile, intent);
        ProfileTokenUtils.addProfileToIntent(mOtherProfile, intent);
    }

    @Test
    public void testAddProfileToIntent_PendingIntent() {
        Context context = ContextUtils.getApplicationContext();
        Intent intent = createIntent();
        ProfileTokenUtils.addProfileToIntent(mProfile, intent);
        Intent sameIntent = createIntent();
        ProfileTokenUtils.addProfileToIntent(mProfile, sameIntent);
        Intent otherProfileIntent = createIntent();
        ProfileTokenUtils.addProfileToIntent(mOtherProfile, otherProfileIntent);

        PendingIntent pendingIntent =
                PendingIntent.getActivity(context, /* requestCode= */ 0, intent, 0);
        PendingIntent samePendingIntent =
                PendingIntent.getActivity(context, /* requestCode= */ 0, sameIntent, 0);
        PendingIntent otherRequestCodePendingIntent =
                PendingIntent.getActivity(context, /* requestCode= */ 1, sameIntent, 0);
        PendingIntent otherProfilePendingIntent =
                PendingIntent.getActivity(context, /* requestCode= */ 0, otherProfileIntent, 0);

        assertEquals(pendingIntent, samePendingIntent);
        assertNotEquals(pendingIntent, otherRequestCodePendingIntent);
        assertNotEquals(pendingIntent, otherProfilePendingIntent);
    }

    @Test
    public void testGetNotificationTag() {
        assertEquals(
                "prefix:" + ENCODED_TOKEN,
                ProfileTokenUtils.getNotificationTag("prefix", mProfile));
        assertFalse(
                ProfileTokenUtils.getNotificationTag("prefix", mProfile)
                        .equals(ProfileTokenUtils.getNotificationTag("prefix", mOtherProfile)));
    }

    @Test
    public void testGetNotificationTagFromIntent() {
        Intent intent = createIntent();
        ProfileTokenUtils.addProfileToIntent(mProfile, intent);
        assertEquals(
                ProfileTokenUtils.getNotificationTag("prefix", mProfile),
                ProfileTokenUtils.getNotificationTagFromIntent("prefix", intent));
    }

    @Test
    public void testGetNotificationTagFromIntent_NoToken() {
        Intent untrustedIntent = createIntent();
        untrustedIntent.putExtra(ProfileTokenUtils.PROFILE_TOKEN_EXTRA, ENCODED_TOKEN);
        assertNull(ProfileTokenUtils.getNotificationTagFromIntent("prefix", untrustedIntent));

        Intent legacyIntent = createIntent();
        IntentUtils.addTrustedIntentExtras(legacyIntent);
        legacyIntent.putExtra(ProfileTokenUtils.LEGACY_PROFILE_TOKEN_EXTRA, LEGACY_TOKEN);
        assertNull(ProfileTokenUtils.getNotificationTagFromIntent("prefix", legacyIntent));
    }
}
