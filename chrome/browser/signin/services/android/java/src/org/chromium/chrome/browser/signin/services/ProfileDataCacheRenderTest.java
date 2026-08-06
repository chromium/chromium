// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.drawable.Drawable;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.annotation.Px;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.subscription_eligibility.SubscriptionEligibilityService;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.widget.ChromeImageView;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/** Tests for {@link ProfileDataCache} image scaling. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(ProfileDataCacheRenderTest.PROFILE_DATA_BATCH_NAME)
public class ProfileDataCacheRenderTest {
    public static final String PROFILE_DATA_BATCH_NAME = "profile_data";

    private static final int RING_THICKNESS = 3;

    // TODO(crbug.com/493130564) - Remove the data source parameterization after
    // MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS launch.
    @ClassParameter
    private static final List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet()
                            .value(64, false)
                            .name("ImageSize64_AccountManagerFacadeSource"),
                    new ParameterSet().value(64, true).name("ImageSize64_IdentityManagerSource"),
                    new ParameterSet()
                            .value(128, false)
                            .name("ImageSize128_AccountManagerFacadeSource"),
                    new ParameterSet().value(128, true).name("ImageSize128_IdentityManagerSource"));

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    private final @Px int mImageSize;
    private final boolean mIsIdentityManagerSourceOfAccounts;

    public ProfileDataCacheRenderTest(int imageSize, boolean isIdentityManagerSourceOfAccounts) {
        mImageSize = imageSize;
        mIsIdentityManagerSourceOfAccounts = isIdentityManagerSourceOfAccounts;
    }

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.SERVICES_SIGN_IN)
                    .build();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    private FrameLayout mContentView;
    private ImageView mImageView;
    @Mock private ProfileDataCache.Observer mObserver;
    @Mock private SubscriptionEligibilityService mSubscriptionEligibilityServiceMock;
    private ProfileDataCache mProfileDataCache;

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() {
        FeatureOverrides.overrideFlag(
                SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS,
                mIsIdentityManagerSourceOfAccounts);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContentView = new FrameLayout(sActivity);
                    mImageView = new ChromeImageView(sActivity);
                    mContentView.addView(
                            mImageView, LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
                    sActivity.setContentView(mContentView);

                    mProfileDataCache =
                            new ProfileDataCache(
                                    sActivity,
                                    mAccountManagerTestRule.getAccountManagerFacade(),
                                    mAccountManagerTestRule.getIdentityManager(),
                                    /* subscriptionEligibilityService= */ null,
                                    mImageSize,
                                    /* ringThicknessPx= */ 0,
                                    /* badgeConfig= */ null,
                                    /* aiTierRingEnabled= */ false,
                                    /* brandingDelegate= */ null);
                });
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testProfileDataPopulatedFromIdentityManagerObserver() throws IOException {
        mAccountManagerTestRule.blockGetAccountsUpdate();
        ThreadUtils.runOnUiThreadBlocking(() -> mProfileDataCache.addObserver(mObserver));
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        CriteriaHelper.pollUiThread(
                () -> mProfileDataCache.hasProfileDataForTesting(TestAccounts.ACCOUNT1.getId()));
        ThreadUtils.runOnUiThreadBlocking(() -> checkImageIsScaled(TestAccounts.ACCOUNT1.getId()));
        mRenderTestRule.render(mImageView, "profile_data_cache_avatar" + mImageSize);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testProfileDataPopulatedWithoutGmsProfileDataSource() throws IOException {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfileDataCache =
                            new ProfileDataCache(
                                    sActivity,
                                    mAccountManagerTestRule.getAccountManagerFacade(),
                                    mAccountManagerTestRule.getIdentityManager(),
                                    /* subscriptionEligibilityService= */ null,
                                    mImageSize,
                                    /* ringThicknessPx= */ 0,
                                    /* badgeConfig= */ null,
                                    /* aiTierRingEnabled= */ false,
                                    /* brandingDelegate= */ null);

                    final DisplayableProfileData profileData =
                            mProfileDataCache.getById(TestAccounts.ACCOUNT1.getId());
                    Assert.assertEquals(
                            TestAccounts.ACCOUNT1.getFullName(), profileData.getFullName());
                    Assert.assertEquals(
                            TestAccounts.ACCOUNT1.getGivenName(), profileData.getGivenName());
                    checkImageIsScaled(TestAccounts.ACCOUNT1.getId());
                });
        mRenderTestRule.render(mImageView, "profile_data_cache_avatar" + mImageSize);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testPlaceholderIsScaled() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(() -> mProfileDataCache.addObserver(mObserver));
        mAccountManagerTestRule.addAccount(TestAccounts.TEST_ACCOUNT_NO_NAME);
        CriteriaHelper.pollUiThread(
                () ->
                        mProfileDataCache.hasProfileDataForTesting(
                                TestAccounts.TEST_ACCOUNT_NO_NAME.getId()));
        ThreadUtils.runOnUiThreadBlocking(
                () -> checkImageIsScaled(TestAccounts.TEST_ACCOUNT_NO_NAME.getId()));
        mRenderTestRule.render(mImageView, "profile_data_cache_placeholder" + mImageSize);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testAvatarIsScaled() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(() -> mProfileDataCache.addObserver(mObserver));
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        CriteriaHelper.pollUiThread(
                () -> mProfileDataCache.hasProfileDataForTesting(TestAccounts.ACCOUNT1.getId()));
        ThreadUtils.runOnUiThreadBlocking(() -> checkImageIsScaled(TestAccounts.ACCOUNT1.getId()));
        mRenderTestRule.render(mImageView, "profile_data_cache_avatar" + mImageSize);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testAiTierRingIsScaled() throws IOException {
        when(mSubscriptionEligibilityServiceMock.getAiSubscriptionTier()).thenReturn(1);
        mAccountManagerTestRule.getIdentityManager().setPrimaryAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SubscriptionTierBrandingDelegate brandingDelegate =
                            new SubscriptionTierBrandingDelegate() {
                                @Override
                                public Shader getRingShader(RectF bounds) {
                                    return new LinearGradient(
                                            bounds.left,
                                            bounds.top,
                                            bounds.right,
                                            bounds.bottom,
                                            Color.RED,
                                            Color.GREEN,
                                            Shader.TileMode.CLAMP);
                                }
                            };
                    mProfileDataCache =
                            new ProfileDataCache(
                                    sActivity,
                                    mAccountManagerTestRule.getAccountManagerFacade(),
                                    mAccountManagerTestRule.getIdentityManager(),
                                    mSubscriptionEligibilityServiceMock,
                                    mImageSize,
                                    RING_THICKNESS,
                                    /* badgeConfig= */ null,
                                    /* aiTierRingEnabled= */ true,
                                    brandingDelegate);
                    mProfileDataCache.addObserver(mObserver);
                });

        CriteriaHelper.pollUiThread(
                () -> mProfileDataCache.hasProfileDataForTesting(TestAccounts.ACCOUNT1.getId()));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    checkImageIsScaled(TestAccounts.ACCOUNT1.getId());
                });
        mRenderTestRule.render(mImageView, "profile_data_cache_avatar_ai_tier_ring" + mImageSize);
    }

    private void checkImageIsScaled(CoreAccountId accountId, int expectedSize) {
        DisplayableProfileData displayableProfileData = mProfileDataCache.getById(accountId);
        Drawable profileDataImage = displayableProfileData.getImage();
        assertEquals(expectedSize, profileDataImage.getIntrinsicHeight());
        assertEquals(expectedSize, profileDataImage.getIntrinsicWidth());
        mImageView.setImageDrawable(profileDataImage);
    }

    private void checkImageIsScaled(CoreAccountId accountId) {
        checkImageIsScaled(accountId, mImageSize);
    }
}
