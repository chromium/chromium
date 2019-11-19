// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.v7.preference.TwoStatePreference;

import org.junit.Assert;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.SyncFirstSetupCompleteSource;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.identity.UniqueIdentificationGenerator;
import org.chromium.chrome.browser.identity.UniqueIdentificationGeneratorFactory;
import org.chromium.chrome.browser.identity.UuidBasedUniqueIdentificationGenerator;
import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.SigninManager;
import org.chromium.chrome.browser.signin.UnifiedConsentServiceBridge;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.protocol.AutofillWalletSpecifics;
import org.chromium.components.sync.protocol.EntitySpecifics;
import org.chromium.components.sync.protocol.SyncEntity;
import org.chromium.components.sync.protocol.WalletMaskedCreditCard;
import org.chromium.components.sync.test.util.MockSyncContentResolverDelegate;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/**
 * TestRule for common functionality between sync tests.
 */
public class SyncTestRule extends ChromeActivityTestRule<ChromeActivity> {
    private static final String TAG = "SyncTestBase";

    private static final String CLIENT_ID = "Client_ID";

    private static final Set<Integer> USER_SELECTABLE_TYPES =
            new HashSet<Integer>(Arrays.asList(new Integer[] {
                    ModelType.AUTOFILL, ModelType.BOOKMARKS, ModelType.PASSWORDS,
                    ModelType.PREFERENCES, ModelType.PROXY_TABS, ModelType.TYPED_URLS,
            }));

    public abstract static class DataCriteria<T> extends Criteria {
        public DataCriteria() {
            super("Sync data criteria not met.");
        }

        public abstract boolean isSatisfied(List<T> data);

        public abstract List<T> getData() throws Exception;

        @Override
        public boolean isSatisfied() {
            try {
                return isSatisfied(getData());
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }
    }

    private Context mContext;
    private FakeServerHelper mFakeServerHelper;
    private ProfileSyncService mProfileSyncService;
    private MockSyncContentResolverDelegate mSyncContentResolver;

    private void ruleSetUp() {
        // This must be called before super.setUp() in order for test authentication to work.
        SigninTestUtil.setUpAuthForTest();
    }

