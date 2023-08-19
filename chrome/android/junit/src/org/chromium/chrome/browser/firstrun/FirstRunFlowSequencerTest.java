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

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
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
        private final boolean mIsSyncAllowed;
        private final boolean mShouldShowSearchEnginePage;

        TestFirstRunFlowSequencerDelegate(OneshotSupplier<Profile> profileSupplier,
                boolean isSyncAllowed, boolean shouldShowSearchEnginePage) {
            super(profileSupplier);
            mIsSyncAllowed = isSyncAllowed;
            mShouldShowSearchEnginePage = shouldShowSearchEnginePage;
        }

        @Override
        public boolean shouldShowSearchEnginePage() {
            return mShouldShowSearchEnginePage;
        }

        @Override
        public boolean isSyncAllowed() {
            return mIsSyncAllowed;
        }
    }

    private static class TestFirstRunFlowSequencer extends FirstRunFlowSequencer {
        public Bundle returnedBundle;
        public boolean calledOnFlowIsKnown;

        public TestFirstRunFlowSequencer(
                Activity activity, OneshotSupplier<Profile> profileSupplier) {
            super(activity, profileSupplier,
                    new ChildAccountStatusSupplier(AccountManagerFacadeProvider.getInstance(),
                            FirstRunAppRestrictionInfo.takeMaybeInitialized()));
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
    private OneshotSupplierImpl<Profile> mProfileSupplier;

    @Before
    public void setUp() {
        Profile profile = mock(Profile.class);
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        when(IdentityServicesProvider.get().getIdentityManager(profile))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(false);
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SYNC)).thenReturn(false);

        mActivityController = Robolectric.buildActivity(Activity.class);
        mActivity = mActivityController.setup().get();
        mProfileSupplier = new OneshotSupplierImpl<>();
        mProfileSupplier.set(profile);
    }

    @After
    public void tearDown() {
        mActivityController.pause().stop().destroy();
    }

    private void setDelegateFactory(boolean isSyncAllowed, boolean shouldShowSearchEnginePage) {
        FirstRunFlowSequencer.setDelegateFactoryForTesting((profileSupplier) -> {
            return new TestFirstRunFlowSequencerDelegate(
                    profileSupplier, isSyncAllowed, shouldShowSearchEnginePage);
        });
    }

    @Test
    @Feature({"FirstRun"})
    public void testFlowOneChildAccount() {
        mAccountManagerTestRule.addAccount(CHILD_ACCOUNT_NAME);
        setDelegateFactory(true, false);
        HistogramWatcher numberOfAccountsHistogram = HistogramWatcher.newSingleRecordWatcher(
                "Signin.AndroidDeviceAccountsNumberWhenEnteringFRE", 1);

        TestFirstRunFlowSequencer sequencer =
                new TestFirstRunFlowSequencer(mActivity, mProfileSupplier);
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
        setDelegateFactory(true, true);
        HistogramWatcher numberOfAccountsHistogram = HistogramWatcher.newSingleRecordWatcher(
                "Signin.AndroidDeviceAccountsNumberWhenEnteringFRE", 0);

        TestFirstRunFlowSequencer sequencer =
                new TestFirstRunFlowSequencer(mActivity, mProfileSupplier);
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
        setDelegateFactory(true, false);
        HistogramWatcher numberOfAccountsHistogram = HistogramWatcher.newSingleRecordWatcher(
                "Signin.AndroidDeviceAccountsNumberWhenEnteringFRE", 0);

        TestFirstRunFlowSequencer sequencer =
                new TestFirstRunFlowSequencer(mActivity, mProfileSupplier);
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
        setDelegateFactory(true, false);
        HistogramWatcher numberOfAccountsHistogram = HistogramWatcher.newSingleRecordWatcher(
                "Signin.AndroidDeviceAccountsNumberWhenEnteringFRE", 1);

        TestFirstRunFlowSequencer sequencer =
                new TestFirstRunFlowSequencer(mActivity, mProfileSupplier);
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
