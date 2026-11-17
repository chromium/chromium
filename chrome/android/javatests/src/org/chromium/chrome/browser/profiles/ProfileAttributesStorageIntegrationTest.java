// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.signin.test.util.TestAccounts;

/** Tests {@link ProfileAttributesStorage} with native. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class ProfileAttributesStorageIntegrationTest {
    @Rule(order = 0)
    public final SigninTestRule mSigninTestRule = SigninTestRule.createWithCleanups();

    @Rule(order = 1)
    public final AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private final CallbackHelper mAuthInfoChangedCallbackHelper = new CallbackHelper();
    private @Nullable String mAuthInfoChangedProfilePath;
    private final ProfileAttributesStorage.Observer mObserver =
            new ProfileAttributesStorage.Observer() {
                @Override
                public void onProfileAuthInfoChanged(String profilePath) {
                    mAuthInfoChangedProfilePath = profilePath;
                    mAuthInfoChangedCallbackHelper.notifyCalled();
                }
            };

    @Before
    public void setUp() {
        mActivityTestRule.startOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
                () -> ProfileAttributesStorage.get().addObserver(mObserver));
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> ProfileAttributesStorage.get().removeObserver(mObserver));
    }

    @Test
    @MediumTest
    public void testSigninAndSignoutUpdateGaiaId() throws Exception {
        int callCount = mAuthInfoChangedCallbackHelper.getCallCount();
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        mAuthInfoChangedCallbackHelper.waitForCallback(callCount);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var entries = ProfileAttributesStorage.get().getAllProfilesAttributes();
                    assertEquals(1, entries.size());
                    assertEquals(TestAccounts.ACCOUNT1.getGaiaId(), entries.get(0).getGaiaId());
                    assertEquals(entries.get(0).getPath(), mAuthInfoChangedProfilePath);
                });

        callCount = mAuthInfoChangedCallbackHelper.getCallCount();
        mSigninTestRule.signOut();
        mAuthInfoChangedCallbackHelper.waitForCallback(callCount);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        assertTrue(
                                ProfileAttributesStorage.get()
                                        .getPrimaryAccountIdsForAllProfiles()
                                        .isEmpty()));
    }
}
