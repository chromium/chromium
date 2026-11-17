// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.RectF;
import android.graphics.Shader;
import android.text.TextUtils;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.annotation.DrawableRes;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

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
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.subscription_eligibility.SubscriptionEligibilityService;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for ProfileDataCache with a badge. Leverages RenderTest instead of reimplementing bitmap
 * comparison to simplify access to the compared images on buildbots (via result_details).
 *
 * <p>TODO(crbug.com/493130564): Revert to regular runner after
 * MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS launch.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(ProfileDataCacheRenderTest.PROFILE_DATA_BATCH_NAME)
public class ProfileDataCacheWithBadgeRenderTest {
    // The avatar size the edit avatar badge is designed for (account_management_picture_size,
    // the Account page's central card).
    private static final int EDIT_AVATAR_BADGE_AVATAR_SIZE_DP = 60;

    @ClassParameter
    private static final List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet().value(false).name("AccountManagerFacadeSource"),
                    new ParameterSet().value(true).name("IdentityManagerSource"));

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(2)
                    .setBugComponent(ChromeRenderTestRule.Component.SERVICES_SIGN_IN)
                    .build();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SubscriptionEligibilityService mSubscriptionEligibilityServiceMock;

    private IdentityManager mIdentityManager;
    private FrameLayout mContentView;
    private ImageView mImageView;
    private ProfileDataCache mProfileDataCache;

    private final boolean mIsIdentityManagerSourceOfAccounts;

    public ProfileDataCacheWithBadgeRenderTest(boolean isIdentityManagerSourceOfAccounts) {
        mIsIdentityManagerSourceOfAccounts = isIdentityManagerSourceOfAccounts;
    }

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() {
        FeatureOverrides.overrideFlag(
                SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS,
                mIsIdentityManagerSourceOfAccounts);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mIdentityManager = mAccountManagerTestRule.getIdentityManager();
                    mContentView = new FrameLayout(sActivity);
                    mImageView = new ImageView(sActivity);
                    mContentView.addView(
                            mImageView,
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT);
                    sActivity.setContentView(mContentView);
                });
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testProfileDataWithChildBadge() throws IOException {
        setUpProfileDataCache();
        mRenderTestRule.render(mImageView, "profile_data_cache_without_badge");
        setBadge(R.drawable.ic_account_child_20dp);
        mRenderTestRule.render(mImageView, "profile_data_cache_with_child_badge");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testProfileDataWithoutBadge() throws IOException {
        setUpProfileDataCache();
        mRenderTestRule.render(mImageView, "profile_data_cache_without_badge");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testProfileDataWithRemovingBadgeDynamically() throws IOException {
        setUpProfileDataCache();
        setBadge(R.drawable.ic_account_child_20dp);
        mRenderTestRule.render(mImageView, "profile_data_cache_with_child_badge");
        setBadge(0);
        mRenderTestRule.render(mImageView, "profile_data_cache_without_badge");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testProfileDataWithEditAvatarBadgeConfig() throws IOException {
        setUpProfileDataCacheForEditAvatarBadge(/* aiTierRingEnabled= */ false);
        setBadge(
                BadgeConfig.create(R.drawable.ic_error_badge_16dp)
                        .withEditAvatarConfig()
                        .withAiTierRingKept()
                        .build(sActivity));
        mRenderTestRule.render(mImageView, "profile_data_cache_with_edit_avatar_badge_config");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testProfileDataWithBadgeOverAiTierRing() throws IOException {
        setUpProfileDataCacheForEditAvatarBadge(/* aiTierRingEnabled= */ true);
        mRenderTestRule.render(mImageView, "profile_data_cache_with_ai_tier_ring_60dp");

        // A badge built to keep the ring is drawn over it.
        setBadge(
                BadgeConfig.create(R.drawable.ic_error_badge_16dp)
                        .withEditAvatarConfig()
                        .withAiTierRingKept()
                        .build(sActivity));
        mRenderTestRule.render(mImageView, "profile_data_cache_with_badge_over_ai_tier_ring");

        // The same badge without withAiTierRingKept() replaces the ring.
        setBadge(
                BadgeConfig.create(R.drawable.ic_error_badge_16dp)
                        .withEditAvatarConfig()
                        .build(sActivity));
        mRenderTestRule.render(mImageView, "profile_data_cache_with_badge_replacing_ai_tier_ring");
    }

    private void setUpProfileDataCache() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfileDataCache =
                            ProfileDataCache.create(
                                    sActivity, mIdentityManager, R.dimen.user_picture_size);
                });
        waitForProfileDataAndShowImage();
    }

    /**
     * Creates a cache for the 60dp avatar the edit avatar badge is designed for, with or without
     * the AI tier ring around it, and shows the primary account's image.
     */
    private void setUpProfileDataCacheForEditAvatarBadge(boolean aiTierRingEnabled) {
        if (aiTierRingEnabled) {
            when(mSubscriptionEligibilityServiceMock.getAiSubscriptionTier()).thenReturn(1);
        }
        mAccountManagerTestRule.getIdentityManager().setPrimaryAccount(TestAccounts.ACCOUNT1);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var resources = sActivity.getResources();
                    int avatarSize =
                            Math.round(
                                    EDIT_AVATAR_BADGE_AVATAR_SIZE_DP
                                            * resources.getDisplayMetrics().density);
                    int ringThickness =
                            resources.getDimensionPixelSize(R.dimen.ai_tier_ring_thickness_large);
                    int ringSpacing = resources.getDimensionPixelSize(R.dimen.ai_tier_ring_spacing);
                    int imageSize =
                            aiTierRingEnabled
                                    ? avatarSize + 2 * (ringThickness + ringSpacing)
                                    : avatarSize;
                    mProfileDataCache =
                            new ProfileDataCache(
                                    sActivity,
                                    mAccountManagerTestRule.getAccountManagerFacade(),
                                    mIdentityManager,
                                    aiTierRingEnabled ? mSubscriptionEligibilityServiceMock : null,
                                    /* syncService= */ null,
                                    imageSize,
                                    aiTierRingEnabled ? ringThickness : 0,
                                    aiTierRingEnabled,
                                    aiTierRingEnabled ? createBrandingDelegate() : null);
                });
        waitForProfileDataAndShowImage();
    }

    private static SubscriptionTierBrandingDelegate createBrandingDelegate() {
        return new SubscriptionTierBrandingDelegate() {
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
    }

    private void waitForProfileDataAndShowImage() {
        CriteriaHelper.pollUiThread(
                () -> {
                    return !TextUtils.isEmpty(
                            mProfileDataCache.getById(TestAccounts.ACCOUNT1.getId()).getFullName());
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mImageView.setImageDrawable(
                            mProfileDataCache.getById(TestAccounts.ACCOUNT1.getId()).getImage());
                });
    }

    private void setBadge(@DrawableRes int badgeResId) {
        setBadge(
                badgeResId == 0
                        ? null
                        : BadgeConfig.create(badgeResId)
                                .withDefaultSizeChildAccountConfig()
                                .build(sActivity));
    }

    private void setBadge(@Nullable BadgeConfig badgeConfig) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfileDataCache.setBadge(TestAccounts.ACCOUNT1.getId(), badgeConfig);
                });
        waitForProfileDataAndShowImage();
    }
}
