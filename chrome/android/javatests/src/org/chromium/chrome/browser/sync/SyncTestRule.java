// Copyright 2015 The Chromium Authors. All rights reserved.
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
import android.support.test.InstrumentationRegistry;

import androidx.annotation.Nullable;
import androidx.preference.TwoStatePreference;
import androidx.test.espresso.contrib.RecyclerViewActions;

import org.junit.Assert;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.IntentUtils;
import org.chromium.base.Promise;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.protocol.AutofillWalletSpecifics;
import org.chromium.components.sync.protocol.EntitySpecifics;
import org.chromium.components.sync.protocol.SyncEntity;
import org.chromium.components.sync.protocol.WalletMaskedCreditCard;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.Callable;

/**
 * TestRule for common functionality between sync tests.
 */
public class SyncTestRule extends ChromeTabbedActivityTestRule {
    private static final String TAG = "SyncTestBase";

    private static final Set<Integer> USER_SELECTABLE_TYPES =
            new HashSet<Integer>(Arrays.asList(new Integer[] {
                    ModelType.AUTOFILL, ModelType.BOOKMARKS, ModelType.PASSWORDS,
                    ModelType.PREFERENCES, ModelType.PROXY_TABS, ModelType.TYPED_URLS,
            }));

    /**
     * Simple activity that mimics a trusted vault key retrieval flow that succeeds immediately.
     */
    public static class DummyKeyRetrievalActivity extends Activity {
        @Override
        protected void onCreate(@Nullable Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            setResult(RESULT_OK);
            FakeTrustedVaultClientBackend.get().startPopulateKeys();
            finish();
        }
    };

    /**
     * A fake implementation of TrustedVaultClient.Backend. Allows to specify keys to be fetched.
     * Keys aren't populated through fetchKeys() unless startPopulateKeys() is called.
     * startPopulateKeys() is called by DummyKeyRetrievalActivity before its completion to mimic
     * real TrustedVaultClient.Backend implementation.
     */
    public static class FakeTrustedVaultClientBackend implements TrustedVaultClient.Backend {
        private static FakeTrustedVaultClientBackend sInstance;
        private boolean mPopulateKeys;
        private @Nullable List<byte[]> mKeys;

        public FakeTrustedVaultClientBackend() {
            mPopulateKeys = false;
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
            Context context = InstrumentationRegistry.getContext();
            Intent intent = new Intent(context, DummyKeyRetrievalActivity.class);
            return Promise.fulfilled(PendingIntent.getActivity(context, 0 /* requestCode */, intent,
                    IntentUtils.getPendingIntentMutabilityFlag(false)));
        }

        @Override
        public Promise<Boolean> markKeysAsStale(CoreAccountInfo accountInfo) {
            return Promise.rejected();
        }

        public void setKeys(List<byte[]> keys) {
            mKeys = Collections.unmodifiableList(keys);
        }

        public void startPopulateKeys() {
            mPopulateKeys = true;
        }
    }

