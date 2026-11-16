// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.preference.PreferenceViewHolder;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.signin.services.DisplayableProfileData;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.subscription_eligibility.AiSubscriptionTierNames;
import org.chromium.chrome.browser.subscription_eligibility.AiSubscriptionTierNamesJni;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.test.util.TestAccounts;

/**
 * Tests for the AI tier pill of {@link CentralAccountCardPreference} and the card's description.
 * The pill's look is checked by {@link CentralAccountCardPreferenceRenderTest}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures({
    SigninFeatures.ENABLE_AI_SUBSCRIPTION_AVATAR_RING,
    SigninFeatures.AI_SUBSCRIPTION_AVATAR_RING_FOLLOWUP
})
public class CentralAccountCardPreferenceTest {
    private static final String FULL_NAME = "Full Name";
    private static final String EMAIL = "test@gmail.com";
    private static final String TIER_SHORT_NAME = "Tier name";
    private static final String TIER_FULL_NAME = "Full tier name";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ProfileDataCache mProfileDataCache;
    @Mock private AiSubscriptionTierNames.Natives mTierNamesJni;

    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        AiSubscriptionTierNamesJni.setInstanceForTesting(mTierNamesJni);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testShowsAiTierPill() {
        when(mTierNamesJni.getShortName(1)).thenReturn(TIER_SHORT_NAME);
        when(mTierNamesJni.getFullName(1)).thenReturn(TIER_FULL_NAME);

        View view = bind(createProfileData(FULL_NAME, /* hasAiTierRing= */ true, /* tier= */ 1));

        TextView pill = view.findViewById(R.id.central_account_ai_tier_pill);
        assertEquals(View.VISIBLE, pill.getVisibility());
        assertEquals(TIER_SHORT_NAME, pill.getText().toString());
        assertNotNull(pill.getBackground());
        assertFalse(pill.isClickable());
        assertFalse(pill.isFocusable());
        assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_NO, pill.getImportantForAccessibility());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testCardDescriptionHasTierName() {
        when(mTierNamesJni.getShortName(1)).thenReturn(TIER_SHORT_NAME);
        when(mTierNamesJni.getFullName(1)).thenReturn(TIER_FULL_NAME);

        View view = bind(createProfileData(FULL_NAME, /* hasAiTierRing= */ true, /* tier= */ 1));

        assertDescription(view, FULL_NAME + ", " + EMAIL + ". " + TIER_FULL_NAME + " membership.");
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testCardDescriptionWithEmailOnly() {
        when(mTierNamesJni.getShortName(1)).thenReturn(TIER_SHORT_NAME);
        when(mTierNamesJni.getFullName(1)).thenReturn(TIER_FULL_NAME);

        View view =
                bind(
                        createProfileData(
                                /* fullName= */ null, /* hasAiTierRing= */ true, /* tier= */ 1));

        assertDescription(view, EMAIL + ". " + TIER_FULL_NAME + " membership.");
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testCardDescriptionWithoutDisplayableEmail() {
        when(mTierNamesJni.getShortName(1)).thenReturn(TIER_SHORT_NAME);
        when(mTierNamesJni.getFullName(1)).thenReturn(TIER_FULL_NAME);

        View view =
                bind(
                        createProfileData(
                                FULL_NAME,
                                /* hasDisplayableEmailAddress= */ false,
                                /* hasAiTierRing= */ true,
                                /* tier= */ 1));
        assertDescription(view, FULL_NAME + ". " + TIER_FULL_NAME + " membership.");

        // Without a name either, the card shows a default string.
        view =
                bind(
                        createProfileData(
                                /* fullName= */ null,
                                /* hasDisplayableEmailAddress= */ false,
                                /* hasAiTierRing= */ true,
                                /* tier= */ 1));
        assertDescription(
                view,
                mContext.getString(R.string.default_google_account_username)
                        + ". "
                        + TIER_FULL_NAME
                        + " membership.");
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testCardDescriptionWithoutTierFullName() {
        // The full name is empty when the server overrides it with an empty name.
        when(mTierNamesJni.getShortName(1)).thenReturn(TIER_SHORT_NAME);
        when(mTierNamesJni.getFullName(1)).thenReturn("");

        View view = bind(createProfileData(FULL_NAME, /* hasAiTierRing= */ true, /* tier= */ 1));

        assertEquals(
                View.VISIBLE, view.findViewById(R.id.central_account_ai_tier_pill).getVisibility());
        // Branded builds say "Google membership", others only "membership", as on the New Tab
        // Page.
        assertMembershipDescription(view);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testCardDescriptionWithEmailOnlyAndWithoutTierFullName() {
        when(mTierNamesJni.getShortName(1)).thenReturn(TIER_SHORT_NAME);
        when(mTierNamesJni.getFullName(1)).thenReturn("");

        View view =
                bind(
                        createProfileData(
                                /* fullName= */ null, /* hasAiTierRing= */ true, /* tier= */ 1));

        assertDescription(
                view,
                mContext.getString(
                        R.string.account_settings_card_description_single_line_with_membership,
                        EMAIL));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testMembershipDescriptionWithoutAiTierPillForUnknownTiers() {
        // E.g. a tier added after this version was released: it has the ring but no name.
        when(mTierNamesJni.getShortName(99)).thenReturn("");

        View view = bind(createProfileData(FULL_NAME, /* hasAiTierRing= */ true, /* tier= */ 99));

        // The ring is shown, so screen readers still hear about the membership.
        assertNoAiTierPill(view);
        assertMembershipDescription(view);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testNoAiTierPillWithoutAiTierRing() {
        // E.g. during an identity error.
        View view = bind(createProfileData(FULL_NAME, /* hasAiTierRing= */ false, /* tier= */ 1));

        assertNoAiTierPill(view);
        assertNoDescription(view);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testRebindHidesAiTierPill() {
        when(mTierNamesJni.getShortName(1)).thenReturn(TIER_SHORT_NAME);
        when(mTierNamesJni.getFullName(1)).thenReturn(TIER_FULL_NAME);
        CentralAccountCardPreference preference =
                createPreference(
                        createProfileData(FULL_NAME, /* hasAiTierRing= */ true, /* tier= */ 1));
        PreferenceViewHolder holder = createViewHolder(preference);
        preference.onBindViewHolder(holder);
        assertEquals(
                View.VISIBLE,
                holder.itemView.findViewById(R.id.central_account_ai_tier_pill).getVisibility());

        // E.g. an identity error appears: the same view is bound again, without the ring.
        preference.onProfileDataUpdated(
                createProfileData(FULL_NAME, /* hasAiTierRing= */ false, /* tier= */ 1));
        preference.onBindViewHolder(holder);

        assertNoAiTierPill(holder.itemView);
        assertNoDescription(holder.itemView);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testEachViewHasItsOwnAiTierPillDrawable() {
        when(mTierNamesJni.getShortName(1)).thenReturn(TIER_SHORT_NAME);
        when(mTierNamesJni.getFullName(1)).thenReturn(TIER_FULL_NAME);
        CentralAccountCardPreference preference =
                createPreference(
                        createProfileData(FULL_NAME, /* hasAiTierRing= */ true, /* tier= */ 1));
        PreferenceViewHolder holder = createViewHolder(preference);
        preference.onBindViewHolder(holder);
        TextView pill = holder.itemView.findViewById(R.id.central_account_ai_tier_pill);
        Drawable background = pill.getBackground();

        // The same view is bound again, after the pill is hidden and shown again: it keeps its
        // drawable.
        preference.onProfileDataUpdated(
                createProfileData(FULL_NAME, /* hasAiTierRing= */ false, /* tier= */ 1));
        preference.onBindViewHolder(holder);
        preference.onProfileDataUpdated(
                createProfileData(FULL_NAME, /* hasAiTierRing= */ true, /* tier= */ 1));
        preference.onBindViewHolder(holder);
        assertSame(background, pill.getBackground());

        // A new view is bound, e.g. for a change animation, while the old one is still shown: it
        // gets its own drawable, since a drawable can't be shared by two views.
        PreferenceViewHolder newHolder = createViewHolder(preference);
        preference.onBindViewHolder(newHolder);
        TextView newPill = newHolder.itemView.findViewById(R.id.central_account_ai_tier_pill);
        assertNotNull(newPill.getBackground());
        assertNotSame(background, newPill.getBackground());
        assertSame(background, pill.getBackground());

        verify(mProfileDataCache, times(2)).createAiTierPillDrawable();
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures({
        SigninFeatures.ENABLE_AI_SUBSCRIPTION_AVATAR_RING,
        SigninFeatures.AI_SUBSCRIPTION_AVATAR_RING_FOLLOWUP
    })
    public void testNoAiTierPillInLegacyLayout() {
        View view = bind(createProfileData(FULL_NAME, /* hasAiTierRing= */ false, /* tier= */ 0));

        assertNull(view.findViewById(R.id.central_account_ai_tier_pill));
        assertNull(view.findViewById(R.id.central_account_card).getContentDescription());
    }

    private static void assertNoAiTierPill(View view) {
        TextView pill = view.findViewById(R.id.central_account_ai_tier_pill);
        assertEquals(View.GONE, pill.getVisibility());
    }

    /** Checks that the card has no description, so that screen readers read its texts. */
    private static void assertNoDescription(View view) {
        View card = view.findViewById(R.id.central_account_card);
        assertNull(card.getContentDescription());
        assertFalse(card.isScreenReaderFocusable());
        assertTextsImportance(view, View.IMPORTANT_FOR_ACCESSIBILITY_AUTO);
    }

    /** Checks the generic membership description, for an account with a name and an email. */
    private void assertMembershipDescription(View view) {
        assertDescription(
                view,
                mContext.getString(
                        R.string.account_settings_card_description_with_membership,
                        FULL_NAME,
                        EMAIL));
    }

    /** Checks that the card is one stop for screen readers, which read {@code description}. */
    private static void assertDescription(View view, String description) {
        View card = view.findViewById(R.id.central_account_card);
        assertEquals(description, card.getContentDescription().toString());
        assertTrue(card.isScreenReaderFocusable());
        assertFalse(card.isFocusable());
        // The description replaces the texts.
        assertTextsImportance(view, View.IMPORTANT_FOR_ACCESSIBILITY_NO);
    }

    private static void assertTextsImportance(View view, int importance) {
        assertEquals(
                importance,
                view.findViewById(R.id.central_account_primary_text)
                        .getImportantForAccessibility());
        assertEquals(
                importance,
                view.findViewById(R.id.central_account_secondary_text)
                        .getImportantForAccessibility());
    }

    private DisplayableProfileData createProfileData(
            @Nullable String fullName, boolean hasAiTierRing, int tier) {
        return createProfileData(
                fullName, /* hasDisplayableEmailAddress= */ true, hasAiTierRing, tier);
    }

    private DisplayableProfileData createProfileData(
            @Nullable String fullName,
            boolean hasDisplayableEmailAddress,
            boolean hasAiTierRing,
            int tier) {
        return new DisplayableProfileData(
                TestAccounts.ACCOUNT1.getId(),
                EMAIL,
                new ColorDrawable(),
                fullName,
                /* givenName= */ null,
                hasDisplayableEmailAddress,
                hasAiTierRing,
                tier);
    }

    private CentralAccountCardPreference createPreference(DisplayableProfileData profileData) {
        when(mProfileDataCache.createAiTierPillDrawable())
                .thenAnswer(invocation -> new ColorDrawable());
        when(mProfileDataCache.getById(TestAccounts.ACCOUNT1.getId())).thenReturn(profileData);
        CentralAccountCardPreference preference =
                new CentralAccountCardPreference(mContext, /* attrs= */ null);
        preference.setSelectable(false);
        preference.initialize(TestAccounts.ACCOUNT1.getId(), mProfileDataCache);
        return preference;
    }

    private PreferenceViewHolder createViewHolder(CentralAccountCardPreference preference) {
        View view =
                LayoutInflater.from(mContext).inflate(preference.getLayoutResource(), null, false);
        return PreferenceViewHolder.createInstanceForTests(view);
    }

    /** Binds a new card to {@code profileData} and returns its view. */
    private View bind(DisplayableProfileData profileData) {
        CentralAccountCardPreference preference = createPreference(profileData);
        PreferenceViewHolder holder = createViewHolder(preference);
        preference.onBindViewHolder(holder);
        return holder.itemView;
    }
}
