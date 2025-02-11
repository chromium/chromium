// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Intent;

import org.junit.Assert;
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

import java.util.concurrent.atomic.AtomicReference;

/** Tests for {@link ProfileIntentUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ProfileIntentUtilsTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ProfileResolver.Natives mProfileResolverNatives;
    @Mock private Profile mProfile;
    @Captor private ArgumentCaptor<Callback<Profile>> mCallbackArgumentCaptor;

    @Before
    public void setUp() {
        ProfileResolverJni.setInstanceForTesting(mProfileResolverNatives);
    }

    @Test
    public void testHasProfileToken() {
        Intent intent = new Intent();
        intent.setClass(ContextUtils.getApplicationContext(), TestActivity.class);
        assertFalse(ProfileIntentUtils.hasProfileToken(intent));

        intent.putExtra(ProfileIntentUtils.PROFILE_INTENT_KEY, "Empty_Profile_Str");
        assertFalse(ProfileIntentUtils.hasProfileToken(intent));

        IntentUtils.addTrustedIntentExtras(intent);
        assertTrue(ProfileIntentUtils.hasProfileToken(intent));
    }

    @Test
    public void testAddProfileToIntent() {
        String profileToken = "Profile_Token_Str";
        when(mProfileResolverNatives.tokenizeProfile(mProfile)).thenReturn(profileToken);

        Intent intent = new Intent();
        intent.setClass(ContextUtils.getApplicationContext(), TestActivity.class);
        assertFalse(ProfileIntentUtils.hasProfileToken(intent));
        assertFalse(IntentUtils.isTrustedIntentFromSelf(intent));

        ProfileIntentUtils.addProfileToIntent(mProfile, intent);
        assertTrue(ProfileIntentUtils.hasProfileToken(intent));
        assertTrue(IntentUtils.isTrustedIntentFromSelf(intent));
        assertEquals(profileToken, intent.getStringExtra(ProfileIntentUtils.PROFILE_INTENT_KEY));
    }

    @Test(expected = AssertionError.class)
    public void testRetrieveProfileFromIntent_UntrustedIntent() {
        String profileToken = "Profile_Token_Str";

        Intent intent = new Intent();
        intent.setClass(ContextUtils.getApplicationContext(), TestActivity.class);
        intent.putExtra(ProfileIntentUtils.PROFILE_INTENT_KEY, profileToken);
        assertFalse(ProfileIntentUtils.hasProfileToken(intent));
        assertFalse(IntentUtils.isTrustedIntentFromSelf(intent));

        AtomicReference<Profile> retrievedProfile = new AtomicReference<>();
        ProfileIntentUtils.retrieveProfileFromIntent(intent, retrievedProfile::set);
        Assert.assertNull(retrievedProfile.get());
    }

    @Test
    public void testRetrieveProfileFromIntent_TrustedIntent_NoToken() {
        Intent intent = new Intent();
        intent.setClass(ContextUtils.getApplicationContext(), TestActivity.class);
        IntentUtils.addTrustedIntentExtras(intent);
        assertFalse(ProfileIntentUtils.hasProfileToken(intent));
        assertTrue(IntentUtils.isTrustedIntentFromSelf(intent));

        AtomicReference<Profile> retrievedProfile = new AtomicReference<>();
        ProfileIntentUtils.retrieveProfileFromIntent(intent, retrievedProfile::set);
        Assert.assertNull(retrievedProfile.get());
    }

    @Test
    public void testRetrieveProfileFromIntent_TrustedIntent_HasToken_InvalidProfile() {
        String profileToken = "Invalid_Profile_Token_Str";

        Intent intent = new Intent();
        intent.setClass(ContextUtils.getApplicationContext(), TestActivity.class);
        IntentUtils.addTrustedIntentExtras(intent);
        intent.putExtra(ProfileIntentUtils.PROFILE_INTENT_KEY, profileToken);
        assertTrue(ProfileIntentUtils.hasProfileToken(intent));
        assertTrue(IntentUtils.isTrustedIntentFromSelf(intent));

        AtomicReference<Profile> retrievedProfile = new AtomicReference<>();
        ProfileIntentUtils.retrieveProfileFromIntent(intent, retrievedProfile::set);
        verify(mProfileResolverNatives)
                .resolveProfile(eq(profileToken), mCallbackArgumentCaptor.capture());
        mCallbackArgumentCaptor.getValue().onResult(null);
        Assert.assertNull(retrievedProfile.get());
    }

    @Test
    public void testRetrieveProfileFromIntent_TrustedIntent_HasToken_ValidProfile() {
        String profileToken = "Valid_Profile_Token_Str";

        Intent intent = new Intent();
        intent.setClass(ContextUtils.getApplicationContext(), TestActivity.class);
        IntentUtils.addTrustedIntentExtras(intent);
        intent.putExtra(ProfileIntentUtils.PROFILE_INTENT_KEY, profileToken);
        assertTrue(ProfileIntentUtils.hasProfileToken(intent));
        assertTrue(IntentUtils.isTrustedIntentFromSelf(intent));

        AtomicReference<Profile> retrievedProfile = new AtomicReference<>();
        ProfileIntentUtils.retrieveProfileFromIntent(intent, retrievedProfile::set);
        verify(mProfileResolverNatives)
                .resolveProfile(eq(profileToken), mCallbackArgumentCaptor.capture());
        mCallbackArgumentCaptor.getValue().onResult(mProfile);
        assertEquals(mProfile, retrievedProfile.get());
    }
}
