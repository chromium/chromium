// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.ProfileDataSource;
import org.chromium.components.signin.identitymanager.AccountInfoService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.FakeProfileDataSource;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DummyUiActivityTestCase;
import org.chromium.ui.widget.ChromeImageView;

import java.io.IOException;

/**
 * Tests for ProfileDataCache with a badge. Leverages RenderTest instead of reimplementing
 * bitmap comparison to simplify access to the compared images on buildbots (via result_details).
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(ProfileDataCacheRenderTest.PROFILE_DATA_BATCH_NAME)
@DisableFeatures({ChromeFeatureList.DEPRECATE_MENAGERIE_API})
public class ProfileDataCacheWithBadgeRenderTest extends DummyUiActivityTestCase {
    private static final long NATIVE_IDENTITY_MANAGER = 10002L;

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

    @Rule
    public final Features.JUnitProcessor mProcessor = new Features.JUnitProcessor();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(new FakeProfileDataSource());

    @Mock
    private ProfileDataCache.Observer mObserver;

    private static final String TEST_ACCOUNT_NAME = "test@example.com";

    private final IdentityManager mIdentityManager =
            IdentityManager.create(NATIVE_IDENTITY_MANAGER, null /* OAuth2TokenService */);

    private FrameLayout mContentView;
    private ImageView mImageView;
    private ProfileDataCache mProfileDataCache;

    @Before
    public void setUp() {
        initMocks(this);
        AccountInfoService.init(mIdentityManager);
        final ProfileDataSource.ProfileData profileData = new ProfileDataSource.ProfileData(
                TEST_ACCOUNT_NAME, createAvatar(), "Full Name", "Given Name");
        mAccountManagerTestRule.addAccount(profileData);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Activity activity = getActivity();
            mContentView = new FrameLayout(activity);
            mImageView = new ChromeImageView(activity);
            mContentView.addView(mImageView, ViewGroup.LayoutParams.WRAP_CONTENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT);
            activity.setContentView(mContentView);
        });
    }

    @After
    public void tearDown() {
        AccountInfoService.resetForTests();
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testProfileDataWithChildBadge() throws IOException {
        setUpProfileDataCache(true);
        mRenderTestRule.render(mImageView, "profile_data_cache_with_child_badge");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testProfileDataWithoutBadge() throws IOException {
        setUpProfileDataCache(false);
        mRenderTestRule.render(mImageView, "profile_data_cache_without_badge");
    }

    private void setUpProfileDataCache(boolean withBadge) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mProfileDataCache = withBadge
                    ? ProfileDataCache.createWithDefaultImageSize(
                            getActivity(), R.drawable.ic_account_child_20dp)
                    : ProfileDataCache.createWithoutBadge(getActivity(), R.dimen.user_picture_size);
            // ProfileDataCache only populates the cache when an observer is added.
            mProfileDataCache.addObserver(mObserver);
            mImageView.setImageDrawable(
                    mProfileDataCache.getProfileDataOrDefault(TEST_ACCOUNT_NAME).getImage());
        });
    }

    /**
     * Creates a simple dummy bitmap to use as the avatar picture.
     */
    private Bitmap createAvatar() {
        final int avatarSize =
                getActivity().getResources().getDimensionPixelSize(R.dimen.user_picture_size);
        Assert.assertTrue("avatarSize must not be 0", avatarSize > 0);
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
