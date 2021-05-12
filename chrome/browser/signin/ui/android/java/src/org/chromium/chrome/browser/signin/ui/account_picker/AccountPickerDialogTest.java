// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui.account_picker;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.Mockito.verify;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.signin.ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.ProfileDataSource;
import org.chromium.components.signin.identitymanager.AccountInfoService;
import org.chromium.components.signin.identitymanager.AccountTrackerService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.FakeProfileDataSource;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

import java.io.IOException;

/**
 * Instrumentation tests for account picker dialog.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableFeatures(
        {ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY, ChromeFeatureList.DEPRECATE_MENAGERIE_API})
@Batch(Batch.PER_CLASS)
public class AccountPickerDialogTest extends DummyUiActivityTestCase {
    @Rule
    public final Features.JUnitProcessor mProcessor = new Features.JUnitProcessor();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(new FakeProfileDataSource());

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock
    private AccountTrackerService mAccountTrackerServiceMock;

    @Mock
    private IdentityManager mIdentityManagerMock;

    @Mock
    private AccountPickerCoordinator.Listener mListenerMock;

    private final String mFullName1 = "Test Account1";

    private final String mAccountName1 = "test.account1@gmail.com";

    private final String mAccountName2 = "test.account2@gmail.com";

    private AccountPickerDialogCoordinator mCoordinator;

    @Before
    public void setUp() {
        AccountInfoService.init(mIdentityManagerMock, mAccountTrackerServiceMock);
        addAccount(mAccountName1, mFullName1);
        addAccount(mAccountName2, "");
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mCoordinator = new AccountPickerDialogCoordinator(getActivity(), mListenerMock);
        });
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(mCoordinator::dismissDialog);
        AccountInfoService.resetForTests();
    }

    @Test
    @MediumTest
    public void testTitle() {
        onView(withText(R.string.signin_account_picker_dialog_title)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testAddAccount() {
        onView(withText(R.string.signin_add_account)).perform(click());
        verify(mListenerMock).addAccount();
    }

    @Test
    @MediumTest
    public void testSelectDefaultAccount() {
        onView(withText(mAccountName1)).check(matches(isDisplayed()));
        onView(withText(mFullName1)).perform(click());
        verify(mListenerMock).onAccountSelected(mAccountName1, true);
    }

    @Test
    @MediumTest
    public void testSelectNonDefaultAccount() {
        onView(withText(mAccountName2)).perform(click());
        verify(mListenerMock).onAccountSelected(mAccountName2, false);
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testAccountPickerDialogViewLegacy() throws IOException {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mRenderTestRule.render(
                mCoordinator.getAccountPickerViewForTests(), "account_picker_dialog_legacy");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @Features.EnableFeatures({ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY})
    public void testAccountPickerDialogView() throws IOException {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mRenderTestRule.render(
                mCoordinator.getAccountPickerViewForTests(), "account_picker_dialog");
    }

    private void addAccount(String accountName, String fullName) {
        ProfileDataSource.ProfileData profileData =
                new ProfileDataSource.ProfileData(accountName, null, fullName, null);
        mAccountManagerTestRule.addAccount(profileData);
    }
}
