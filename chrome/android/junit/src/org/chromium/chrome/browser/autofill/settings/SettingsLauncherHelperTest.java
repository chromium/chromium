// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.content.Context;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.device_lock.DeviceLockActivityLauncherImpl;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.AutomotiveContextWrapperTestRule;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.ui.base.WindowAndroid;

import java.util.concurrent.atomic.AtomicReference;

/**
 * Test for {@link SettingsLauncherHelper}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Uses static launcher.")
public class SettingsLauncherHelperTest {
    @Rule
    public AutomotiveContextWrapperTestRule mAutomotiveContextWrapperTestRule =
            new AutomotiveContextWrapperTestRule();

    @Mock
    private SettingsLauncher mMockLauncher;
    @Mock
    private Context mMockContext;
    @Mock
    private WindowAndroid mWindowAndroid;
    @Mock
    private DeviceLockActivityLauncherImpl mDeviceLockActivityLauncherImpl;

    private UserActionTester mActionTester = new UserActionTester();

    @Before
    public void setup() {
        MockitoAnnotations.openMocks(this);
        SettingsLauncherHelper.setLauncher(mMockLauncher);
        DeviceLockActivityLauncherImpl.setInstanceForTesting(mDeviceLockActivityLauncherImpl);
    }

    @After
    public void tearDown() {
        SettingsLauncherHelper.setLauncher(null);
        mActionTester.tearDown();
    }

    @Test
    @SmallTest
    public void testRecordsActionThenLaunchesPaymentsSettings() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(false);

        assertTrue(SettingsLauncherHelper.showAutofillCreditCardSettings(mMockContext, null));
        assertTrue(mActionTester.getActions().contains("AutofillCreditCardsViewed"));
        verify(mMockLauncher)
                .launchSettingsActivity(mMockContext, AutofillPaymentMethodsFragment.class);
    }

    @Test
    @SmallTest
    public void testRecordsActionThenLaunchesAddressesSettings() {
        assertTrue(SettingsLauncherHelper.showAutofillProfileSettings(mMockContext));
        assertTrue(mActionTester.getActions().contains("AutofillAddressesViewed"));
        verify(mMockLauncher).launchSettingsActivity(mMockContext, AutofillProfilesFragment.class);
    }

    @Test
    @SmallTest
    public void testDoesntLaunchOrRecordPaymentsSettingsWithoutContext() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(false);

        assertFalse(SettingsLauncherHelper.showAutofillCreditCardSettings(null, null));
        assertFalse(mActionTester.getActions().contains("AutofillCreditCardsViewed"));
        verifyNoInteractions(mMockLauncher);
    }

    @Test
    @SmallTest
    public void testDoesntLaunchOrRecordAddressesSettingsWithoutContext() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(false);

        assertFalse(SettingsLauncherHelper.showAutofillCreditCardSettings(null, null));
        assertFalse(mActionTester.getActions().contains("AutofillAddressesViewed"));
        verifyNoInteractions(mMockLauncher);
    }

    @Test
    @SmallTest
    public void testShowAutofillCreditCardSettings_onAutomotiveDevice_ensuresDeviceLockSecure() {
        mAutomotiveContextWrapperTestRule.setIsAutomotive(true);
        AtomicReference<Runnable> callback = new AtomicReference<>();

        doAnswer((invocation) -> {
            Runnable ensureDeviceLockSecureCallback = invocation.getArgument(2);
            callback.set(ensureDeviceLockSecureCallback);
            return null;
        })
                .when(mDeviceLockActivityLauncherImpl)
                .presentDeviceLockChallenge(any(), any(), any());

        SettingsLauncherHelper.showAutofillCreditCardSettings(mMockContext, mWindowAndroid);
        verify(mMockLauncher, never())
                .launchSettingsActivity(mMockContext, AutofillPaymentMethodsFragment.class);
        verify(mDeviceLockActivityLauncherImpl, times(1))
                .presentDeviceLockChallenge(eq(mMockContext), eq(mWindowAndroid), any());

        callback.get().run();
        verify(mMockLauncher, times(1))
                .launchSettingsActivity(mMockContext, AutofillPaymentMethodsFragment.class);
    }
}
