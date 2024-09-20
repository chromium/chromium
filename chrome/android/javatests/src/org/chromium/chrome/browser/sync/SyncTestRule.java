// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.preference.TwoStatePreference;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.contrib.RecyclerViewActions;

import org.junit.Assert;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.IntentUtils;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.protocol.AutofillWalletSpecifics;
import org.chromium.components.sync.protocol.EntitySpecifics;
import org.chromium.components.sync.protocol.SyncEntity;
import org.chromium.components.sync.protocol.WalletMaskedCreditCard;

import java.util.Collections;
import java.util.List;
import java.util.Set;
import java.util.concurrent.Callable;

/**
 * TestRule for common functionality between sync tests. TODO(crbug.com/40743432): Support batching
 * tests with SyncTestRule.
 */
public class SyncTestRule extends ChromeTabbedActivityTestRule {
    /** Simple activity that mimics a trusted vault key retrieval flow that succeeds immediately. */
    public static class FakeKeyRetrievalActivity extends Activity {
        @Override
        protected void onCreate(@Nullable Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            setResult(RESULT_OK);
            FakeTrustedVaultClientBackend.get().startPopulateKeys();
            finish();
        }
    }

    /**
     * Simple activity that mimics a trusted vault degraded recoverability fix flow that succeeds
     * immediately.
     */
    public static class FakeRecoverabilityDegradedFixActivity extends Activity {
        @Override
        protected void onCreate(@Nullable Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            setResult(RESULT_OK);
            FakeTrustedVaultClientBackend.get().setRecoverabilityDegraded(false);
            finish();
        }
    }

    /**
     * A fake implementation of TrustedVaultClient.Backend. Allows to specify keys to be fetched.
     * Keys aren't populated through fetchKeys() unless startPopulateKeys() is called.
     * startPopulateKeys() is called by FakeKeyRetrievalActivity before its completion to mimic real
     * TrustedVaultClient.Backend implementation.
     *
     * <p>Similarly, recoverability-degraded logic is implemented with a fake activity. Tests can
     * choose to enter this state via invoking setRecoverabilityDegraded(true), and the state can be
     * resolved with FakeRecoverabilityDegradedFixActivity.
     */
    public static class FakeTrustedVaultClientBackend implements TrustedVaultClient.Backend {
        private static FakeTrustedVaultClientBackend sInstance;
        private boolean mPopulateKeys;
        private boolean mRecoverabilityDegraded;
        private @Nullable List<byte[]> mKeys;

        public FakeTrustedVaultClientBackend() {
            mPopulateKeys = false;
            mRecoverabilityDegraded = false;
        }

        public static FakeTrustedVaultClientBackend get() {
            if (sInstance == null) {
                sInstance = new FakeTrustedVaultClientBackend();
            }
            return sInstance;
        }

        @Override
        public Promise<List<byte[]>> fetchKeys(CoreAccountInfo accountInfo) {
            if (mKeys == null || !mPopulateKeys) {
                return Promise.rejected();
            }
            return Promise.fulfilled(mKeys);
        }

        @Override
        public Promise<PendingIntent> createKeyRetrievalIntent(CoreAccountInfo accountInfo) {
            Context context = ApplicationProvider.getApplicationContext();
            Intent intent = new Intent(context, FakeKeyRetrievalActivity.class);
            return Promise.fulfilled(
                    PendingIntent.getActivity(
                            context,
                            /* requestCode= */ 0,
                            intent,
                            IntentUtils.getPendingIntentMutabilityFlag(false)));
        }

        @Override
        public Promise<Boolean> markLocalKeysAsStale(CoreAccountInfo accountInfo) {
            return Promise.rejected();
        }

        @Override
        public Promise<Boolean> getIsRecoverabilityDegraded(CoreAccountInfo accountInfo) {
            return Promise.fulfilled(mRecoverabilityDegraded);
        }

        @Override
        public Promise<Void> addTrustedRecoveryMethod(
                CoreAccountInfo accountInfo, byte[] publicKey, int methodTypeHint) {
            return Promise.fulfilled(null);
        }

        @Override
        public Promise<PendingIntent> createRecoverabilityDegradedIntent(
                CoreAccountInfo accountInfo) {
            Context context = ApplicationProvider.getApplicationContext();
            Intent intent = new Intent(context, FakeRecoverabilityDegradedFixActivity.class);
            return Promise.fulfilled(
                    PendingIntent.getActivity(
                            context,
                            /* requestCode= */ 0,
                            intent,
                            IntentUtils.getPendingIntentMutabilityFlag(false)));
        }

        @Override
        public Promise<PendingIntent> createOptInIntent(CoreAccountInfo accountInfo) {
            return Promise.rejected();
        }

        public void setKeys(List<byte[]> keys) {
            mKeys = Collections.unmodifiableList(keys);
        }

        public void startPopulateKeys() {
            mPopulateKeys = true;
        }

