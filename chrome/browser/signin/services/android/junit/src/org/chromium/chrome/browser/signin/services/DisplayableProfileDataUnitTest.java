// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.drawable.Drawable;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.subscription_eligibility.AiSubscriptionTierNames;
import org.chromium.chrome.browser.subscription_eligibility.AiSubscriptionTierNamesJni;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.google_apis.gaia.GaiaId;

/** Unit tests for {@link DisplayableProfileData}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DisplayableProfileDataUnitTest {
    private static final String EMAIL = "test@gmail.com";
    private static final CoreAccountId ACCOUNT_ID = new CoreAccountId(new GaiaId("gaia-id-123"));
    private final Drawable mMockDrawable = mock(Drawable.class);

    private DisplayableProfileData createProfileData(String fullName, String givenName) {
        return new DisplayableProfileData(
                ACCOUNT_ID,
                EMAIL,
                mMockDrawable,
                fullName,
                givenName,
                /* hasDisplayableEmailAddress= */ true,
                /* hasAiTierRing= */ false,
                /* aiSubscriptionTier= */ 0);
    }

    private DisplayableProfileData createProfileDataWithAiTier(boolean hasAiTierRing, int tier) {
        return new DisplayableProfileData(
                ACCOUNT_ID,
                EMAIL,
                mMockDrawable,
                "John Doe",
                "John",
                /* hasDisplayableEmailAddress= */ true,
                hasAiTierRing,
                tier);
    }

    @Test
    public void testGetAiTierNames_withAiTierRing() {
        AiSubscriptionTierNames.Natives tierNamesJni = mock(AiSubscriptionTierNames.Natives.class);
        when(tierNamesJni.getShortName(2)).thenReturn("Tier name");
        when(tierNamesJni.getFullName(2)).thenReturn("Full tier name");
        AiSubscriptionTierNamesJni.setInstanceForTesting(tierNamesJni);

        DisplayableProfileData profileData =
                createProfileDataWithAiTier(/* hasAiTierRing= */ true, /* tier= */ 2);
        assertEquals("Tier name", profileData.getAiTierPillName());
        assertEquals("Full tier name", profileData.getAiTierFullName());
    }

    @Test
    public void testGetAiTierPillName_withoutAiTierRing() {
        AiSubscriptionTierNames.Natives tierNamesJni = mock(AiSubscriptionTierNames.Natives.class);
        AiSubscriptionTierNamesJni.setInstanceForTesting(tierNamesJni);

        // E.g. a badge or an identity error replaced the ring.
        DisplayableProfileData profileData =
                createProfileDataWithAiTier(/* hasAiTierRing= */ false, /* tier= */ 2);
        assertEquals("", profileData.getAiTierPillName());
        assertEquals("", profileData.getAiTierFullName());
        verifyNoInteractions(tierNamesJni);
    }

    @Test
    public void testGetFullNameOrFallbackName_withFullName() {
        DisplayableProfileData profileData = createProfileData("John Doe", "John");
        Context context = RuntimeEnvironment.getApplication();
        assertEquals("John Doe", profileData.getFullNameOrFallbackName(context));
    }

    @Test
    public void testGetFullNameOrFallbackName_withNullFullName() {
        DisplayableProfileData profileData = createProfileData(null, "John");
        Context context = RuntimeEnvironment.getApplication();
        assertEquals(
                context.getString(R.string.default_google_account_username),
                profileData.getFullNameOrFallbackName(context));
    }

    @Test
    public void testGetFullNameOrFallbackName_withEmptyFullName() {
        DisplayableProfileData profileData = createProfileData("", "John");
        Context context = RuntimeEnvironment.getApplication();
        assertEquals(
                context.getString(R.string.default_google_account_username),
                profileData.getFullNameOrFallbackName(context));
    }

    @Test
    public void testGetFullNameOrEmail_withFullName() {
        DisplayableProfileData profileData = createProfileData("John Doe", "John");
        assertEquals("John Doe", profileData.getFullNameOrEmail());
    }

    @Test
    public void testGetFullNameOrEmail_withNullFullName() {
        DisplayableProfileData profileData = createProfileData(null, "John");
        assertEquals(EMAIL, profileData.getFullNameOrEmail());
    }

    @Test
    public void testGetFullNameOrEmail_withEmptyFullName() {
        DisplayableProfileData profileData = createProfileData("", "John");
        assertEquals(EMAIL, profileData.getFullNameOrEmail());
    }

    @Test
    public void testGetGivenNameOrFullNameOrEmail_withGivenName() {
        DisplayableProfileData profileData = createProfileData("John Doe", "John");
        assertEquals("John", profileData.getGivenNameOrFullNameOrEmail());
    }

    @Test
    public void testGetGivenNameOrFullNameOrEmail_withNullGivenNameAndFullName() {
        DisplayableProfileData profileData = createProfileData("John Doe", null);
        assertEquals("John Doe", profileData.getGivenNameOrFullNameOrEmail());
    }

    @Test
    public void testGetGivenNameOrFullNameOrEmail_withEmptyGivenNameAndFullName() {
        DisplayableProfileData profileData = createProfileData("John Doe", "");
        assertEquals("John Doe", profileData.getGivenNameOrFullNameOrEmail());
    }

    @Test
    public void testGetGivenNameOrFullNameOrEmail_withNullGivenNameAndNullFullName() {
        DisplayableProfileData profileData = createProfileData(null, null);
        assertEquals(EMAIL, profileData.getGivenNameOrFullNameOrEmail());
    }

    @Test
    public void testGetGivenNameOrFullNameOrEmail_withEmptyGivenNameAndEmptyFullName() {
        DisplayableProfileData profileData = createProfileData("", "");
        assertEquals(EMAIL, profileData.getGivenNameOrFullNameOrEmail());
    }
}
