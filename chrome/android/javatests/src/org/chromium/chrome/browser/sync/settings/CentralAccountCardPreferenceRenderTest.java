// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;

import androidx.preference.PreferenceViewHolder;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SubscriptionTierBrandingDelegate;
import org.chromium.chrome.browser.subscription_eligibility.AiSubscriptionTierNames;
import org.chromium.chrome.browser.subscription_eligibility.AiSubscriptionTierNamesJni;
import org.chromium.chrome.browser.subscription_eligibility.SubscriptionEligibilityService;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.FakeSubscriptionTierBrandingDelegate;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;

import java.io.IOException;
import java.util.List;

/** Render tests for the AI tier pill of {@link CentralAccountCardPreference}. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures({
    SigninFeatures.ENABLE_AI_SUBSCRIPTION_AVATAR_RING,
    SigninFeatures.AI_SUBSCRIPTION_AVATAR_RING_FOLLOWUP
})
public class CentralAccountCardPreferenceRenderTest {
    private static final int AI_SUBSCRIPTION_TIER = 1;

    @ClassParameter
    private static final List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.SERVICES_SIGN_IN)
                    .build();

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Mock private SubscriptionEligibilityService mSubscriptionEligibilityService;
    @Mock private AiSubscriptionTierNames.Natives mTierNamesJni;

    public CentralAccountCardPreferenceRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(/* startIntent= */ null);
        mActivityTestRule.getActivity().setTheme(R.style.Theme_BrowserUI_DayNight);
        // Fixed names, so that the images don't depend on the build's branding.
        AiSubscriptionTierNamesJni.setInstanceForTesting(mTierNamesJni);
        when(mTierNamesJni.getFullName(AI_SUBSCRIPTION_TIER)).thenReturn("Full tier name");
        when(mTierNamesJni.shouldShowDecoration(AI_SUBSCRIPTION_TIER)).thenReturn(true);
        // Test APKs have no branding delegate. The fake one draws the wave and the shadow.
        ServiceLoaderUtil.setInstanceForTesting(
                SubscriptionTierBrandingDelegate.class, new FakeSubscriptionTierBrandingDelegate());
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                NightModeTestUtils::tearDownNightModeForBlankUiTestActivity);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testAiTierPill() throws IOException {
        View card = showCard(mActivityTestRule.getActivity(), "Tier name");
        mRenderTestRule.render(card, "central_account_card_ai_tier_pill");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testAiTierPill_Rtl() throws IOException {
        LocalizationUtils.setRtlForTesting(true);
        View card = showCard(mActivityTestRule.getActivity(), "Tier name");
        mRenderTestRule.render(card, "central_account_card_ai_tier_pill_rtl");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testAiTierPill_LargeFont() throws IOException {
        Activity activity = mActivityTestRule.getActivity();
        Configuration config = new Configuration(activity.getResources().getConfiguration());
        config.fontScale = 2.0f;
        Context largeFontContext = activity.createConfigurationContext(config);
        largeFontContext.setTheme(R.style.Theme_BrowserUI_DayNight);
        View card = showCard(largeFontContext, "Tier name");
        mRenderTestRule.render(card, "central_account_card_ai_tier_pill_large_font");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testAiTierPill_LongName() throws IOException {
        View card =
                showCard(
                        mActivityTestRule.getActivity(),
                        "A tier name that is much too long to fit in the pill");
        mRenderTestRule.render(card, "central_account_card_ai_tier_pill_long_name");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testNoAiTierPill() throws IOException {
        // E.g. a tier without a name: the pill is gone, so no space is left below the avatar.
        View card = showCard(mActivityTestRule.getActivity(), "");
        mRenderTestRule.render(card, "central_account_card_no_ai_tier_pill");
    }

    /**
     * Shows the central account card for an account whose AI subscription tier has {@code
     * tierShortName}, and returns the card.
     */
    private View showCard(Context context, String tierShortName) {
        when(mTierNamesJni.getShortName(AI_SUBSCRIPTION_TIER)).thenReturn(tierShortName);
        when(mSubscriptionEligibilityService.getAiSubscriptionTier())
                .thenReturn(AI_SUBSCRIPTION_TIER);
        mAccountManagerTestRule.getIdentityManager().setPrimaryAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        // A real cache, so that the avatar has the ring and the same size and padding as on the
        // Account page.
        int imageSize =
                context.getResources()
                        .getDimensionPixelSize(
                                R.dimen.account_management_picture_size_with_ai_tier_ring);
        int ringThickness =
                context.getResources().getDimensionPixelSize(R.dimen.ai_tier_ring_thickness_large);
        ProfileDataCache profileDataCache =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                ProfileDataCache.createWithAiTierRing(
                                        context,
                                        mAccountManagerTestRule.getIdentityManager(),
                                        mSubscriptionEligibilityService,
                                        /* syncService= */ null,
                                        imageSize,
                                        ringThickness));
        CriteriaHelper.pollUiThread(
                () -> profileDataCache.hasProfileDataForTesting(TestAccounts.ACCOUNT1.getId()));

        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Activity activity = mActivityTestRule.getActivity();
                    CentralAccountCardPreference preference =
                            new CentralAccountCardPreference(context, /* attrs= */ null);
                    preference.setSelectable(false);
                    preference.initialize(TestAccounts.ACCOUNT1.getId(), profileDataCache);

                    FrameLayout contentView = new FrameLayout(activity);
                    View card =
                            LayoutInflater.from(context)
                                    .inflate(preference.getLayoutResource(), contentView, false);
                    // The background of settings pages, which the pill must stand out from.
                    card.setBackgroundColor(SemanticColorUtils.getSettingsBackgroundColor(context));
                    if (LocalizationUtils.isLayoutRtl()) {
                        card.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
                    }
                    contentView.addView(card);
                    activity.setContentView(contentView);
                    preference.onBindViewHolder(PreferenceViewHolder.createInstanceForTests(card));
                    return card;
                });
    }
}
