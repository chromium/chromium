// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
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
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.TestAccounts;
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

    @ClassParameter
    private static final List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet().value(64).name("ImageSize64"),
                    new ParameterSet().value(128).name("ImageSize128"));

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    private final @Px int mImageSize;

    public ProfileDataCacheRenderTest(int imageSize) {
        mImageSize = imageSize;
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

    private final AccountManagerFacade mAccountManagerFacade =
            mAccountManagerTestRule.getAccountManagerFacade();
    private final IdentityManager mIdentityManager = mAccountManagerTestRule.getIdentityManager();
    private ProfileDataCache mProfileDataCache;

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() {
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
                                    mAccountManagerFacade,
                                    mIdentityManager,
                                    mImageSize,
                                    /* badgeConfig= */ null);
                });
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testProfileDataPopulatedFromIdentityManagerObserver() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
                    checkImageIsScaled(TestAccounts.ACCOUNT1.getEmail());
                });
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
                                    mAccountManagerFacade,
                                    mIdentityManager,
                                    mImageSize,
                                    /* badgeConfig= */ null);
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
                    final DisplayableProfileData profileData =
                            mProfileDataCache.getProfileDataOrDefault(
                                    TestAccounts.ACCOUNT1.getEmail());
                    Assert.assertEquals(
                            TestAccounts.ACCOUNT1.getFullName(), profileData.getFullName());
                    Assert.assertEquals(
                            TestAccounts.ACCOUNT1.getGivenName(), profileData.getGivenName());
                    checkImageIsScaled(TestAccounts.ACCOUNT1.getEmail());
                });
        mRenderTestRule.render(mImageView, "profile_data_cache_avatar" + mImageSize);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testNoProfileDataRemovedWithEmptyAccountInfo() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
                    final AccountInfo emptyAccountInfo =
                            new AccountInfo.Builder(
                                            TestAccounts.ACCOUNT1.getEmail(),
                                            TestAccounts.ACCOUNT1.getGaiaId())
                                    .build();
                    mAccountManagerTestRule.updateAccount(emptyAccountInfo);
                    checkImageIsScaled(TestAccounts.ACCOUNT1.getEmail());
                });
        mRenderTestRule.render(mImageView, "profile_data_cache_avatar" + mImageSize);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testPlaceholderIsScaled() throws IOException {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    checkImageIsScaled(TestAccounts.ACCOUNT1.getEmail());
                });
        mRenderTestRule.render(mImageView, "profile_data_cache_placeholder" + mImageSize);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testAvatarIsScaled() throws IOException {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    checkImageIsScaled(TestAccounts.ACCOUNT1.getEmail());
                });
        mRenderTestRule.render(mImageView, "profile_data_cache_avatar" + mImageSize);
    }

    private void checkImageIsScaled(String accountName) {
        DisplayableProfileData displayableProfileData =
                mProfileDataCache.getProfileDataOrDefault(accountName);
        Drawable placeholderImage = displayableProfileData.getImage();
        assertEquals(mImageSize, placeholderImage.getIntrinsicHeight());
        assertEquals(mImageSize, placeholderImage.getIntrinsicWidth());
        mImageView.setImageDrawable(placeholderImage);
    }
}
