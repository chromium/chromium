// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.drawable.Drawable;
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

import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.ProfileDataSource;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountId;
import org.chromium.components.signin.identitymanager.AccountInfoService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.IdentityManagerJni;
import org.chromium.components.signin.test.util.FakeProfileDataSource;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DummyUiActivityTestCase;
import org.chromium.ui.widget.ChromeImageView;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link ProfileDataCache} image scaling.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@DisableFeatures({ChromeFeatureList.DEPRECATE_MENAGERIE_API})
@Batch(ProfileDataCacheRenderTest.PROFILE_DATA_BATCH_NAME)
public class ProfileDataCacheRenderTest extends DummyUiActivityTestCase {
    public static final String PROFILE_DATA_BATCH_NAME = "profile_data";
    public static final String ACCOUNT_EMAIL = "test@gmail.com";
    private static final long NATIVE_IDENTITY_MANAGER = 10002L;

    @ClassParameter
    private static final List<ParameterSet> sClassParams =
            Arrays.asList(new ParameterSet().value(64).name("ImageSize64"),
                    new ParameterSet().value(128).name("ImageSize128"));

    private final @Px int mImageSize;

    public ProfileDataCacheRenderTest(int imageSize) {
        mImageSize = imageSize;
    }

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

    @Rule
    public final Features.JUnitProcessor mProcessor = new Features.JUnitProcessor();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(new FakeProfileDataSource());

    @Rule
    public final JniMocker mocker = new JniMocker();

    @Mock
    private IdentityManager.Natives mIdentityManagerNativeMock;

    @Mock
    private ProfileDataCache.Observer mObserverMock;

    private final IdentityManager mIdentityManager =
            IdentityManager.create(NATIVE_IDENTITY_MANAGER, null /* OAuth2TokenService */);

    private final AccountInfo mAccountInfoWithAvatar =
            new AccountInfo(new CoreAccountId("gaia-id-test"), ACCOUNT_EMAIL, "gaia-id-test",
                    "full name", "given name", createAvatar());

    private FrameLayout mContentView;
    private ImageView mImageView;

    private ProfileDataCache mProfileDataCache;

