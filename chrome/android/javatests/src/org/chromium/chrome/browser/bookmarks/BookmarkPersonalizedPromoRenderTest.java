// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.signin.SigninPromoController.SyncPromoState;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.BookmarkTestRule;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.UiDisableIf;

// TODO(crbug.com/1334912): This class should be removed, since we have a similar tests in
// SigninPromoControllerRenderTest.

/**
 * Tests for the personalized signin promo on the Bookmarks page.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableIf.Device(type = {UiDisableIf.TABLET})
public class BookmarkPersonalizedPromoRenderTest {
    private final SigninTestRule mSigninTestRule = new SigninTestRule();

    private final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private final BookmarkTestRule mBookmarkTestRule = new BookmarkTestRule();

    // Bookmarks need the FakeAccountManagerFacade initialized in AccountManagerTestRule.
    @Rule
    public final RuleChain chain = RuleChain.outerRule(mSigninTestRule)
                                           .around(mActivityTestRule)
                                           .around(mBookmarkTestRule);

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(4)
                    .setBugComponent(ChromeRenderTestRule.Component.SERVICES_SIGN_IN)
                    .build();

    @ParameterAnnotations.UseMethodParameterBefore(NightModeTestUtils.NightModeParams.class)
    public void setupNightMode(boolean nightModeEnabled) {
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        ChromeNightModeTestUtils.setUpNightModeBeforeChromeActivityLaunched();
    }

    @Before
    public void setUp() {
        // Native side needs to loaded before signing in test account.
        mActivityTestRule.startMainActivityOnBlankPage();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BookmarkModel bookmarkModel = new BookmarkModel(Profile.getLastUsedRegularProfile());
            bookmarkModel.loadFakePartnerBookmarkShimForTesting();
        });
        BookmarkTestUtil.waitForBookmarkModelLoaded();
    }

    @After
    public void tearDown() {
        BookmarkPromoHeader.forcePromoStateForTests(null);
    }

    @AfterClass
    public static void tearDownAfterActivityDestroyed() {
        ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed();
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testPersonalizedSigninPromoInBookmarkPage(boolean nightModeEnabled)
            throws Exception {
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.PROMO_FOR_SIGNED_OUT_STATE);
        mBookmarkTestRule.showBookmarkManager(mActivityTestRule.getActivity());
        mRenderTestRule.render(getPersonalizedPromoView(), "bookmark_personalized_signin_promo");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testPersonalizedSyncPromoInBookmarkPage(boolean nightModeEnabled) throws Exception {
        mSigninTestRule.addTestAccountThenSignin();
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.PROMO_FOR_SIGNED_IN_STATE);
        mBookmarkTestRule.showBookmarkManager(mActivityTestRule.getActivity());
        mRenderTestRule.render(getPersonalizedPromoView(), "bookmark_personalized_sync_promo");
    }

    private View getPersonalizedPromoView() {
        BookmarkActivity bookmarkActivity = mBookmarkTestRule.getBookmarkActivity();
        Assert.assertNotNull("BookmarkActivity should not be null", bookmarkActivity);
        return bookmarkActivity.findViewById(R.id.signin_promo_view_wrapper);
    }
}
