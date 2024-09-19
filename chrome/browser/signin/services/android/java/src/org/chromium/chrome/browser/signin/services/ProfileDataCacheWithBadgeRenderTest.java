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
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.ui.widget.ChromeImageView;

import java.io.IOException;

/**
 * Tests for ProfileDataCache with a badge. Leverages RenderTest instead of reimplementing bitmap
 * comparison to simplify access to the compared images on buildbots (via result_details).
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(ProfileDataCacheRenderTest.PROFILE_DATA_BATCH_NAME)
public class ProfileDataCacheWithBadgeRenderTest extends BlankUiTestActivityTestCase {

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.SERVICES_SIGN_IN)
                    .build();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    private FrameLayout mContentView;
    private ImageView mImageView;
    private ProfileDataCache mProfileDataCache;

    @Before
    public void setUp() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Activity activity = getActivity();
                    mContentView = new FrameLayout(activity);
                    mImageView = new ChromeImageView(activity);
                    mContentView.addView(
                            mImageView,
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT);
                    activity.setContentView(mContentView);
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
        setBadgeConfig(R.drawable.ic_account_child_20dp);
        mRenderTestRule.render(mImageView, "profile_data_cache_with_child_badge");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testProfileDataWithRemovingBadgeDynamically() throws IOException {
        setUpProfileDataCache(R.drawable.ic_account_child_20dp);
        mRenderTestRule.render(mImageView, "profile_data_cache_with_child_badge");
        setBadgeConfig(0);
        mRenderTestRule.render(mImageView, "profile_data_cache_without_badge");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testProfileDataWithExistingBadge() throws IOException {
        setUpProfileDataCache(R.drawable.ic_account_child_20dp);
        mRenderTestRule.render(mImageView, "profile_data_cache_with_child_badge");
        setBadgeConfig(R.drawable.ic_sync_badge_error_20dp);
        mRenderTestRule.render(mImageView, "profile_data_cache_with_sync_error_badge");
    }

    private void setUpProfileDataCache(@DrawableRes int badgeResId) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfileDataCache =
                            badgeResId != 0
                                    ? ProfileDataCache.createWithDefaultImageSize(
                                            getActivity(), badgeResId)
                                    : ProfileDataCache.createWithoutBadge(
                                            getActivity(), R.dimen.user_picture_size);
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    return !TextUtils.isEmpty(
                            mProfileDataCache
                                    .getProfileDataOrDefault(
                                            AccountManagerTestRule.TEST_ACCOUNT_1.getEmail())
                                    .getFullName());
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mImageView.setImageDrawable(
                            mProfileDataCache
                                    .getProfileDataOrDefault(
                                            AccountManagerTestRule.TEST_ACCOUNT_1.getEmail())
                                    .getImage());
                });
    }

    private void setBadgeConfig(@DrawableRes int badgeResId) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfileDataCache.setBadge(badgeResId);
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    return !TextUtils.isEmpty(
                            mProfileDataCache
                                    .getProfileDataOrDefault(
                                            AccountManagerTestRule.TEST_ACCOUNT_1.getEmail())
                                    .getFullName());
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mImageView.setImageDrawable(
                            mProfileDataCache
                                    .getProfileDataOrDefault(
                                            AccountManagerTestRule.TEST_ACCOUNT_1.getEmail())
                                    .getImage());
                });
    }
}