    private Context mContext;
    private FakeServerHelper mFakeServerHelper;
    private ProfileSyncService mProfileSyncService;
    private MockSyncContentResolverDelegate mSyncContentResolver;
    private final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    private void ruleTearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mProfileSyncService.setSyncRequested(false);
            FakeServerHelper.deleteFakeServer();
        });
        ProfileSyncService.resetForTests();
    }

    public SyncTestRule() {}

    /**Getters for Test variables */
    public Context getTargetContext() {
        return mContext;
    }

    public FakeServerHelper getFakeServerHelper() {
        return mFakeServerHelper;
    }

    public ProfileSyncService getProfileSyncService() {
        return mProfileSyncService;
    }

    MockSyncContentResolverDelegate getSyncContentResolver() {
        return mSyncContentResolver;
    }

    public void startMainActivityForSyncTest() throws Exception {
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

    /**
     * Adds an account of given account name to AccountManagerFacade and waits for the seeding.
     */
    public CoreAccountInfo addAccount(String accountName) {
        CoreAccountInfo coreAccountInfo =
                mAccountManagerTestRule.addAccountAndWaitForSeeding(accountName);
        Assert.assertFalse(SyncTestUtil.isSyncRequested());
        return coreAccountInfo;
    }

    /**
     * Returns the currently signed in account.
     */
    public CoreAccountInfo getCurrentSignedInAccount() {
        return mAccountManagerTestRule.getCurrentSignedInAccount();
    }

    /**
     * Set up a test account, sign in and enable sync. FirstSetupComplete bit will be set after
     * this. For most purposes this function should be used as this emulates the basic sign in flow.
     * @return the test account that is signed in.
     */
    public CoreAccountInfo setUpAccountAndEnableSyncForTesting() {
        CoreAccountInfo accountInfo =
                mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync(mProfileSyncService);
        // Enable UKM when enabling sync as it is done by the sync confirmation UI.
        enableUKM();
        SyncTestUtil.waitForSyncFeatureActive();
        SyncTestUtil.triggerSyncAndWaitForCompletion();
        return accountInfo;
    }

    /**
     * Set up a test account and sign in. Does not setup sync.
     * @return the test accountInfo that is signed in.
     */
    public CoreAccountInfo setUpAccountAndSignInForTesting() {
        return mAccountManagerTestRule.addTestAccountThenSignin();
    }

    /**
     * Set up a test account, sign in but don't mark sync setup complete.
     * @return the test account that is signed in.
     */
    public CoreAccountInfo setUpTestAccountAndSignInWithSyncSetupAsIncomplete() {
        CoreAccountInfo accountInfo = mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync(
                /* profileSyncService= */ null);
        // Enable UKM when enabling sync as it is done by the sync confirmation UI.
        enableUKM();
        SyncTestUtil.waitForSyncTransportActive();
        return accountInfo;
    }

    public void startSync() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mProfileSyncService.setSyncRequested(true); });
    }

    public void startSyncAndWait() {
        startSync();
        SyncTestUtil.waitForSyncFeatureActive();
    }

    public void stopSync() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mProfileSyncService.setSyncRequested(false); });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    public void signinAndEnableSync(final CoreAccountInfo accountInfo) {
        SigninTestUtil.signinAndEnableSync(accountInfo, mProfileSyncService);
        // Enable UKM when enabling sync as it is done by the sync confirmation UI.
        enableUKM();
        SyncTestUtil.waitForSyncFeatureActive();
        SyncTestUtil.triggerSyncAndWaitForCompletion();
    }

    public void signOut() {
        mAccountManagerTestRule.signOut();
        Assert.assertNull(mAccountManagerTestRule.getCurrentSignedInAccount());
        Assert.assertFalse(SyncTestUtil.isSyncRequested());
    }

    public void clearServerData() {
        mFakeServerHelper.clearServerData();
        SyncTestUtil.triggerSync();
        CriteriaHelper.pollUiThread(() -> {
            return !ProfileSyncService.get().isSyncRequested();
        }, SyncTestUtil.TIMEOUT_MS, SyncTestUtil.INTERVAL_MS);
    }

    /*
     * Enables the |modelType| Sync data type, which must be in USER_SELECTABLE_TYPES.
     */
    public void enableDataType(final int modelType) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Set<Integer> chosenTypes = mProfileSyncService.getChosenDataTypes();
            chosenTypes.add(modelType);
            mProfileSyncService.setChosenDataTypes(false, chosenTypes);
        });
    }

    /*
     * Enables the |chosenDataTypes|, which must be in USER_SELECTABLE_TYPES.
     */
    public void setChosenDataTypes(boolean syncEverything, Set<Integer> chosenDataTypes) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mProfileSyncService.setChosenDataTypes(syncEverything, chosenDataTypes); });
    }

    /*
     * Sets payments integration to |enabled|.
     */
    public void setPaymentsIntegrationEnabled(final boolean enabled) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> PersonalDataManager.setPaymentsIntegrationEnabled(enabled));
    }

    /*
     * Disables the |modelType| Sync data type, which must be in USER_SELECTABLE_TYPES.
     */
    public void disableDataType(final int modelType) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Set<Integer> chosenTypes = mProfileSyncService.getChosenDataTypes();
            chosenTypes.remove(modelType);
            mProfileSyncService.setChosenDataTypes(false, chosenTypes);
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
    public Statement apply(final Statement statement, final Description desc) {
        final Statement base = super.apply(new Statement() {
            @Override
            public void evaluate() throws Throwable {
                mSyncContentResolver = new MockSyncContentResolverDelegate();
                mSyncContentResolver.setMasterSyncAutomatically(true);
                TestThreadUtils.runOnUiThreadBlocking(
                        () -> SyncContentResolverDelegate.overrideForTests(mSyncContentResolver));

                TrustedVaultClient.setInstanceForTesting(
                        new TrustedVaultClient(FakeTrustedVaultClientBackend.get()));

                // Load native since the FakeServer needs it and possibly ProfileSyncService as well
                // (depends on what fake is provided by |createProfileSyncService()|).
                NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();

                TestThreadUtils.runOnUiThreadBlocking(() -> {
                    ProfileSyncService profileSyncService = createProfileSyncService();
                    if (profileSyncService != null) {
                        ProfileSyncService.overrideForTests(profileSyncService);
                    }
                    mProfileSyncService = ProfileSyncService.get();

                    mContext = InstrumentationRegistry.getTargetContext();
                    FakeServerHelper.useFakeServer(mContext);
                    mFakeServerHelper = FakeServerHelper.get();
                });

                startMainActivityForSyncTest();

                // Ensure SyncController is created.
                TestThreadUtils.runOnUiThreadBlocking(() -> SyncController.get());

                statement.evaluate();
            }
        }, desc);
        return mAccountManagerTestRule.apply(new Statement() {
            @Override
            public void evaluate() throws Throwable {
                base.evaluate();
                ruleTearDown();
            }
        }, desc);
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
        SyncEntity entity = SyncEntity.newBuilder()
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
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            List<CreditCard> cards = PersonalDataManager.getInstance().getCreditCardsForSettings();
            for (int i = 0; i < cards.size(); i++) {
                if (!cards.get(i).getIsLocal()) return true;
            }
            return false;
        });
    }

    // UI interaction convenience methods.
    public void togglePreference(final TwoStatePreference pref) {
        onView(withId(R.id.recycler_view))
                .perform(RecyclerViewActions.actionOnItem(
                        hasDescendant(withText(pref.getTitle().toString())), click()));
    }

    /**
     * Returns an instance of ProfileSyncService that can be overridden by subclasses.
     */
    protected ProfileSyncService createProfileSyncService() {
        return null;
    }

    private static void enableUKM() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Outside of tests, URL-keyed anonymized data collection is enabled by sign-in UI.
            UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                    Profile.getLastUsedRegularProfile(), true);
        });
    }
}
