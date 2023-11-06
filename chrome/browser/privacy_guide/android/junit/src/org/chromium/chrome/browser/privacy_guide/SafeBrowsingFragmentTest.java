// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Bundle;
import android.view.View;

import androidx.fragment.app.testing.FragmentScenario;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridgeJni;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionAndAuxButton;

/** JUnit tests of the class {@link SafeBrowsingFragment} */
@RunWith(BaseRobolectricTestRunner.class)
public class SafeBrowsingFragmentTest {
    // TODO(crbug.com/1357003): Use Espresso for view interactions.
    @Rule public JniMocker mMocker = new JniMocker();
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock private SafeBrowsingBridge.Natives mNativeMock;
    @Mock private OneshotSupplierImpl<BottomSheetController> mBottomSheetControllerSupplier;

    private FragmentScenario mScenario;
    private RadioButtonWithDescriptionAndAuxButton mEnhancedProtectionButton;
    private RadioButtonWithDescriptionAndAuxButton mStandardProtectionButton;
    private RadioButtonWithDescription mStandardProtectionButtonFriendlier;
    private String mFriendlierESBDescription;
    private String mOriginalESBDescription;
    private final UserActionTester mActionTester = new UserActionTester();

    @Before
    public void setUp() {
        mMocker.mock(SafeBrowsingBridgeJni.TEST_HOOKS, mNativeMock);
    }

    @After
    public void tearDown() {
        if (mScenario != null) {
            mScenario.close();
        }
        mActionTester.tearDown();
    }

    private void initFragmentWithSBState(@SafeBrowsingState int state) {
        when(mNativeMock.getSafeBrowsingState()).thenReturn(state);
        mScenario =
                FragmentScenario.launchInContainer(
                        SafeBrowsingFragment.class, Bundle.EMPTY, R.style.Theme_MaterialComponents);
        mScenario.onFragment(
                fragment -> {
                    mEnhancedProtectionButton =
                            fragment.getView().findViewById(R.id.enhanced_option);
                    mStandardProtectionButtonFriendlier =
                            fragment.getView().findViewById(R.id.standard_option_friendlier);
                    mStandardProtectionButton =
                            fragment.getView().findViewById(R.id.standard_option);
                    ((SafeBrowsingFragment) fragment)
                            .setBottomSheetControllerSupplier(mBottomSheetControllerSupplier);
                    mFriendlierESBDescription =
                            fragment.getContext()
                                    .getString(
                                            R.string
                                                    .safe_browsing_enhanced_protection_summary_updated);
                    mOriginalESBDescription =
                            fragment.getContext()
                                    .getString(
                                            R.string
                                                    .privacy_guide_safe_browsing_enhanced_description);
                });
    }

    @Test
    public void testInitWhenSBEnhanced() {
        initFragmentWithSBState(SafeBrowsingState.ENHANCED_PROTECTION);
        assertTrue(mEnhancedProtectionButton.isChecked());
        assertFalse(mStandardProtectionButton.isChecked());
    }

    @Test
    public void testInitWhenSBStandard() {
        initFragmentWithSBState(SafeBrowsingState.STANDARD_PROTECTION);
        assertFalse(mEnhancedProtectionButton.isChecked());
        assertTrue(mStandardProtectionButton.isChecked());
    }

    @Test(expected = AssertionError.class)
    public void testInitWhenSBOff() {
        initFragmentWithSBState(SafeBrowsingState.NO_SAFE_BROWSING);
    }

    @Test
    public void testSelectEnhanced() {
        initFragmentWithSBState(SafeBrowsingState.STANDARD_PROTECTION);
        mEnhancedProtectionButton.performClick();
        verify(mNativeMock).setSafeBrowsingState(SafeBrowsingState.ENHANCED_PROTECTION);
    }

    @Test
    public void testSelectStandard() {
        initFragmentWithSBState(SafeBrowsingState.ENHANCED_PROTECTION);
        mStandardProtectionButton.performClick();
        verify(mNativeMock).setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
    }

    @Test
    public void testSelectEnhanced_changeSafeBrowsingEnhancedUserAction() {
        initFragmentWithSBState(SafeBrowsingState.STANDARD_PROTECTION);
        mEnhancedProtectionButton.performClick();
        assertTrue(
                mActionTester
                        .getActions()
                        .contains("Settings.PrivacyGuide.ChangeSafeBrowsingEnhanced"));
    }

