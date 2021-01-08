// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
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
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.ProfileDataSource;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountId;
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
 * Tests for ProfileDataCache image scaling. Leverages RenderTest instead of reimplementing
 * bitmap comparison to simplify access to the compared images on buildbots (via result_details).
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(ProfileDataCacheRenderTest.PROFILE_DATA_BATCH_NAME)
public class ProfileDataCacheRenderTest extends DummyUiActivityTestCase {
    public static final String PROFILE_DATA_BATCH_NAME = "profile_data";

    @ClassParameter
    private static final List<ParameterSet> sClassParams =
            Arrays.asList(new ParameterSet().value(64).name("ImageSize64"),
                    new ParameterSet().value(128).name("ImageSize128"));

    private final @Px int mImageSize;

    public ProfileDataCacheRenderTest(int imageSize) {
        mImageSize = imageSize;
    }

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(new FakeProfileDataSource());

    @Rule
    public final JniMocker mocker = new JniMocker();

    @Mock
    private Profile mProfileMock;

    @Mock
    private IdentityServicesProvider mIdentityServicesProviderMock;

    @Mock
    private IdentityManager.Natives mIdentityManagerNativeMock;

    private final IdentityManager mIdentityManager =
            new IdentityManager(0 /* nativeIdentityManager */, null /* OAuth2TokenService */);

    private FrameLayout mContentView;
    private ImageView mImageView;

    private ProfileDataCache mProfileDataCache;

    @Before
    public void setUp() {
        initMocks(this);
        mocker.mock(IdentityManagerJni.TEST_HOOKS, mIdentityManagerNativeMock);
        Profile.setLastUsedProfileForTesting(mProfileMock);
        when(mIdentityServicesProviderMock.getIdentityManager(mProfileMock))
                .thenReturn(mIdentityManager);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Activity activity = getActivity();
            mContentView = new FrameLayout(activity);
            mImageView = new ChromeImageView(activity);
            mContentView.addView(mImageView, LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
            activity.setContentView(mContentView);

            mProfileDataCache = new ProfileDataCache(getActivity(), mImageSize, null);
            // ProfileDataCache only populates the cache when an observer is added.
            mProfileDataCache.addObserver(accountId -> {});
        });
    }

    @After
    public void tearDown() {
        IdentityServicesProvider.setInstanceForTests(null);
        Profile.setLastUsedProfileForTesting(null);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testProfileDataUpdatedFromIdentityManager() throws IOException {
        String accountEmail = "test@example.com";
        when(mIdentityManagerNativeMock
                        .findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
                                anyLong(), eq(accountEmail)))
                .thenReturn(new AccountInfo(new CoreAccountId("gaia-id-test"), accountEmail,
                        "gaia-id-test", "full name", "given name", null));

        mAccountManagerTestRule.addAccount(
                new ProfileDataSource.ProfileData(accountEmail, null, "Full Name", "Given Name"));
        mIdentityManager.onExtendedAccountInfoUpdated(
                new AccountInfo(new CoreAccountId("gaia-id-test"), accountEmail, "gaia-id-test",
                        "full name", "given name", createAvatar()));
        TestThreadUtils.runOnUiThreadBlocking(() -> { checkImageIsScaled(accountEmail); });
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
        String accountName = "test@example.com";
        ProfileDataSource.ProfileData profileData = new ProfileDataSource.ProfileData(
                accountName, createAvatar(), "Full Name", "Given Name");
        mAccountManagerTestRule.addAccount(profileData);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            checkImageIsScaled(accountName);
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