    @Before
    public void setUp() {
        initMocks(this);
        mocker.mock(IdentityManagerJni.TEST_HOOKS, mIdentityManagerNativeMock);
        AccountInfoService.init(mIdentityManager);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Activity activity = getActivity();
            mContentView = new FrameLayout(activity);
            mImageView = new ChromeImageView(activity);
            mContentView.addView(mImageView, LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
            activity.setContentView(mContentView);

            mProfileDataCache = new ProfileDataCache(activity, mImageSize,
                    /*badgeConfig=*/null);
            // ProfileDataCache only populates the cache when an observer is added.
            mProfileDataCache.addObserver(accountId -> {});
        });
    }

    @After
    public void tearDown() {
        AccountInfoService.resetForTests();
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testProfileDataWithAvatarFromIdentityManager() throws IOException {
        when(mIdentityManagerNativeMock
                        .findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
                                anyLong(), eq(ACCOUNT_EMAIL)))
                .thenReturn(mAccountInfoWithAvatar);
        mAccountManagerTestRule.addAccount(
                new ProfileDataSource.ProfileData(ACCOUNT_EMAIL, null, "Full Name", "Given Name"));
        TestThreadUtils.runOnUiThreadBlocking(() -> { checkImageIsScaled(ACCOUNT_EMAIL); });
        mRenderTestRule.render(mImageView, "profile_data_cache_avatar" + mImageSize);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testProfileDataUpdatedFromIdentityManagerObserver() throws IOException {
        mAccountManagerTestRule.addAccount(
                new ProfileDataSource.ProfileData(ACCOUNT_EMAIL, null, "Full Name", "Given Name"));
        mIdentityManager.onExtendedAccountInfoUpdated(mAccountInfoWithAvatar);
        TestThreadUtils.runOnUiThreadBlocking(() -> { checkImageIsScaled(ACCOUNT_EMAIL); });
        mRenderTestRule.render(mImageView, "profile_data_cache_avatar" + mImageSize);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testProfileDataPopulatedFromIdentityManagerObserver() throws IOException {
        mIdentityManager.onExtendedAccountInfoUpdated(mAccountInfoWithAvatar);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { checkImageIsScaled(mAccountInfoWithAvatar.getEmail()); });
        mRenderTestRule.render(mImageView, "profile_data_cache_avatar" + mImageSize);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.DEPRECATE_MENAGERIE_API})
    @Feature("RenderTest")
    public void testProfileDataPopulatedWithoutGmsProfileDataSource() throws IOException {
        when(mIdentityManagerNativeMock
                        .findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
                                anyLong(), eq(ACCOUNT_EMAIL)))
                .thenReturn(mAccountInfoWithAvatar);
        mAccountManagerTestRule.addAccount(ACCOUNT_EMAIL);
        mProfileDataCache = new ProfileDataCache(getActivity(), mImageSize, /*badgeConfig=*/null);

        // ProfileDataCache only populates the cache when an observer is added.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mProfileDataCache.addObserver(mObserverMock); });

        // Certain classes like IdentityDiscController can trigger infinite loop if we populate
        // the cache with an existing observer, details can be found in crbug/1183295.
        verify(mObserverMock, never()).onProfileDataUpdated(any());
        final DisplayableProfileData profileData =
                mProfileDataCache.getProfileDataOrDefault(mAccountInfoWithAvatar.getEmail());
        Assert.assertEquals(mAccountInfoWithAvatar.getFullName(), profileData.getFullName());
        Assert.assertEquals(mAccountInfoWithAvatar.getGivenName(), profileData.getGivenName());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { checkImageIsScaled(mAccountInfoWithAvatar.getEmail()); });
        mRenderTestRule.render(mImageView, "profile_data_cache_avatar" + mImageSize);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testNoProfileDataRemovedWithEmptyAccountInfo() throws IOException {
        mIdentityManager.onExtendedAccountInfoUpdated(mAccountInfoWithAvatar);
        final AccountInfo emptyAccountInfo =
                new AccountInfo(mAccountInfoWithAvatar.getId(), mAccountInfoWithAvatar.getEmail(),
                        mAccountInfoWithAvatar.getGaiaId(), null, null, null);
        mIdentityManager.onExtendedAccountInfoUpdated(emptyAccountInfo);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { checkImageIsScaled(mAccountInfoWithAvatar.getEmail()); });
        mRenderTestRule.render(mImageView, "profile_data_cache_avatar" + mImageSize);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testPlaceholderIsScaled() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { checkImageIsScaled("no.data.for.this.account@example.com"); });
        mRenderTestRule.render(mImageView, "profile_data_cache_placeholder" + mImageSize);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testAvatarIsScaled() throws IOException {
        ProfileDataSource.ProfileData profileData = new ProfileDataSource.ProfileData(
                ACCOUNT_EMAIL, createAvatar(), "Full Name", "Given Name");
        mAccountManagerTestRule.addAccount(profileData);
        TestThreadUtils.runOnUiThreadBlocking(() -> { checkImageIsScaled(ACCOUNT_EMAIL); });
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

    /**
     * Creates a simple dummy bitmap to use as the avatar picture. The avatar is intentionally
     * asymmetric to test scaling.
     */
    private Bitmap createAvatar() {
        final int avatarSize = 100;
        assertNotEquals("Should be different to test scaling", mImageSize, avatarSize);

        Bitmap result = Bitmap.createBitmap(avatarSize, avatarSize, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(result);
        canvas.drawColor(Color.RED);

        Paint paint = new Paint();
        paint.setAntiAlias(true);

        paint.setColor(Color.BLUE);
        canvas.drawCircle(0, 0, avatarSize, paint);

        return result;
    }
}
