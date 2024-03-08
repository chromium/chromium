// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import android.accounts.Account;
import android.app.Dialog;
import android.app.ProgressDialog;
import android.os.Bundle;

import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncErrorNotifier;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;

/**
 * This activity is used for requesting a sync passphrase from the user. Typically, this will be the
 * target of an Android notification.
 */
public class PassphraseActivity extends AppCompatActivity
        implements PassphraseDialogFragment.Delegate, FragmentManager.OnBackStackChangedListener {
    public static final String FRAGMENT_PASSPHRASE = "passphrase_fragment";
    public static final String FRAGMENT_SPINNER = "spinner_fragment";

    private Profile mProfile;
    private IdentityManager mIdentityManager;
    private SyncService mSyncService;

    private SyncService.SyncStateChangedListener mSyncStateChangedListener;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // The Chrome browser process must be started here because this Activity
        // may be started explicitly from Android notifications.
        // During a normal user flow the ChromeTabbedActivity would start the Chrome browser
        // process and this wouldn't be necessary.
        ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
        mProfile = ProfileManager.getLastUsedRegularProfile();
        mIdentityManager = IdentityServicesProvider.get().getIdentityManager(mProfile);
        mSyncService = SyncServiceFactory.getForProfile(mProfile);
        assert mSyncService != null;
        getSupportFragmentManager().addOnBackStackChangedListener(this);
    }

    @Override
    protected void onResume() {
        super.onResume();
        Account account =
                CoreAccountInfo.getAndroidAccountFrom(
                        mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN));
        if (account == null) {
            finish();
            return;
        }

        if (!isShowingDialog(FRAGMENT_PASSPHRASE)) {
            if (mSyncService.isEngineInitialized()) {
                displayPassphraseDialog();
            } else {
                addSyncStateChangedListener();
                displaySpinnerDialog();
            }
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        // Make sure we don't receive callbacks while in the background.
        // See http://crbug.com/469890.
        removeSyncStateChangedListener();
    }

    private void addSyncStateChangedListener() {
        if (mSyncStateChangedListener != null) {
            return;
        }
        mSyncStateChangedListener =
                new SyncService.SyncStateChangedListener() {
                    @Override
                    public void syncStateChanged() {
                        if (mSyncService.isEngineInitialized()) {
                            removeSyncStateChangedListener();
                            displayPassphraseDialog();
                        }
                    }
                };
        mSyncService.addSyncStateChangedListener(mSyncStateChangedListener);
    }

    private void removeSyncStateChangedListener() {
        if (mSyncStateChangedListener != null) {
            mSyncService.removeSyncStateChangedListener(mSyncStateChangedListener);
            mSyncStateChangedListener = null;
        }
    }

    private boolean isShowingDialog(String tag) {
        return getFragmentManager().findFragmentByTag(tag) != null;
    }

    private void displayPassphraseDialog() {
        assert mSyncService.isEngineInitialized();
        FragmentTransaction ft = getSupportFragmentManager().beginTransaction();
        ft.addToBackStack(null);
        PassphraseDialogFragment.newInstance(null).show(ft, FRAGMENT_PASSPHRASE);
    }

    private void displaySpinnerDialog() {
        FragmentTransaction ft = getSupportFragmentManager().beginTransaction();
        ft.addToBackStack(null);
        SpinnerDialogFragment dialog = new SpinnerDialogFragment();
        dialog.show(ft, FRAGMENT_SPINNER);
    }

    /** Callback for {@link PassphraseDialogFragment.Delegate} */
    @Override
    public boolean onPassphraseEntered(String passphrase) {
        if (!passphrase.isEmpty() && mSyncService.setDecryptionPassphrase(passphrase)) {
            // The passphrase was correct - close this activity.
            finish();
            return true;
        }
        return false;
    }

    @Override
    public void onPassphraseCanceled() {
        // Re add the notification.
        SyncErrorNotifier.getForProfile(mProfile).syncStateChanged();
        finish();
    }

    @Override
    public Profile getProfile() {
        return mProfile;
    }

    @Override
    public void onBackStackChanged() {
        if (getSupportFragmentManager().getBackStackEntryCount() == 0) {
            finish();
        }
    }

    /** Dialog shown while sync is loading. */
    public static class SpinnerDialogFragment extends DialogFragment {
        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            ProgressDialog dialog = new ProgressDialog(getActivity());
            dialog.setMessage(getResources().getString(R.string.sync_loading));
            return dialog;
        }
    }
}