    private void ruleTearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mProfileSyncService.requestStop();
            FakeServerHelper.deleteFakeServer();
        });
        SigninTestUtil.tearDownAuthForTest();
    }

    public SyncTestRule() {
        super(ChromeActivity.class);
    }

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

    public MockSyncContentResolverDelegate getSyncContentResolver() {
        return mSyncContentResolver;
    }

    public void startMainActivityForSyncTest() throws Exception {
        // Start the activity by opening about:blank. This URL is ideal because it is not synced as
        // a typed URL. If another URL is used, it could interfere with test data.
        startMainActivityOnBlankPage();
    }

    private void setUpMockAndroidSyncSettings() {
        mSyncContentResolver = new MockSyncContentResolverDelegate();
        mSyncContentResolver.setMasterSyncAutomatically(true);
        AndroidSyncSettings.overrideForTests(mSyncContentResolver, null);
    }

    public Account setUpTestAccount() {
        Account account = SigninTestUtil.addTestAccount();
        Assert.assertFalse(SyncTestUtil.isSyncRequested());
        return account;
    }

    /**
     * Set up a test account, sign in and enable sync. FirstSetupComplete bit will be set after
     * this. For most purposes this function should be used as this emulates the basic sign in flow.
     * @return the test account that is signed in.
     */
    public Account setUpTestAccountAndSignIn() {
        Account account = setUpTestAccount();
        signinAndEnableSync(account);
        return account;
    }

    /**
     * Set up a test account, sign in but don't mark sync setup complete.
     * @return the test account that is signed in.
     */
    public Account setUpTestAccountAndSignInWithSyncSetupAsIncomplete() {
        Account account = setUpTestAccount();
        signinAndEnableSyncInternal(account, false);
        return account;
    }

    public void startSync() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mProfileSyncService.requestStart(); });
    }

    public void startSyncAndWait() {
        startSync();
        SyncTestUtil.waitForSyncActive();
    }

    public void stopSync() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mProfileSyncService.requestStop(); });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    public void signinAndEnableSync(final Account account) {
        signinAndEnableSyncInternal(account, true);
    }

    public void signOut() throws InterruptedException {
        final Semaphore s = new Semaphore(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            IdentityServicesProvider.getSigninManager().signOut(
                    SignoutReason.SIGNOUT_TEST, s::release, false);
        });
        Assert.assertTrue(s.tryAcquire(SyncTestUtil.TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertNull(SigninTestUtil.getCurrentAccount());
        Assert.assertFalse(SyncTestUtil.isSyncRequested());
    }

    public void clearServerData() {
        mFakeServerHelper.clearServerData();
        SyncTestUtil.triggerSync();
        CriteriaHelper.pollUiThread(new Criteria("Timed out waiting for sync to stop.") {
            @Override
            public boolean isSatisfied() {
                return !ProfileSyncService.get().isSyncRequested();
            }
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

    public void pollInstrumentationThread(Criteria criteria) {
        CriteriaHelper.pollInstrumentationThread(
                criteria, SyncTestUtil.TIMEOUT_MS, SyncTestUtil.INTERVAL_MS);
    }

    @Override
    public Statement apply(final Statement statement, final Description desc) {
        final Statement base = super.apply(new Statement() {
            @Override
            public void evaluate() throws Throwable {
                setUpMockAndroidSyncSettings();

                startMainActivityForSyncTest();
                mContext = InstrumentationRegistry.getTargetContext();

                TestThreadUtils.runOnUiThreadBlocking(() -> {
                    // Ensure SyncController is registered with the new AndroidSyncSettings.
                    AndroidSyncSettings.get().registerObserver(SyncController.get(mContext));
                    mFakeServerHelper = FakeServerHelper.get();
                });
                FakeServerHelper.useFakeServer(mContext);
                TestThreadUtils.runOnUiThreadBlocking(
                        () -> { mProfileSyncService = ProfileSyncService.get(); });

                UniqueIdentificationGeneratorFactory.registerGenerator(
                        UuidBasedUniqueIdentificationGenerator.GENERATOR_ID,
                        new UniqueIdentificationGenerator() {
                            @Override
                            public String getUniqueId(String salt) {
                                return CLIENT_ID;
                            }
                        },
                        true);
                statement.evaluate();
            }
        }, desc);
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                ruleSetUp();
                base.evaluate();
                ruleTearDown();
            }
        };
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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            boolean newValue = !pref.isChecked();
            pref.getOnPreferenceChangeListener().onPreferenceChange(pref, newValue);
            pref.setChecked(newValue);
        });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private void signinAndEnableSyncInternal(final Account account, boolean setFirstSetupComplete) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            IdentityServicesProvider.getSigninManager().signIn(
                    account, new SigninManager.SignInCallback() {
                        @Override
                        public void onSignInComplete() {
                            if (ChromeFeatureList.isEnabled(
                                        ChromeFeatureList.SYNC_MANUAL_START_ANDROID)
                                    && setFirstSetupComplete) {
                                mProfileSyncService.setFirstSetupComplete(
                                        SyncFirstSetupCompleteSource.BASIC_FLOW);
                            }
                        }

                        @Override
                        public void onSignInAborted() {
                            Assert.fail("Sign-in was aborted");
                        }
                    });
            // Outside of tests, URL-keyed anonymized data collection is enabled by sign-in UI.
            UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(true);
        });
        if (setFirstSetupComplete) {
            SyncTestUtil.waitForSyncActive();
            SyncTestUtil.triggerSyncAndWaitForCompletion();
        } else {
            SyncTestUtil.waitForSyncTransportActive();
        }
        Assert.assertEquals(account, SigninTestUtil.getCurrentAccount());
    }
}
