// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.support.test.InstrumentationRegistry;
import android.view.View;

import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtilsJni;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

/**
 * Render tests for {@link SignOutDialogFragment}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class SignOutDialogRenderTest extends DummyUiActivityTestCase {
    private static final String TEST_DOMAIN = "test.domain.example.com";

    @Rule
    public final DisableAnimationsTestRule mNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

    @Rule
    public final JniMocker mocker = new JniMocker();

    @Mock
    private SigninMetricsUtils.Natives mSigninMetricsUtilsNativeMock;

    @Mock
    private SigninManager mSigninManagerMock;

    @Mock
    private Profile mProfile;

    private SignOutDialogFragment mSignOutDialog;

    @Before
    public void setUp() {
        initMocks(this);
        mocker.mock(SigninMetricsUtilsJni.TEST_HOOKS, mSigninMetricsUtilsNativeMock);
        Profile.setLastUsedProfileForTesting(mProfile);
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
    }

    @After
    public void tearDown() {
        // Since the Dialog dismiss calls native method, we need to close the dialog before the
        // Native mock SigninMetricsUtils.Natives gets removed.
        if (mSignOutDialog != null) {
            TestThreadUtils.runOnUiThreadBlocking(() -> mSignOutDialog.dismiss());
        }
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSignOutDialogForManagedAccount() throws Exception {
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);
        mRenderTestRule.render(showSignOutDialog(), "signout_dialog_for_managed_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSignOutDialogForNonManagedAccount() throws Exception {
        mRenderTestRule.render(showSignOutDialog(), "signout_dialog_for_non_managed_account");
    }

    private View showSignOutDialog() {
        mSignOutDialog = SignOutDialogFragment.create(GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
        mSignOutDialog.show(getActivity().getSupportFragmentManager(), null);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        return mSignOutDialog.getDialog().getWindow().getDecorView();
    }
}
