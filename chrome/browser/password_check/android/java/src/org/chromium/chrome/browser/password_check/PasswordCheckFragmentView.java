// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import android.content.Context;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.fragment.app.DialogFragment;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.password_manager.PasswordCheckReferrer;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;

/** This class is responsible for rendering the check passwords view in the settings menu. */
public class PasswordCheckFragmentView extends PreferenceFragmentCompat
        implements EmbeddableSettingsPage {
    // Key for the argument with which the PasswordsCheck fragment will be launched. The value for
    // this argument should be part of the PasswordCheckReferrer enum, which contains
    // all points of entry to the password check UI.
    public static final String PASSWORD_CHECK_REFERRER = "password-check-referrer";

    private PasswordCheckComponentUi mComponentDelegate;
    private @PasswordCheckReferrer int mPasswordCheckReferrer;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    /**
     * Set the delegate that handles view events which affect the state of the component.
     *
     * @param componentDelegate The {@link PasswordCheckComponentUi} delegate.
     */
    void setComponentDelegate(PasswordCheckComponentUi componentDelegate) {
        mComponentDelegate = componentDelegate;
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        mPageTitle.set(getString(R.string.passwords_check_title));
        setPreferenceScreen(getPreferenceManager().createPreferenceScreen(getStyledContext()));
        mPasswordCheckReferrer = getReferrerFromInstanceStateOrLaunchBundle(savedInstanceState);
        setHasOptionsMenu(true);
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        MenuItem help =
                menu.add(Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, R.string.menu_help);
        help.setIcon(
                TraceEventVectorDrawableCompat.create(
                        getResources(), R.drawable.ic_help_and_feedback, getActivity().getTheme()));
    }

    @Override
    public void onStart() {
        super.onStart();
        mComponentDelegate.onStartFragment();
    }

    @Override
    public void onResume() {
        super.onResume();
        mComponentDelegate.onResumeFragment();
    }

    private @PasswordCheckReferrer int getReferrerFromInstanceStateOrLaunchBundle(
            Bundle savedInstanceState) {
        if (savedInstanceState != null && savedInstanceState.containsKey(PASSWORD_CHECK_REFERRER)) {
            return savedInstanceState.getInt(PASSWORD_CHECK_REFERRER);
        }
        Bundle extras = getArguments();
        assert extras.containsKey(PASSWORD_CHECK_REFERRER)
                : "PasswordCheckFragmentView must be launched with a password-check-referrer"
                        + " fragment argument, but none was provided.";
        return extras.getInt(PASSWORD_CHECK_REFERRER);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        // The component should only be destroyed when the activity has been closed by the user
        // (e.g. by pressing on the back button) and not when the activity is temporarily destroyed
        // by the system.
        mComponentDelegate.onDestroyFragment();
        if (getActivity().isFinishing()
                && (mPasswordCheckReferrer == PasswordCheckReferrer.LEAK_DIALOG
                        || mPasswordCheckReferrer
                                == PasswordCheckReferrer.PHISHED_WARNING_DIALOG)) {
            mComponentDelegate.destroy();
        }
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putInt(PASSWORD_CHECK_REFERRER, mPasswordCheckReferrer);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        return mComponentDelegate.handleHelp(item);
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }

    void showDialogFragment(DialogFragment passwordCheckDeletionDialogFragment) {
        if (passwordCheckDeletionDialogFragment == null) return;
        passwordCheckDeletionDialogFragment.show(getParentFragmentManager(), null);
    }

    @PasswordCheckReferrer
    int getReferrer() {
        return mPasswordCheckReferrer;
    }
}
