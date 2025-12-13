// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import android.app.Activity;
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

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.IdentityManagerImpl;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.widget.ChromeImageView;

import java.io.IOException;

/**
 * Tests for ProfileDataCache with a badge. Leverages RenderTest instead of reimplementing bitmap
 * comparison to simplify access to the compared images on buildbots (via result_details).
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(ProfileDataCacheRenderTest.PROFILE_DATA_BATCH_NAME)
public class ProfileDataCacheWithBadgeRenderTest {
    private static final long NATIVE_IDENTITY_MANAGER = 10002L;

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.SERVICES_SIGN_IN)
                    .build();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    private IdentityManager mIdentityManager;
    private FrameLayout mContentView;
    private ImageView mImageView;
    private ProfileDataCache mProfileDataCache;

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // TODO(crbug.com/374683682): Use a fake IdentityManager as ProfileDataCache
                    // will be updated to fetch accounts from IdentityManager
                    mIdentityManager =
                            IdentityManagerImpl.create(
                                    NATIVE_IDENTITY_MANAGER, null /* OAuth2TokenService */);
                    mContentView = new FrameLayout(sActivity);
                    mImageView = new ChromeImageView(sActivity);
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
        setUpProfileDataCache(R.drawable.ic_account_child_20dp);
        mRenderTestRule.render(mImageView, "profile_data_cache_with_child_badge");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testProfileDataWithSyncErrorBadge() throws IOException {
        setUpProfileDataCache(R.drawable.ic_sync_badge_error_20dp);
        mRenderTestRule.render(mImageView, "profile_data_cache_with_sync_error_badge");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testProfileDataWithoutBadge() throws IOException {
        setUpProfileDataCache(0);
        mRenderTestRule.render(mImageView, "profile_data_cache_without_badge");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testProfileDataWithSettingBadgeDynamically() throws IOException {
        setUpProfileDataCache(0);
        mRenderTestRule.render(mImageView, "profile_data_cache_without_badge");
        setBadge(R.drawable.ic_account_child_20dp);
        mRenderTestRule.render(mImageView, "profile_data_cache_with_child_badge");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testProfileDataWithRemovingBadgeDynamically() throws IOException {
        setUpProfileDataCache(R.drawable.ic_account_child_20dp);
        mRenderTestRule.render(mImageView, "profile_data_cache_with_child_badge");
        setBadge(0);
        mRenderTestRule.render(mImageView, "profile_data_cache_without_badge");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testProfileDataWithExistingBadge() throws IOException {
        setUpProfileDataCache(R.drawable.ic_account_child_20dp);
        mRenderTestRule.render(mImageView, "profile_data_cache_with_child_badge");
        setBadge(R.drawable.ic_sync_badge_error_20dp);
        mRenderTestRule.render(mImageView, "profile_data_cache_with_sync_error_badge");
    }

    private void setUpProfileDataCache(@DrawableRes int badgeResId) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfileDataCache =
                            badgeResId != 0
                                    ? ProfileDataCache.createWithDefaultImageSize(
                                            sActivity, mIdentityManager, badgeResId)
                                    : ProfileDataCache.createWithoutBadge(
                                            sActivity, mIdentityManager, R.dimen.user_picture_size);
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    return !TextUtils.isEmpty(
                            mProfileDataCache
                                    .getProfileDataOrDefault(TestAccounts.ACCOUNT1.getEmail())
                                    .getFullName());
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mImageView.setImageDrawable(
                            mProfileDataCache
                                    .getProfileDataOrDefault(TestAccounts.ACCOUNT1.getEmail())
                                    .getImage());
                });
    }

    private void setBadge(@DrawableRes int badgeResId) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfileDataCache.setBadge(
                            TestAccounts.ACCOUNT1.getEmail(),
                            badgeResId == 0
                                    ? null
                                    : ProfileDataCache.createDefaultSizeChildAccountBadgeConfig(
                                            sActivity, badgeResId));
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    return !TextUtils.isEmpty(
                            mProfileDataCache
                                    .getProfileDataOrDefault(TestAccounts.ACCOUNT1.getEmail())
                                    .getFullName());
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mImageView.setImageDrawable(
                            mProfileDataCache
                                    .getProfileDataOrDefault(TestAccounts.ACCOUNT1.getEmail())
                                    .getImage());
                });
    }
}