        public void setRecoverabilityDegraded(boolean degraded) {
            mRecoverabilityDegraded = degraded;
        }
    }

    private FakeServerHelper mFakeServerHelper;
    private SyncService mSyncService;
    private final SigninTestRule mSigninTestRule = new SigninTestRule();

    public SigninTestRule getSigninTestRule() {
        return mSigninTestRule;
    }

    public SyncTestRule() {}

    /** Getters for Test variables */
    public Context getTargetContext() {
        return ApplicationProvider.getApplicationContext();
    }

    public FakeServerHelper getFakeServerHelper() {
        return mFakeServerHelper;
    }

    public SyncService getSyncService() {
        return mSyncService;
    }

    public void startMainActivityForSyncTest() {
        // Start the activity by opening about:blank. This URL is ideal because it is not synced as
        // a typed URL. If another URL is used, it could interfere with test data.
        startMainActivityOnBlankPage();
    }

    /**
     * Adds an account of default account name to AccountManagerFacade and waits for the seeding.
     */
    public CoreAccountInfo addTestAccount() {
        return addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
    }

    /** Adds an account of given account name to AccountManagerFacade and waits for the seeding. */
    public CoreAccountInfo addAccount(String accountName) {
        CoreAccountInfo coreAccountInfo = mSigninTestRule.addAccount(accountName);
        Assert.assertFalse(SyncTestUtil.isSyncFeatureEnabled());
        return coreAccountInfo;
    }

    /**
     * @return The primary account of the requested {@link ConsentLevel}.
     */
    public CoreAccountInfo getPrimaryAccount(@ConsentLevel int consentLevel) {
        return mSigninTestRule.getPrimaryAccount(consentLevel);
    }

    /**
     * Set up a test account, sign in and enable sync. FirstSetupComplete bit will be set after
     * this. For most purposes this function should be used as this emulates the basic sign in flow.
     *
     * @return the test account that is signed in.
     */
    public CoreAccountInfo setUpAccountAndEnableSyncForTesting() {
        return setUpAccountAndEnableSyncForTesting(false);
    }

    /**
     * Set up a child test account, sign in and enable sync. FirstSetupComplete bit will be set
     * after this. For most purposes this function should be used as this emulates the basic sign in
     * flow.
     *
     * @return the test account that is signed in.
     */
    public CoreAccountInfo setUpChildAccountAndEnableSyncForTesting() {
        return setUpAccountAndEnableSyncForTesting(true);
    }

    /**
     * Set up a test account and sign in. Does not setup sync.
     *
     * @return the test accountInfo that is signed in.
     */
    public CoreAccountInfo setUpAccountAndSignInForTesting() {
        return mSigninTestRule.addTestAccountThenSignin();
    }

    /**
     * Set up a test account, sign in but don't mark sync setup complete.
     *
     * @return the test account that is signed in.
     */
    public CoreAccountInfo setUpTestAccountAndSignInWithSyncSetupAsIncomplete() {
        CoreAccountInfo accountInfo =
                mSigninTestRule.addTestAccountThenSigninAndEnableSync(/* syncService= */ null);
        SyncTestUtil.waitForSyncTransportActive();
        return accountInfo;
    }

    public void signinAndEnableSync(final CoreAccountInfo accountInfo) {
        SigninTestUtil.signinAndEnableSync(accountInfo, mSyncService);
        // Enable UKM when enabling sync as it is done by the sync confirmation UI.
        enableUKM();
        SyncTestUtil.waitForSyncFeatureActive();
        SyncTestUtil.triggerSyncAndWaitForCompletion();
    }

    public void signOut() {
        mSigninTestRule.signOut();
        Assert.assertNull(mSigninTestRule.getPrimaryAccount(ConsentLevel.SYNC));
        Assert.assertFalse(SyncTestUtil.isSyncFeatureEnabled());
    }

    public void clearServerData() {
        mFakeServerHelper.clearServerData();
        // SyncTestRule doesn't currently exercise invalidations, as opposed to
        // C++ sync integration tests (based on SyncTest) which use
        // FakeServerInvalidationSender to mimic invalidations. Hence, it is
        // necessary to invoke triggerSync() explicitly, just like many Java
        // tests do.
        SyncTestUtil.triggerSync();
        CriteriaHelper.pollUiThread(
                () -> {
                    return !SyncTestUtil.getSyncServiceForLastUsedProfile().isSyncFeatureEnabled();
                },
                SyncTestUtil.TIMEOUT_MS,
                SyncTestUtil.INTERVAL_MS);
    }

