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
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;

/**
 * Tests FirstRunFlowSequencer which contains the core logic of what should be shown during the
 * first run.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class FirstRunFlowSequencerTest {
    private static final String ADULT_ACCOUNT_NAME = "adult.account@gmail.com";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    /** Testing version of FirstRunFlowSequencer that allows us to override all needed checks. */
    private static class TestFirstRunFlowSequencerDelegate
            extends FirstRunFlowSequencer.FirstRunFlowSequencerDelegate {
        private final boolean mShouldShowSearchEnginePage;

        TestFirstRunFlowSequencerDelegate(
                OneshotSupplier<ProfileProvider> profileSupplier,
                boolean shouldShowSearchEnginePage) {
            super(profileSupplier);
            mShouldShowSearchEnginePage = shouldShowSearchEnginePage;
        }

        @Override
        public boolean shouldShowSearchEnginePage() {
            return mShouldShowSearchEnginePage;
        }
    }

    private static class TestFirstRunFlowSequencer extends FirstRunFlowSequencer {
        public Bundle returnedBundle;
        public boolean calledOnFlowIsKnown;

        public TestFirstRunFlowSequencer(
                Activity activity, OneshotSupplier<ProfileProvider> profileSupplier) {
            super(
                    profileSupplier,
                    new ChildAccountStatusSupplier(
                            AccountManagerFacadeProvider.getInstance(),
                            FirstRunAppRestrictionInfo.takeMaybeInitialized()));
        }

        @Override
        public void onFlowIsKnown(Bundle freProperties) {
            calledOnFlowIsKnown = true;
            if (freProperties != null) updateFirstRunProperties(freProperties);
            returnedBundle = freProperties;
        }
    }

    @Mock private IdentityManager mIdentityManagerMock;
    @Mock private SyncService mSyncServiceMock;
    @Mock private HistorySyncHelper mHistorySyncHelperMock;

    private ActivityController<Activity> mActivityController;
    private Activity mActivity;
    private OneshotSupplierImpl<ProfileProvider> mProfileSupplier;

    @Before
    public void setUp() {
        Profile profile = mock(Profile.class);
        ProfileProvider profileProvider = mock(ProfileProvider.class);

        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        when(IdentityServicesProvider.get().getIdentityManager(profile))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(false);
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SYNC)).thenReturn(false);

        SyncServiceFactory.setInstanceForTesting(mSyncServiceMock);
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelperMock);

        mActivityController = Robolectric.buildActivity(Activity.class);
        mActivity = mActivityController.setup().get();
        mProfileSupplier = new OneshotSupplierImpl<>();
        when(profileProvider.getOriginalProfile()).thenReturn(profile);
        mProfileSupplier.set(profileProvider);
    }

    @After
    public void tearDown() {
        mActivityController.pause().stop().destroy();
    }

    private void setDelegateFactory(boolean shouldShowSearchEnginePage) {
        FirstRunFlowSequencer.setDelegateFactoryForTesting(
                (profileSupplier) -> {
                    return new TestFirstRunFlowSequencerDelegate(
                            profileSupplier, shouldShowSearchEnginePage);
                });
    }

    @Test
    @Feature({"FirstRun"})
    @Features.DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testFlowOneChildAccount() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_CHILD_ACCOUNT);
        setDelegateFactory(false);
        HistogramWatcher numberOfAccountsHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.AndroidDeviceAccountsNumberWhenEnteringFRE", 1);

        TestFirstRunFlowSequencer sequencer =
                new TestFirstRunFlowSequencer(mActivity, mProfileSupplier);
        sequencer.start();

        numberOfAccountsHistogram.assertExpected();
        assertTrue(sequencer.calledOnFlowIsKnown);

        Bundle bundle = sequencer.returnedBundle;
        assertTrue(bundle.getBoolean(FirstRunActivityBase.SHOW_SYNC_CONSENT_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_HISTORY_SYNC_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        assertTrue(bundle.getBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT));
        assertEquals(4, bundle.size());
    }

    @Test
    @Feature({"FirstRun"})
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testFlowOneChildAccount_historySyncEnabled() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_CHILD_ACCOUNT);
        setDelegateFactory(false);
        HistogramWatcher numberOfAccountsHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.AndroidDeviceAccountsNumberWhenEnteringFRE", 1);

        TestFirstRunFlowSequencer sequencer =
                new TestFirstRunFlowSequencer(mActivity, mProfileSupplier);
        sequencer.start();

        numberOfAccountsHistogram.assertExpected();
        assertTrue(sequencer.calledOnFlowIsKnown);

        Bundle bundle = sequencer.returnedBundle;
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SYNC_CONSENT_PAGE));
        assertTrue(bundle.getBoolean(FirstRunActivityBase.SHOW_HISTORY_SYNC_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        assertTrue(bundle.getBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT));
        assertEquals(4, bundle.size());
    }

    @Test
    @Feature({"FirstRun"})
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testFlowOneChildAccount_historySyncManagedByCustodian_historySyncEnabled() {
        when(mHistorySyncHelperMock.isHistorySyncDisabledByCustodian()).thenReturn(true);
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_CHILD_ACCOUNT);
        setDelegateFactory(false);
        HistogramWatcher numberOfAccountsHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.AndroidDeviceAccountsNumberWhenEnteringFRE", 1);

        TestFirstRunFlowSequencer sequencer =
                new TestFirstRunFlowSequencer(mActivity, mProfileSupplier);
        sequencer.start();

        numberOfAccountsHistogram.assertExpected();
        assertTrue(sequencer.calledOnFlowIsKnown);

        Bundle bundle = sequencer.returnedBundle;
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SYNC_CONSENT_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_HISTORY_SYNC_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        assertTrue(bundle.getBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT));
        assertEquals(4, bundle.size());
    }

    @Test
    @Feature({"FirstRun"})
    @Features.DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testFlowShowSearchEnginePage() {
        setDelegateFactory(true);
        HistogramWatcher numberOfAccountsHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.AndroidDeviceAccountsNumberWhenEnteringFRE", 0);

        TestFirstRunFlowSequencer sequencer =
                new TestFirstRunFlowSequencer(mActivity, mProfileSupplier);
        sequencer.start();

        numberOfAccountsHistogram.assertExpected();
        assertTrue(sequencer.calledOnFlowIsKnown);

        Bundle bundle = sequencer.returnedBundle;
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SYNC_CONSENT_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_HISTORY_SYNC_PAGE));
        assertTrue(bundle.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        assertFalse(bundle.getBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT));
        assertEquals(4, bundle.size());
    }

    @Test
    @Feature({"FirstRun"})
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testFlowShowSearchEnginePage_historySyncEnabled() {
        setDelegateFactory(true);
        HistogramWatcher numberOfAccountsHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.AndroidDeviceAccountsNumberWhenEnteringFRE", 0);

        TestFirstRunFlowSequencer sequencer =
                new TestFirstRunFlowSequencer(mActivity, mProfileSupplier);
        sequencer.start();

        numberOfAccountsHistogram.assertExpected();
        assertTrue(sequencer.calledOnFlowIsKnown);

        Bundle bundle = sequencer.returnedBundle;
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SYNC_CONSENT_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_HISTORY_SYNC_PAGE));
        assertTrue(bundle.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        assertFalse(bundle.getBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT));
        assertEquals(4, bundle.size());
    }

    @Test
    @Feature({"FirstRun"})
    @Features.DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testFlowHideSyncConsentPageWhenUserIsNotSignedIn() {
        setDelegateFactory(false);
        HistogramWatcher numberOfAccountsHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.AndroidDeviceAccountsNumberWhenEnteringFRE", 0);

        TestFirstRunFlowSequencer sequencer =
                new TestFirstRunFlowSequencer(mActivity, mProfileSupplier);
        sequencer.start();

        numberOfAccountsHistogram.assertExpected();
        assertTrue(sequencer.calledOnFlowIsKnown);
        final Bundle bundle = sequencer.returnedBundle;
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SYNC_CONSENT_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_HISTORY_SYNC_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        assertFalse(bundle.getBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT));
        assertEquals(4, bundle.size());
    }

    @Test
    @Feature({"FirstRun"})
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testFlowHideHistorySyncPageWhenUserIsNotSignedIn() {
        setDelegateFactory(false);
        HistogramWatcher numberOfAccountsHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.AndroidDeviceAccountsNumberWhenEnteringFRE", 0);

        TestFirstRunFlowSequencer sequencer =
                new TestFirstRunFlowSequencer(mActivity, mProfileSupplier);
        sequencer.start();

        numberOfAccountsHistogram.assertExpected();
        assertTrue(sequencer.calledOnFlowIsKnown);
        final Bundle bundle = sequencer.returnedBundle;
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SYNC_CONSENT_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_HISTORY_SYNC_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        assertFalse(bundle.getBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT));
        assertEquals(4, bundle.size());
    }

    @Test
    @Feature({"FirstRun"})
    @Features.DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testFlowShowSyncConsentPageWhenUserIsSignedIn() {
        mAccountManagerTestRule.addAccount(ADULT_ACCOUNT_NAME);
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(true);
        setDelegateFactory(false);
        HistogramWatcher numberOfAccountsHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.AndroidDeviceAccountsNumberWhenEnteringFRE", 1);

        TestFirstRunFlowSequencer sequencer =
                new TestFirstRunFlowSequencer(mActivity, mProfileSupplier);
        sequencer.start();

        numberOfAccountsHistogram.assertExpected();
        assertTrue(sequencer.calledOnFlowIsKnown);
        final Bundle bundle = sequencer.returnedBundle;
        assertTrue(bundle.getBoolean(FirstRunActivityBase.SHOW_SYNC_CONSENT_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_HISTORY_SYNC_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        assertFalse(bundle.getBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT));
        assertEquals(4, bundle.size());
    }

    @Test
    @Feature({"FirstRun"})
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testFlowShowHistorySyncPageWhenUserIsSignedIn() {
        mAccountManagerTestRule.addAccount(ADULT_ACCOUNT_NAME);
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(true);
        setDelegateFactory(false);
        HistogramWatcher numberOfAccountsHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.AndroidDeviceAccountsNumberWhenEnteringFRE", 1);

        TestFirstRunFlowSequencer sequencer =
                new TestFirstRunFlowSequencer(mActivity, mProfileSupplier);
        sequencer.start();

        numberOfAccountsHistogram.assertExpected();
        assertTrue(sequencer.calledOnFlowIsKnown);
        final Bundle bundle = sequencer.returnedBundle;
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SYNC_CONSENT_PAGE));
        assertTrue(bundle.getBoolean(FirstRunActivityBase.SHOW_HISTORY_SYNC_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        assertFalse(bundle.getBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT));
        assertEquals(4, bundle.size());
    }

    @Test
    @Feature({"FirstRun"})
    @Features.DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testFlowUserIsSignedIn_syncDisabledByPolicy() {
        when(mSyncServiceMock.isSyncDisabledByEnterprisePolicy()).thenReturn(true);
        mAccountManagerTestRule.addAccount(ADULT_ACCOUNT_NAME);
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(true);
        setDelegateFactory(false);
        HistogramWatcher numberOfAccountsHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.AndroidDeviceAccountsNumberWhenEnteringFRE", 1);

        TestFirstRunFlowSequencer sequencer =
                new TestFirstRunFlowSequencer(mActivity, mProfileSupplier);
        sequencer.start();

        numberOfAccountsHistogram.assertExpected();
        assertTrue(sequencer.calledOnFlowIsKnown);
        final Bundle bundle = sequencer.returnedBundle;
        assertTrue(bundle.getBoolean(FirstRunActivityBase.SHOW_SYNC_CONSENT_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_HISTORY_SYNC_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        assertFalse(bundle.getBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT));
        assertEquals(4, bundle.size());
    }

    @Test
    @Feature({"FirstRun"})
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testFlowUserIsSignedIn_historySyncDisabledByPolicy_historySyncEnabled() {
        when(mHistorySyncHelperMock.isHistorySyncDisabledByPolicy()).thenReturn(true);
        mAccountManagerTestRule.addAccount(ADULT_ACCOUNT_NAME);
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(true);
        setDelegateFactory(false);
        HistogramWatcher numberOfAccountsHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.AndroidDeviceAccountsNumberWhenEnteringFRE", 1);

        TestFirstRunFlowSequencer sequencer =
                new TestFirstRunFlowSequencer(mActivity, mProfileSupplier);
        sequencer.start();

        numberOfAccountsHistogram.assertExpected();
        assertTrue(sequencer.calledOnFlowIsKnown);
        final Bundle bundle = sequencer.returnedBundle;
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SYNC_CONSENT_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_HISTORY_SYNC_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        assertFalse(bundle.getBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT));
        assertEquals(4, bundle.size());
    }

    @Test
    @Feature({"FirstRun"})
    @Features.DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testFlowUserIsSignedInAndSyncing() {
        mAccountManagerTestRule.addAccount(ADULT_ACCOUNT_NAME);
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SYNC)).thenReturn(true);
        setDelegateFactory(false);
        HistogramWatcher numberOfAccountsHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.AndroidDeviceAccountsNumberWhenEnteringFRE", 1);

        TestFirstRunFlowSequencer sequencer =
                new TestFirstRunFlowSequencer(mActivity, mProfileSupplier);
        sequencer.start();

        numberOfAccountsHistogram.assertExpected();
        assertTrue(sequencer.calledOnFlowIsKnown);
        final Bundle bundle = sequencer.returnedBundle;
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SYNC_CONSENT_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_HISTORY_SYNC_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        assertFalse(bundle.getBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT));
        assertEquals(4, bundle.size());
    }

    @Test
    @Feature({"FirstRun"})
    @Features.EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testFlowUserIsSignedIn_userAlreadySyncsHistory_historySyncEnabled() {
        when(mHistorySyncHelperMock.didAlreadyOptIn()).thenReturn(true);
        mAccountManagerTestRule.addAccount(ADULT_ACCOUNT_NAME);
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(true);
        setDelegateFactory(false);
        HistogramWatcher numberOfAccountsHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.AndroidDeviceAccountsNumberWhenEnteringFRE", 1);

        TestFirstRunFlowSequencer sequencer =
                new TestFirstRunFlowSequencer(mActivity, mProfileSupplier);
        sequencer.start();

        numberOfAccountsHistogram.assertExpected();
        assertTrue(sequencer.calledOnFlowIsKnown);
        final Bundle bundle = sequencer.returnedBundle;
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SYNC_CONSENT_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_HISTORY_SYNC_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        assertFalse(bundle.getBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT));
        assertEquals(4, bundle.size());
    }
}
