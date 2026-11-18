// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKey;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileResolver;
import org.chromium.chrome.browser.profiles.ProfileResolverJni;
import org.chromium.chrome.browser.profiles.ProfileTokenUtils;
import org.chromium.chrome.browser.sync.FakeSyncServiceImpl;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/** Tests for PassphraseActivity. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class PassphraseActivityTest {
    @Rule public final SigninTestRule mSigninTestRule = SigninTestRule.createWithCleanups();

    private Context mContext;

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        mContext = ApplicationProvider.getApplicationContext();
    }

    /** This is a regression test for http://crbug.com/41164232. */
    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testCallbackAfterBackgrounded() {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        // Override before signing in, otherwise regular SyncService will be created.
        FakeSyncServiceImpl fakeSyncService = overrideSyncService();
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        // Create the activity.
        final PassphraseActivity activity = launchPassphraseActivity();
        Assert.assertNotNull(activity);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Fake backgrounding the activity.
                    Bundle bundle = new Bundle();
                    InstrumentationRegistry.getInstrumentation().callActivityOnPause(activity);
                    InstrumentationRegistry.getInstrumentation()
                            .callActivityOnSaveInstanceState(activity, bundle);
                    // Fake sync's backend finishing its initialization.
                    fakeSyncService.setEngineInitialized(true);
                });
        // Nothing crashed; success!

        // Finish the activity before resetting the state.
        activity.finish();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    @Test
    @MediumTest
    @Feature({"Sync"})
    public void testLaunchPassphraseDialog() {
        // Override before signing in, otherwise regular SyncService will be created.
        FakeSyncServiceImpl fakeSyncService = overrideSyncService();
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        // Create the activity.
        final PassphraseActivity activity = launchPassphraseActivity();
        Assert.assertNotNull(activity);

        ThreadUtils.runOnUiThreadBlocking(() -> fakeSyncService.setEngineInitialized(true));
        final PassphraseDialogFragment fragment =
                ActivityTestUtils.waitForFragment(activity, PassphraseActivity.FRAGMENT_PASSPHRASE);
        Assert.assertTrue(fragment.isAdded());

        // Finish the activity before resetting the state.
        activity.finish();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    @Test
    @MediumTest
    @Feature({"Sync"})
    public void testLaunchPassphraseDialogForSignedInUsers() {
        // Override before signing in, otherwise regular SyncService will be created.
        FakeSyncServiceImpl fakeSyncService = overrideSyncService();
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        // Create the activity.
        final PassphraseActivity activity = launchPassphraseActivity();
        Assert.assertNotNull(activity);

        ThreadUtils.runOnUiThreadBlocking(() -> fakeSyncService.setEngineInitialized(true));
        final PassphraseDialogFragment fragment =
                ActivityTestUtils.waitForFragment(activity, PassphraseActivity.FRAGMENT_PASSPHRASE);
        Assert.assertTrue(fragment.isAdded());

        // Finish the activity before resetting the state.
        activity.finish();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    @Test
    @MediumTest
    @Feature({"Sync"})
    public void testRecreateKeepsSingleRestoredPassphraseDialog() {
        // Override before signing in, otherwise regular SyncService will be created.
        FakeSyncServiceImpl fakeSyncService = overrideSyncService();
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        PassphraseActivity activity = launchPassphraseActivity();
        ThreadUtils.runOnUiThreadBlocking(() -> fakeSyncService.setEngineInitialized(true));
        ActivityTestUtils.waitForFragment(activity, PassphraseActivity.FRAGMENT_PASSPHRASE);

        // The Profile is already loaded, so it resolves synchronously and the restored dialog is
        // kept rather than shown a second time.
        PassphraseActivity recreated = ApplicationTestUtils.recreateActivity(activity);
        PassphraseDialogFragment fragment =
                ActivityTestUtils.waitForFragment(
                        recreated, PassphraseActivity.FRAGMENT_PASSPHRASE);
        Assert.assertTrue(fragment.isAdded());
        Assert.assertEquals(1, countPassphraseDialogs(recreated));

        // Finish the activity before resetting the state.
        ApplicationTestUtils.finishActivity(recreated);
    }

    @Test
    @MediumTest
    @Feature({"Sync"})
    public void testRecreateWithRestoredPassphraseDialogAndAsyncProfile() {
        // Override before signing in, otherwise regular SyncService will be created.
        FakeSyncServiceImpl fakeSyncService = overrideSyncService();
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        PassphraseActivity activity = launchPassphraseActivity();
        ThreadUtils.runOnUiThreadBlocking(() -> fakeSyncService.setEngineInitialized(true));
        ActivityTestUtils.waitForFragment(activity, PassphraseActivity.FRAGMENT_PASSPHRASE);

        // Defer Profile resolution for the recreated activity, as happens when the Profile has to
        // be loaded from disk.
        Profile regularProfile =
                ThreadUtils.runOnUiThreadBlocking(ProfileManager::getLastUsedRegularProfile);
        AtomicReference<Callback<@Nullable Profile>> pendingCallback = deferProfileResolution();

        // Recreating restores the passphrase dialog while the Profile is still pending. This must
        // not create the dialog before the Profile is available.
        PassphraseActivity recreated = ApplicationTestUtils.recreateActivity(activity);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertNotNull(pendingCallback.get());
                    Assert.assertNull(
                            recreated
                                    .getSupportFragmentManager()
                                    .findFragmentByTag(PassphraseActivity.FRAGMENT_PASSPHRASE));
                    pendingCallback.get().onResult(regularProfile);
                });

        // Once the Profile is available, the passphrase dialog is shown again exactly once.
        PassphraseDialogFragment fragment =
                ActivityTestUtils.waitForFragment(
                        recreated, PassphraseActivity.FRAGMENT_PASSPHRASE);
        Assert.assertTrue(fragment.isAdded());
        Assert.assertEquals(1, countPassphraseDialogs(recreated));

        // Finish the activity before resetting the state.
        ApplicationTestUtils.finishActivity(recreated);
    }

    @Test
    @MediumTest
    @Feature({"Sync"})
    public void testRecreateWithRestoredSpinnerAndPassphraseDialogsAndAsyncProfile() {
        // Override before signing in, otherwise regular SyncService will be created.
        FakeSyncServiceImpl fakeSyncService = overrideSyncService();
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        // The engine isn't initialized yet, so the spinner is shown first. Once it is, the
        // passphrase dialog is shown on top of the spinner, leaving two back stack entries.
        PassphraseActivity activity = launchPassphraseActivity();
        ActivityTestUtils.waitForFragment(activity, PassphraseActivity.FRAGMENT_SPINNER);
        ThreadUtils.runOnUiThreadBlocking(() -> fakeSyncService.setEngineInitialized(true));
        ActivityTestUtils.waitForFragment(activity, PassphraseActivity.FRAGMENT_PASSPHRASE);
        Assert.assertEquals(
                2,
                (int)
                        ThreadUtils.runOnUiThreadBlocking(
                                () ->
                                        activity.getSupportFragmentManager()
                                                .getBackStackEntryCount()));

        Profile regularProfile =
                ThreadUtils.runOnUiThreadBlocking(ProfileManager::getLastUsedRegularProfile);
        AtomicReference<Callback<@Nullable Profile>> pendingCallback = deferProfileResolution();

        // Recreating while the Profile is pending drops every restored dialog, not only the top
        // one.
        PassphraseActivity recreated = ApplicationTestUtils.recreateActivity(activity);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertNotNull(pendingCallback.get());
                    FragmentManager fragmentManager = recreated.getSupportFragmentManager();
                    Assert.assertEquals(0, fragmentManager.getBackStackEntryCount());
                    Assert.assertNull(
                            fragmentManager.findFragmentByTag(PassphraseActivity.FRAGMENT_SPINNER));
                    Assert.assertNull(
                            fragmentManager.findFragmentByTag(
                                    PassphraseActivity.FRAGMENT_PASSPHRASE));
                    pendingCallback.get().onResult(regularProfile);
                });

        // Once the Profile is available, only the passphrase dialog is shown, exactly once.
        PassphraseDialogFragment fragment =
                ActivityTestUtils.waitForFragment(
                        recreated, PassphraseActivity.FRAGMENT_PASSPHRASE);
        Assert.assertTrue(fragment.isAdded());
        Assert.assertEquals(1, countPassphraseDialogs(recreated));
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        Assert.assertNull(
                                recreated
                                        .getSupportFragmentManager()
                                        .findFragmentByTag(PassphraseActivity.FRAGMENT_SPINNER)));

        // Finish the activity before resetting the state.
        ApplicationTestUtils.finishActivity(recreated);
    }

    @Test
    @MediumTest
    @Feature({"Sync"})
    public void testProfileAvailableAfterFinishShowsNoDialog() throws Exception {
        // Override before signing in, otherwise regular SyncService will be created.
        FakeSyncServiceImpl fakeSyncService = overrideSyncService();
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        ThreadUtils.runOnUiThreadBlocking(() -> fakeSyncService.setEngineInitialized(true));

        Profile regularProfile =
                ThreadUtils.runOnUiThreadBlocking(ProfileManager::getLastUsedRegularProfile);
        AtomicReference<Callback<@Nullable Profile>> pendingCallback = deferProfileResolution();
        Intent intent = createPassphraseIntent(/* addProfileToken= */ true);
        PassphraseActivity activity =
                ApplicationTestUtils.waitForActivityWithClass(
                        PassphraseActivity.class,
                        Stage.RESUMED,
                        () -> mContext.startActivity(intent));

        AtomicReference<@Nullable Profile> profileWhenChecked = new AtomicReference<>();
        AtomicBoolean dialogAdded = new AtomicBoolean();
        CallbackHelper checked = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertNotNull(pendingCallback.get());
                    // Resolving the Profile posts onProfileAvailable() to the main looper, so it
                    // runs after finish() but before the activity is paused.
                    pendingCallback.get().onResult(regularProfile);
                    activity.finish();
                    // Posted after onProfileAvailable(), so this runs once it has.
                    new Handler(Looper.getMainLooper())
                            .post(
                                    () -> {
                                        profileWhenChecked.set(activity.getProfile());
                                        dialogAdded.set(hasDialog(activity));
                                        checked.notifyCalled();
                                    });
                });
        checked.waitForOnly();
        Assert.assertEquals(regularProfile, profileWhenChecked.get());
        Assert.assertFalse(dialogAdded.get());

        ApplicationTestUtils.waitForActivityState(activity, Stage.DESTROYED);
    }

    // TODO(crbug.com/585922618): Remove once intents posted without a Profile token have expired.
    @Test
    @MediumTest
    @Feature({"Sync"})
    public void testLaunchPassphraseDialogWithoutProfileToken() {
        // Override before signing in, otherwise regular SyncService will be created.
        FakeSyncServiceImpl fakeSyncService = overrideSyncService();
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        PassphraseActivity activity = launchPassphraseActivity(/* addProfileToken= */ false);
        Assert.assertNotNull(activity);

        ThreadUtils.runOnUiThreadBlocking(() -> fakeSyncService.setEngineInitialized(true));
        PassphraseDialogFragment fragment =
                ActivityTestUtils.waitForFragment(activity, PassphraseActivity.FRAGMENT_PASSPHRASE);
        Assert.assertTrue(fragment.isAdded());

        // Finish the activity before resetting the state.
        ApplicationTestUtils.finishActivity(activity);
    }

    private PassphraseActivity launchPassphraseActivity() {
        return launchPassphraseActivity(/* addProfileToken= */ true);
    }

    private PassphraseActivity launchPassphraseActivity(boolean addProfileToken) {
        mContext.startActivity(createPassphraseIntent(addProfileToken));
        return ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), PassphraseActivity.class);
    }

    private Intent createPassphraseIntent(boolean addProfileToken) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setComponent(new ComponentName(mContext, PassphraseActivity.class));
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        // This activity will become the start of a new task on this history stack.
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        // Clears the task stack above this activity if it already exists.
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        if (addProfileToken) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        ProfileTokenUtils.addProfileToIntent(
                                ProfileManager.getLastUsedRegularProfile(), intent);
                    });
        }
        return intent;
    }

    /**
     * Defers resolving Profile tokens, as happens when the Profile has to be loaded from disk.
     *
     * @return Holds the callback of the latest pending resolution once one is requested.
     */
    private static AtomicReference<Callback<@Nullable Profile>> deferProfileResolution() {
        AtomicReference<Callback<@Nullable Profile>> pendingCallback = new AtomicReference<>();
        ProfileResolver.Natives realNatives = ProfileResolverJni.get();
        ProfileResolverJni.setInstanceForTesting(
                new ProfileResolver.Natives() {
                    @Override
                    public String tokenizeProfile(@Nullable Profile profile) {
                        return realNatives.tokenizeProfile(profile);
                    }

                    @Override
                    public String tokenizeProfileKey(@Nullable ProfileKey profileKey) {
                        return realNatives.tokenizeProfileKey(profileKey);
                    }

                    @Override
                    public void resolveProfile(
                            @Nullable String token, Callback<@Nullable Profile> callback) {
                        pendingCallback.set(callback);
                    }

                    @Override
                    public byte[] tokenizeProfileToBytes(Profile profile) {
                        return realNatives.tokenizeProfileToBytes(profile);
                    }

                    @Override
                    public void resolveProfileFromBytes(
                            byte[] token, Callback<@Nullable Profile> callback) {
                        pendingCallback.set(callback);
                    }

                    @Override
                    public void resolveProfileKey(
                            @Nullable String token, Callback<@Nullable ProfileKey> callback) {
                        realNatives.resolveProfileKey(token, callback);
                    }
                });
        return pendingCallback;
    }

    /** Returns whether a passphrase or spinner dialog is added, once pending commits have run. */
    private static boolean hasDialog(PassphraseActivity activity) {
        FragmentManager fragmentManager = activity.getSupportFragmentManager();
        if (!fragmentManager.isDestroyed()) fragmentManager.executePendingTransactions();
        return fragmentManager.findFragmentByTag(PassphraseActivity.FRAGMENT_PASSPHRASE) != null
                || fragmentManager.findFragmentByTag(PassphraseActivity.FRAGMENT_SPINNER) != null;
    }

    private static int countPassphraseDialogs(PassphraseActivity activity) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int count = 0;
                    for (Fragment fragment : activity.getSupportFragmentManager().getFragments()) {
                        if (PassphraseActivity.FRAGMENT_PASSPHRASE.equals(fragment.getTag())) {
                            count++;
                        }
                    }
                    return count;
                });
    }

    private FakeSyncServiceImpl overrideSyncService() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // PSS has to be constructed on the UI thread.
                    FakeSyncServiceImpl fakeSyncService = new FakeSyncServiceImpl();
                    SyncServiceFactory.setInstanceForTesting(fakeSyncService);
                    return fakeSyncService;
                });
    }
}
