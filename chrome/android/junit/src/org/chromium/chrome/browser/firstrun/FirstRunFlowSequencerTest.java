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
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.signin.AppRestrictionSupplier;
import org.chromium.chrome.browser.signin.ChildAccountStatusSupplier;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.sync.SyncService;

/**
 * Tests FirstRunFlowSequencer which contains the core logic of what should be shown during the
 * first run.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class FirstRunFlowSequencerTest {

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
        public Bundle bundle;
        public boolean calledOnFlowIsKnown;

        public TestFirstRunFlowSequencer(
                Activity activity, OneshotSupplier<ProfileProvider> profileSupplier) {
            super(
                    profileSupplier,
                    new ChildAccountStatusSupplier(
                            AccountManagerFacadeProvider.getInstance(),
                            new AppRestrictionSupplier()));
        }

        @Override
        public void onFlowIsKnown(boolean isChild) {
            calledOnFlowIsKnown = true;
            Bundle freProperties = new Bundle();
            updateFirstRunProperties(freProperties);
            bundle = freProperties;
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
    public void testFlowOneChildAccount() {
        mAccountManagerTestRule.addAccount(TestAccounts.CHILD_ACCOUNT);
        setDelegateFactory(false);
        HistogramWatcher numberOfAccountsHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.AndroidDeviceAccountsNumberWhenEnteringFRE", 1);

        TestFirstRunFlowSequencer sequencer =
                new TestFirstRunFlowSequencer(mActivity, mProfileSupplier);
        sequencer.start();

        numberOfAccountsHistogram.assertExpected();
        assertTrue(sequencer.calledOnFlowIsKnown);

        Bundle bundle = sequencer.bundle;
        assertTrue(bundle.getBoolean(FirstRunActivityBase.SHOW_HISTORY_SYNC_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        assertEquals(2, bundle.size());
    }

    @Test
    @Feature({"FirstRun"})
    public void testFlowOneChildAccount_historySyncManagedByCustodian() {
        when(mHistorySyncHelperMock.isHistorySyncDisabledByCustodian()).thenReturn(true);
        mAccountManagerTestRule.addAccount(TestAccounts.CHILD_ACCOUNT);
        setDelegateFactory(false);
        HistogramWatcher numberOfAccountsHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.AndroidDeviceAccountsNumberWhenEnteringFRE", 1);

        TestFirstRunFlowSequencer sequencer =
                new TestFirstRunFlowSequencer(mActivity, mProfileSupplier);
        sequencer.start();

        numberOfAccountsHistogram.assertExpected();
        assertTrue(sequencer.calledOnFlowIsKnown);

        Bundle bundle = sequencer.bundle;
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_HISTORY_SYNC_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        assertEquals(2, bundle.size());
    }

    @Test
    @Feature({"FirstRun"})
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

        Bundle bundle = sequencer.bundle;
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_HISTORY_SYNC_PAGE));
        assertTrue(bundle.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        assertEquals(2, bundle.size());
    }

    @Test
    @Feature({"FirstRun"})
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
        final Bundle bundle = sequencer.bundle;
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_HISTORY_SYNC_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        assertEquals(2, bundle.size());
    }

    @Test
    @Feature({"FirstRun"})
    public void testFlowShowHistorySyncPageWhenUserIsSignedIn() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
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
        final Bundle bundle = sequencer.bundle;
        assertTrue(bundle.getBoolean(FirstRunActivityBase.SHOW_HISTORY_SYNC_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        assertEquals(2, bundle.size());
    }

    @Test
    @Feature({"FirstRun"})
    public void testFlowUserIsSignedIn_historySyncDisabledByPolicy() {
        when(mHistorySyncHelperMock.isHistorySyncDisabledByPolicy()).thenReturn(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
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
        final Bundle bundle = sequencer.bundle;
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_HISTORY_SYNC_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        assertEquals(2, bundle.size());
    }

    @Test
    @Feature({"FirstRun"})
    public void testFlowUserIsSignedIn_userAlreadySyncsHistory() {
        when(mHistorySyncHelperMock.didAlreadyOptIn()).thenReturn(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
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
        final Bundle bundle = sequencer.bundle;
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_HISTORY_SYNC_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        assertEquals(2, bundle.size());
    }
}