    @Test
    public void testSelectStandard_changeSafeBrowsingStandardUserAction() {
        initFragmentWithSBState(SafeBrowsingState.ENHANCED_PROTECTION);
        mStandardProtectionButton.performClick();
        assertTrue(
                mActionTester
                        .getActions()
                        .contains("Settings.PrivacyGuide.ChangeSafeBrowsingStandard"));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.FRIENDLIER_SAFE_BROWSING_SETTINGS_STANDARD_PROTECTION)
    public void testIsSBStandardFriendlierVisibleWhenInit() {
        initFragmentWithSBState(SafeBrowsingState.STANDARD_PROTECTION);
        assertEquals(View.GONE, mStandardProtectionButton.getVisibility());
        assertFalse(mStandardProtectionButton.isChecked());
        assertEquals(View.VISIBLE, mStandardProtectionButtonFriendlier.getVisibility());
        assertTrue(mStandardProtectionButtonFriendlier.isChecked());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.FRIENDLIER_SAFE_BROWSING_SETTINGS_STANDARD_PROTECTION)
    public void testSelectStandardFriendlierFromEnhanced() {
        initFragmentWithSBState(SafeBrowsingState.ENHANCED_PROTECTION);
        assertFalse(mStandardProtectionButtonFriendlier.isChecked());
        mStandardProtectionButtonFriendlier.performClick();
        assertTrue(mStandardProtectionButtonFriendlier.isChecked());
        verify(mNativeMock).setSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.FRIENDLIER_SAFE_BROWSING_SETTINGS_STANDARD_PROTECTION)
    public void testInitWhenSBStandardAndFriendlierIsOff() {
        initFragmentWithSBState(SafeBrowsingState.STANDARD_PROTECTION);
        assertFalse(mEnhancedProtectionButton.isChecked());

        assertEquals(View.VISIBLE, mStandardProtectionButton.getVisibility());
        assertTrue(mStandardProtectionButton.isChecked());
        assertEquals(View.GONE, mStandardProtectionButtonFriendlier.getVisibility());
        assertFalse(mStandardProtectionButtonFriendlier.isChecked());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.FRIENDLIER_SAFE_BROWSING_SETTINGS_STANDARD_PROTECTION)
    public void testDescriptionTextWhenHashRealTimeDisabled() {
        when(mNativeMock.isHashRealTimeLookupEligibleInSession()).thenReturn(false);
        initFragmentWithSBState(SafeBrowsingState.STANDARD_PROTECTION);
        assertEquals(
                ContextUtils.getApplicationContext()
                        .getString(R.string.safe_browsing_standard_protection_summary_updated),
                mStandardProtectionButtonFriendlier.getDescriptionText());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.FRIENDLIER_SAFE_BROWSING_SETTINGS_STANDARD_PROTECTION})
    public void testDescriptionTextWhenHashRealTimeEnabled() {
        when(mNativeMock.isHashRealTimeLookupEligibleInSession()).thenReturn(true);
        initFragmentWithSBState(SafeBrowsingState.STANDARD_PROTECTION);
        assertEquals(
                ContextUtils.getApplicationContext()
                        .getString(
                                R.string.safe_browsing_standard_protection_summary_updated_proxy),
                mStandardProtectionButtonFriendlier.getDescriptionText());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.FRIENDLIER_SAFE_BROWSING_SETTINGS_ENHANCED_PROTECTION)
    public void testUpdatedDescriptionEnhancedProtection() {
        initFragmentWithSBState(SafeBrowsingState.ENHANCED_PROTECTION);
        assertEquals(mFriendlierESBDescription, mEnhancedProtectionButton.getDescriptionText());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.FRIENDLIER_SAFE_BROWSING_SETTINGS_ENHANCED_PROTECTION)
    public void testOriginalDescriptionEnhancedProtection() {
        initFragmentWithSBState(SafeBrowsingState.ENHANCED_PROTECTION);
        assertEquals(mOriginalESBDescription, mEnhancedProtectionButton.getDescriptionText());
    }
}
