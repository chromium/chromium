// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.content.Context;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.settings.SettingsNavigation;

/** Test for {@link SettingsNavigationHelper}. */
@RunWith(RobolectricTestRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Uses static launcher.")
public class SettingsNavigationHelperTest {
    @Mock private SettingsNavigation mMockLauncher;
    @Mock private Context mMockContext;

    private UserActionTester mActionTester = new UserActionTester();

    @Before
    public void setup() {
        MockitoAnnotations.openMocks(this);
        SettingsNavigationFactory.setInstanceForTesting(mMockLauncher);
    }

    @After
    public void tearDown() {
        mActionTester.tearDown();
    }

    @Test
    @SmallTest
    public void testRecordsActionThenLaunchesPaymentsSettings() {
        assertTrue(SettingsNavigationHelper.showAutofillCreditCardSettings(mMockContext));
        assertTrue(mActionTester.getActions().contains("AutofillCreditCardsViewed"));
        verify(mMockLauncher).startSettings(mMockContext, AutofillPaymentMethodsFragment.class);
    }

    @Test
    @SmallTest
    public void testRecordsActionThenLaunchesAddressesSettings() {
        assertTrue(SettingsNavigationHelper.showAutofillProfileSettings(mMockContext));
        assertTrue(mActionTester.getActions().contains("AutofillAddressesViewed"));
        verify(mMockLauncher).startSettings(mMockContext, AutofillProfilesFragment.class);
    }

    @Test
    @SmallTest
    public void testDoesntLaunchOrRecordPaymentsSettingsWithoutContext() {
        assertFalse(SettingsNavigationHelper.showAutofillCreditCardSettings(null));
        assertFalse(mActionTester.getActions().contains("AutofillCreditCardsViewed"));
        verifyNoInteractions(mMockLauncher);
    }

    @Test
    @SmallTest
    public void testDoesntLaunchOrRecordAddressesSettingsWithoutContext() {
        assertFalse(SettingsNavigationHelper.showAutofillCreditCardSettings(null));
        assertFalse(mActionTester.getActions().contains("AutofillAddressesViewed"));
        verifyNoInteractions(mMockLauncher);
    }
}
