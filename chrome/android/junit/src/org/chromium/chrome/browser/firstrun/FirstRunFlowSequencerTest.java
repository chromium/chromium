// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.os.Bundle;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;

/**
 * Tests FirstRunFlowSequencer which contains the core logic of what should be shown during the
 * first run.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class FirstRunFlowSequencerTest {
    private static final String ADULT_ACCOUNT_NAME = "adult.account@gmail.com";
    private static final String CHILD_ACCOUNT_NAME =
            AccountManagerTestRule.generateChildEmail(/*baseName=*/"account@gmail.com");

    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    /**
     * Testing version of FirstRunFlowSequencer that allows us to override all needed checks.
     */
    private static class TestFirstRunFlowSequencerDelegate
            extends FirstRunFlowSequencer.FirstRunFlowSequencerDelegate {
        public boolean isSyncAllowed;
        public boolean shouldSkipFirstUseHints;
        public boolean shouldShowSearchEnginePage;

        @Override
        public boolean shouldShowSearchEnginePage() {
            return shouldShowSearchEnginePage;
        }

        @Override
        public boolean isSyncAllowed() {
            return isSyncAllowed;
        }
    }

    private static class TestFirstRunFlowSequencer extends FirstRunFlowSequencer {
        public Bundle returnedBundle;
        public boolean calledOnFlowIsKnown;

        public TestFirstRunFlowSequencer(Activity activity) {
            super(activity,
                    new ChildAccountStatusSupplier(
                            AccountManagerFacadeProvider.getInstance(), null));
        }

        @Override
        public void onFlowIsKnown(Bundle freProperties) {
            calledOnFlowIsKnown = true;
            if (freProperties != null) updateFirstRunProperties(freProperties);
            returnedBundle = freProperties;
        }
    }

    @Mock
    private IdentityManager mIdentityManagerMock;

    private ActivityController<Activity> mActivityController;
    private Activity mActivity;
    private TestFirstRunFlowSequencerDelegate mDelegate;

    @Before
    public void setUp() {
        Profile.setLastUsedProfileForTesting(mock(Profile.class));
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        when(IdentityServicesProvider.get().getIdentityManager(Profile.getLastUsedRegularProfile()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(false);
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SYNC)).thenReturn(false);

        mActivityController = Robolectric.buildActivity(Activity.class);
        mActivity = mActivityController.setup().get();
        mDelegate = new TestFirstRunFlowSequencerDelegate();
        FirstRunFlowSequencer.setDelegateForTesting(mDelegate);
    }

    @After
    public void tearDown() {
        mActivityController.pause().stop().destroy();
        FirstRunFlowSequencer.setDelegateForTesting(null);
    }

    @Test
    @Feature({"FirstRun"})
    public void testFlowOneChildAccount() {
        mAccountManagerTestRule.addAccount(CHILD_ACCOUNT_NAME);
        mDelegate.isSyncAllowed = true;
        HistogramWatcher numberOfAccountsHistogram = HistogramWatcher.newSingleRecordWatcher(
                "Signin.AndroidDeviceAccountsNumberWhenEnteringFRE", 1);

        TestFirstRunFlowSequencer sequencer = new TestFirstRunFlowSequencer(mActivity);
        sequencer.start();

        numberOfAccountsHistogram.assertExpected();
        assertTrue(sequencer.calledOnFlowIsKnown);

        Bundle bundle = sequencer.returnedBundle;
        assertTrue(bundle.getBoolean(FirstRunActivityBase.SHOW_SYNC_CONSENT_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        assertTrue(bundle.getBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT));
        assertEquals(3, bundle.size());
    }

    @Test
    @Feature({"FirstRun"})
    public void testFlowShowSearchEnginePage() {
        mDelegate.isSyncAllowed = true;
        mDelegate.shouldShowSearchEnginePage = true;
        HistogramWatcher numberOfAccountsHistogram = HistogramWatcher.newSingleRecordWatcher(
                "Signin.AndroidDeviceAccountsNumberWhenEnteringFRE", 0);

        TestFirstRunFlowSequencer sequencer = new TestFirstRunFlowSequencer(mActivity);
        sequencer.start();

        numberOfAccountsHistogram.assertExpected();
        assertTrue(sequencer.calledOnFlowIsKnown);

        Bundle bundle = sequencer.returnedBundle;
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SYNC_CONSENT_PAGE));
        assertTrue(bundle.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        assertFalse(bundle.getBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT));
        assertEquals(3, bundle.size());
    }

    @Test
    @Feature({"FirstRun"})
    public void testFlowHideSyncConsentPageWhenUserIsNotSignedIn() {
        mDelegate.isSyncAllowed = true;
        mDelegate.shouldShowSearchEnginePage = false;
        HistogramWatcher numberOfAccountsHistogram = HistogramWatcher.newSingleRecordWatcher(
                "Signin.AndroidDeviceAccountsNumberWhenEnteringFRE", 0);

        TestFirstRunFlowSequencer sequencer = new TestFirstRunFlowSequencer(mActivity);
        sequencer.start();

        numberOfAccountsHistogram.assertExpected();
        assertTrue(sequencer.calledOnFlowIsKnown);
        final Bundle bundle = sequencer.returnedBundle;
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SYNC_CONSENT_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        assertFalse(bundle.getBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT));
        assertEquals(3, bundle.size());
    }

    @Test
    @Feature({"FirstRun"})
    public void testFlowShowSyncConsentPageWhenUserIsSignedIn() {
        mAccountManagerTestRule.addAccount(ADULT_ACCOUNT_NAME);
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(true);
        mDelegate.isSyncAllowed = true;
        mDelegate.shouldShowSearchEnginePage = false;
        HistogramWatcher numberOfAccountsHistogram = HistogramWatcher.newSingleRecordWatcher(
                "Signin.AndroidDeviceAccountsNumberWhenEnteringFRE", 1);

        TestFirstRunFlowSequencer sequencer = new TestFirstRunFlowSequencer(mActivity);
        sequencer.start();

        numberOfAccountsHistogram.assertExpected();
        assertTrue(sequencer.calledOnFlowIsKnown);
        final Bundle bundle = sequencer.returnedBundle;
        assertTrue(bundle.getBoolean(FirstRunActivityBase.SHOW_SYNC_CONSENT_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        assertFalse(bundle.getBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT));
        assertEquals(3, bundle.size());
    }
}
