// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.text.TextUtils;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.DummyUiChromeActivityTestCase;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.test.util.FakeAccountInfoService;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.widget.ChromeImageView;

import java.io.IOException;

/**
 * Tests for ProfileDataCache with a badge. Leverages RenderTest instead of reimplementing
 * bitmap comparison to simplify access to the compared images on buildbots (via result_details).
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(ProfileDataCacheRenderTest.PROFILE_DATA_BATCH_NAME)
@Features.EnableFeatures({ChromeFeatureList.DEPRECATE_MENAGERIE_API})
public class ProfileDataCacheWithBadgeRenderTest extends DummyUiChromeActivityTestCase {
    private static final String TEST_ACCOUNT_NAME = "test@example.com";

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

    @Rule
    public final Features.JUnitProcessor mProcessor = new Features.JUnitProcessor();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(new FakeAccountInfoService());

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock
    private ProfileDataCache.Observer mObserver;

    private FrameLayout mContentView;
    private ImageView mImageView;
    private ProfileDataCache mProfileDataCache;

    @Before
    public void setUp() {
        mAccountManagerTestRule.addAccount(
                TEST_ACCOUNT_NAME, "Full Name", "Given Name", createAvatar());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Activity activity = getActivity();
            mContentView = new FrameLayout(activity);
            mImageView = new ChromeImageView(activity);
            mContentView.addView(mImageView, ViewGroup.LayoutParams.WRAP_CONTENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT);
            activity.setContentView(mContentView);
        });
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
        });
        CriteriaHelper.pollUiThread(() -> {
            return !TextUtils.isEmpty(
                    mProfileDataCache.getProfileDataOrDefault(TEST_ACCOUNT_NAME).getFullName());
        });
        TestThreadUtils.runOnUiThreadBlocking(() -> {
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