    /*
     * Enables the Sync data type in USER_SELECTABLE_TYPES.
     */
    public void enableDataType(final int userSelectableType) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Set<Integer> chosenTypes = mSyncService.getSelectedTypes();
                    chosenTypes.add(userSelectableType);
                    mSyncService.setSelectedTypes(false, chosenTypes);
                });
    }

    /*
     * Enables the |selectedTypes| in USER_SELECTABLE_TYPES.
     */
    public void setSelectedTypes(boolean syncEverything, Set<Integer> selectedTypes) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSyncService.setSelectedTypes(syncEverything, selectedTypes);
                });
    }

    /*
     * Disables the Sync data type in USER_SELECTABLE_TYPES.
     */
    public void disableDataType(final int userSelectableType) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Set<Integer> chosenTypes = mSyncService.getSelectedTypes();
                    chosenTypes.remove(userSelectableType);
                    mSyncService.setSelectedTypes(false, chosenTypes);
                });
    }

    public void pollInstrumentationThread(Runnable criteria) {
        CriteriaHelper.pollInstrumentationThread(
                criteria, SyncTestUtil.TIMEOUT_MS, SyncTestUtil.INTERVAL_MS);
    }

    public void pollInstrumentationThread(Callable<Boolean> criteria, String reason) {
        CriteriaHelper.pollInstrumentationThread(
                criteria, reason, SyncTestUtil.TIMEOUT_MS, SyncTestUtil.INTERVAL_MS);
    }

    @Override
    public Statement apply(final Statement base, final Description desc) {
        final Statement superStatement = super.apply(base, desc);
        return mSigninTestRule.apply(superStatement, desc);
    }

    @Override
    protected void before() throws Throwable {
        super.before();
        TrustedVaultClient.setInstanceForTesting(
                new TrustedVaultClient(FakeTrustedVaultClientBackend.get()));

        startMainActivityForSyncTest();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SyncService syncService = createSyncServiceImpl();
                    if (syncService != null) {
                        SyncServiceFactory.setInstanceForTesting(syncService);
                    }
                    mSyncService = SyncTestUtil.getSyncServiceForLastUsedProfile();
                    mFakeServerHelper = FakeServerHelper.createInstanceAndGet();
                });
    }

    @Override
    protected void after() {
        super.after();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSyncService = null;
                    mFakeServerHelper = null;
                    FakeServerHelper.destroyInstance();
                });
    }

    /*
     * Adds a credit card to server for autofill.
     */
    public void addServerAutofillCreditCard() {
        final String serverId = "025eb937c022489eb8dc78cbaa969218";
        WalletMaskedCreditCard card =
                WalletMaskedCreditCard.newBuilder()
                        .setId(serverId)
                        .setStatus(WalletMaskedCreditCard.WalletCardStatus.VALID)
                        .setNameOnCard("Jon Doe")
                        .setType(WalletMaskedCreditCard.WalletCardType.UNKNOWN)
                        .setLastFour("1111")
                        .setExpMonth(11)
                        .setExpYear(2020)
                        .build();
        AutofillWalletSpecifics wallet_specifics =
                AutofillWalletSpecifics.newBuilder()
                        .setType(AutofillWalletSpecifics.WalletInfoType.MASKED_CREDIT_CARD)
                        .setMaskedCard(card)
                        .build();
        EntitySpecifics specifics =
                EntitySpecifics.newBuilder().setAutofillWallet(wallet_specifics).build();
        SyncEntity entity =
                SyncEntity.newBuilder()
                        .setName(serverId)
                        .setIdString(serverId)
                        .setSpecifics(specifics)
                        .build();
        getFakeServerHelper().setWalletData(entity);
        SyncTestUtil.triggerSyncAndWaitForCompletion();
    }

    /*
     * Checks if server has any credit card information to autofill.
     */
    public boolean hasServerAutofillCreditCards() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<CreditCard> cards =
                            AutofillTestHelper.getPersonalDataManagerForLastUsedProfile()
                                    .getCreditCardsForSettings();
                    for (int i = 0; i < cards.size(); i++) {
                        if (!cards.get(i).getIsLocal()) return true;
                    }
                    return false;
                });
    }

    // UI interaction convenience methods.
    public void togglePreference(final TwoStatePreference pref) {
        onView(withId(R.id.recycler_view))
                .perform(
                        RecyclerViewActions.actionOnItem(
                                hasDescendant(withText(pref.getTitle().toString())), click()));
    }

    /** Returns an instance of SyncService that can be overridden by subclasses. */
    protected SyncService createSyncServiceImpl() {
        return null;
    }

    private static void enableUKM() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Outside of tests, URL-keyed anonymized data collection is enabled by sign-in
                    // UI.
                    UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                            ProfileManager.getLastUsedRegularProfile(), true);
                });
    }

    private CoreAccountInfo setUpAccountAndEnableSyncForTesting(boolean isChildAccount) {
        CoreAccountInfo accountInfo =
                mSigninTestRule.addTestAccountThenSigninAndEnableSync(mSyncService, isChildAccount);

        // Enable UKM when enabling sync as it is done by the sync confirmation UI.
        enableUKM();
        SyncTestUtil.waitForSyncFeatureActive();
        SyncTestUtil.triggerSyncAndWaitForCompletion();
        return accountInfo;
    }
}
