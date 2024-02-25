// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
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
import org.mockito.AdditionalAnswers;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

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
import org.chromium.components.signin.base.CoreAccountId;
import org.chromium.components.signin.identitymanager.AccountInfoServiceProvider;
import org.chromium.components.signin.identitymanager.AccountTrackerService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.IdentityManagerJni;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
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

    @Mock private AccountTrackerService mAccountTrackerServiceMock;

    @Mock private IdentityManager.Natives mIdentityManagerNativeMock;

    private final AccountInfo mAccountInfoWithAvatar =
            new AccountInfo(
                    new CoreAccountId("gaia-id-test"),
                    ACCOUNT_EMAIL,
                    "gaia-id-test",
                    "full name",
                    "given name",
                    createAvatar(),
                    new AccountCapabilities(new HashMap<>()));

    private FrameLayout mContentView;
    private ImageView mImageView;

    private IdentityManager mIdentityManager;
    private ProfileDataCache mProfileDataCache;

    @Before
    public void setUp() {
        mocker.mock(IdentityManagerJni.TEST_HOOKS, mIdentityManagerNativeMock);
        doAnswer(AdditionalAnswers.answerVoid(Runnable::run))
                .when(mAccountTrackerServiceMock)
                .legacySeedAccountsIfNeeded(any(Runnable.class));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mIdentityManager =
                            IdentityManager.create(
                                    NATIVE_IDENTITY_MANAGER, null /* OAuth2TokenService */);

                    AccountInfoServiceProvider.init(mIdentityManager, mAccountTrackerServiceMock);
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
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mIdentityManager.onExtendedAccountInfoUpdated(mAccountInfoWithAvatar);
                    checkImageIsScaled(mAccountInfoWithAvatar.getEmail());
                });
        mRenderTestRule.render(mImageView, "profile_data_cache_avatar" + mImageSize);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testProfileDataPopulatedWithoutGmsProfileDataSource() throws IOException {
        when(mIdentityManagerNativeMock.findExtendedAccountInfoByEmailAddress(
                        anyLong(), eq(ACCOUNT_EMAIL)))
                .thenReturn(mAccountInfoWithAvatar);
        mAccountManagerTestRule.addAccount(
                ACCOUNT_EMAIL,
                mAccountInfoWithAvatar.getFullName(),
                mAccountInfoWithAvatar.getGivenName(),
                mAccountInfoWithAvatar.getAccountImage());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfileDataCache =
                            new ProfileDataCache(
                                    getActivity(), mImageSize, /* badgeConfig= */ null);
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    return !TextUtils.isEmpty(
                            mProfileDataCache
                                    .getProfileDataOrDefault(mAccountInfoWithAvatar.getEmail())
                                    .getFullName());
                });
        final DisplayableProfileData profileData =
                mProfileDataCache.getProfileDataOrDefault(mAccountInfoWithAvatar.getEmail());
        Assert.assertEquals(mAccountInfoWithAvatar.getFullName(), profileData.getFullName());
        Assert.assertEquals(mAccountInfoWithAvatar.getGivenName(), profileData.getGivenName());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    checkImageIsScaled(mAccountInfoWithAvatar.getEmail());
                });
        mRenderTestRule.render(mImageView, "profile_data_cache_avatar" + mImageSize);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testNoProfileDataRemovedWithEmptyAccountInfo() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mIdentityManager.onExtendedAccountInfoUpdated(mAccountInfoWithAvatar);
                    final AccountInfo emptyAccountInfo =
                            new AccountInfo(
                                    mAccountInfoWithAvatar.getId(),
                                    mAccountInfoWithAvatar.getEmail(),
                                    mAccountInfoWithAvatar.getGaiaId(),
                                    null,
                                    null,
                                    null,
                                    new AccountCapabilities(new HashMap<>()));
                    mIdentityManager.onExtendedAccountInfoUpdated(emptyAccountInfo);
                    checkImageIsScaled(mAccountInfoWithAvatar.getEmail());
                });
        mRenderTestRule.render(mImageView, "profile_data_cache_avatar" + mImageSize);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testPlaceholderIsScaled() throws IOException {
        final String email = "no.data.for.this.account@example.com";
        mAccountManagerTestRule.addAccount(email);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    checkImageIsScaled(email);
                });
        mRenderTestRule.render(mImageView, "profile_data_cache_placeholder" + mImageSize);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testAvatarIsScaled() throws IOException {
        when(mIdentityManagerNativeMock.findExtendedAccountInfoByEmailAddress(
                        anyLong(), eq(ACCOUNT_EMAIL)))
                .thenReturn(mAccountInfoWithAvatar);
        mAccountManagerTestRule.addAccount(ACCOUNT_EMAIL);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    checkImageIsScaled(ACCOUNT_EMAIL);
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

    /**
     * Creates a simple placeholder bitmap to use as the avatar picture. The avatar is intentionally
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
