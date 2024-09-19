// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.annotation.Px;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.base.AccountCapabilities;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.AccountInfoServiceProvider;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.IdentityManagerJni;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.ui.widget.ChromeImageView;

import java.io.IOException;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;

/** Tests for {@link ProfileDataCache} image scaling. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(ProfileDataCacheRenderTest.PROFILE_DATA_BATCH_NAME)
public class ProfileDataCacheRenderTest extends BlankUiTestActivityTestCase {
    public static final String PROFILE_DATA_BATCH_NAME = "profile_data";
    public static final String ACCOUNT_EMAIL = "test@gmail.com";
    private static final long NATIVE_IDENTITY_MANAGER = 10002L;

    @ClassParameter
    private static final List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet().value(64).name("ImageSize64"),
                    new ParameterSet().value(128).name("ImageSize128"));

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

    @Rule public final JniMocker mocker = new JniMocker();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Mock private IdentityManager.Natives mIdentityManagerNativeMock;

    private FrameLayout mContentView;
    private ImageView mImageView;

    private IdentityManager mIdentityManager;
    private ProfileDataCache mProfileDataCache;

    @Before
    public void setUp() {
        mocker.mock(IdentityManagerJni.TEST_HOOKS, mIdentityManagerNativeMock);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mIdentityManager =
                            IdentityManager.create(
                                    NATIVE_IDENTITY_MANAGER, null /* OAuth2TokenService */);

                    AccountInfoServiceProvider.init(mIdentityManager);
                    Activity activity = getActivity();
                    mContentView = new FrameLayout(activity);
                    mImageView = new ChromeImageView(activity);
                    mContentView.addView(
                            mImageView, LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
                    activity.setContentView(mContentView);

                    mProfileDataCache =
                            new ProfileDataCache(activity, mImageSize, /* badgeConfig= */ null);
                });
    }

    @After
    public void tearDown() {
        AccountInfoServiceProvider.resetForTests();
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testProfileDataPopulatedFromIdentityManagerObserver() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mIdentityManager.onExtendedAccountInfoUpdated(
                            AccountManagerTestRule.TEST_ACCOUNT_1);
                    checkImageIsScaled(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail());
                });
        mRenderTestRule.render(mImageView, "profile_data_cache_avatar" + mImageSize);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testProfileDataPopulatedWithoutGmsProfileDataSource() throws IOException {
        when(mIdentityManagerNativeMock.findExtendedAccountInfoByEmailAddress(
                        anyLong(), eq(ACCOUNT_EMAIL)))
                .thenReturn(AccountManagerTestRule.TEST_ACCOUNT_1);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfileDataCache =
                            new ProfileDataCache(
                                    getActivity(), mImageSize, /* badgeConfig= */ null);
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    return !TextUtils.isEmpty(
                            mProfileDataCache
                                    .getProfileDataOrDefault(
                                            org.chromium.chrome.test.util.browser.signin
                                                    .AccountManagerTestRule.TEST_ACCOUNT_1
                                                    .getEmail())
                                    .getFullName());
                });
        final DisplayableProfileData profileData =
                mProfileDataCache.getProfileDataOrDefault(
                        AccountManagerTestRule.TEST_ACCOUNT_1.getEmail());
        Assert.assertEquals(
                AccountManagerTestRule.TEST_ACCOUNT_1.getFullName(), profileData.getFullName());
        Assert.assertEquals(
                AccountManagerTestRule.TEST_ACCOUNT_1.getGivenName(), profileData.getGivenName());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    checkImageIsScaled(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail());
                });
        mRenderTestRule.render(mImageView, "profile_data_cache_avatar" + mImageSize);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testNoProfileDataRemovedWithEmptyAccountInfo() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mIdentityManager.onExtendedAccountInfoUpdated(
                            AccountManagerTestRule.TEST_ACCOUNT_1);
                    final AccountInfo emptyAccountInfo =
                            new AccountInfo(
                                    AccountManagerTestRule.TEST_ACCOUNT_1.getId(),
                                    AccountManagerTestRule.TEST_ACCOUNT_1.getEmail(),
                                    AccountManagerTestRule.TEST_ACCOUNT_1.getGaiaId(),
                                    null,
                                    null,
                                    null,
                                    new AccountCapabilities(new HashMap<>()));
                    mIdentityManager.onExtendedAccountInfoUpdated(emptyAccountInfo);
                    checkImageIsScaled(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail());
                });
        mRenderTestRule.render(mImageView, "profile_data_cache_avatar" + mImageSize);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testPlaceholderIsScaled() throws IOException {
        // Test the case where there is no data for an account.
        when(mIdentityManagerNativeMock.findExtendedAccountInfoByEmailAddress(
                        anyLong(), eq(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail())))
                .thenReturn(null);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    checkImageIsScaled(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail());
                });
        mRenderTestRule.render(mImageView, "profile_data_cache_placeholder" + mImageSize);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testAvatarIsScaled() throws IOException {
        when(mIdentityManagerNativeMock.findExtendedAccountInfoByEmailAddress(
                        anyLong(), eq(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail())))
                .thenReturn(AccountManagerTestRule.TEST_ACCOUNT_1);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    checkImageIsScaled(AccountManagerTestRule.TEST_ACCOUNT_1.getEmail());
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
