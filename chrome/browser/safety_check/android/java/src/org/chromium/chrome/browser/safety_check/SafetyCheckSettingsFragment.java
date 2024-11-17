// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.ui.widget.ButtonCompat;

/** Settings fragment containing Safety check. This class represents a View in the MVC paradigm. */
public class SafetyCheckSettingsFragment extends PreferenceFragmentCompat
        implements EmbeddableSettingsPage {
    private static final String SAFETY_CHECK_IMMEDIATE_RUN =
            "SafetyCheckSettingsFragment.safetyCheckImmediateRun";

    /** The "Check" button at the bottom that needs to be added after the View is inflated. */
    private ButtonCompat mCheckButton;

    private TextView mTimestampTextView;

    private boolean mRunSafetyCheckImmediately;

    private SafetyCheckComponentUi mComponentDelegate;

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    /** Initializes all the objects related to the preferences page. */
    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        // Add all preferences and set the title.
        SettingsUtils.addPreferencesFromResource(this, R.xml.safety_check_preferences);
        mPageTitle.set(getString(R.string.prefs_safety_check));

        mRunSafetyCheckImmediately =
                getArguments() != null
                        && getArguments().containsKey(SAFETY_CHECK_IMMEDIATE_RUN)
                        && getArguments().getBoolean(SAFETY_CHECK_IMMEDIATE_RUN);
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        LinearLayout view =
                (LinearLayout) super.onCreateView(inflater, container, savedInstanceState);
        // Add a button to the bottom of the preferences view.
        LinearLayout bottomView =
                (LinearLayout) inflater.inflate(R.layout.safety_check_bottom_elements, view, false);
        mCheckButton = (ButtonCompat) bottomView.findViewById(R.id.safety_check_button);
        mTimestampTextView = (TextView) bottomView.findViewById(R.id.safety_check_timestamp);
        view.addView(bottomView);
        setPasswordChecks();
        return view;
    }

    private void setPasswordChecks() {
        findPreference(SafetyCheckViewBinder.PASSWORDS_KEY_ACCOUNT)
                .setVisible(mComponentDelegate.isAccountPasswordStorageUsed());
        findPreference(SafetyCheckViewBinder.PASSWORDS_KEY_LOCAL)
                .setVisible(mComponentDelegate.isLocalPasswordStorageUsed());
    }

    /**
     * Sets the delegate, which exposes the UI related logic of the safety check component to the
     * fragment view.
     *
     * @param componentDelegate The {@link SafetyCheckComponentUi} delegate.
     */
    public void setComponentDelegate(SafetyCheckComponentUi componentDelegate) {
        mComponentDelegate = componentDelegate;
    }

    /**
     * @return A {@link ButtonCompat} object for the Check button.
     */
    ButtonCompat getCheckButton() {
        return mCheckButton;
    }

    /**
     * @return A {@link TextView} object for the last run timestamp.
     */
    TextView getTimestampTextView() {
        return mTimestampTextView;
    }

    /**
     * Update the status string of a given Safety check element, e.g. Passwords.
     * @param key An android:key String corresponding to Safety check element.
     * @param statusString Resource ID of the new status string.
     */
    public void updateElementStatus(String key, int statusString) {
        if (statusString != 0) {
            updateElementStatus(key, getContext().getString(statusString));
        } else {
            updateElementStatus(key, "");
        }
    }

    /**
     * Update the status string of a given Safety check element, e.g. Passwords.
     * @param key An android:key String corresponding to Safety check element.
     * @param statusString The new status string.
     */
    public void updateElementStatus(String key, String statusString) {
        Preference p = findPreference(key);
        // If this is invoked before the preferences are created, do nothing.
        if (p == null) {
            return;
        }
        p.setSummary(statusString);
    }

    /**
     * Creates a bundle for this fragment.
     * @param runSafetyCheckImmediately Whether the afety check should be run right after the
     *         fragment is opened.
     */
    public static Bundle createBundle(boolean runSafetyCheckImmediately) {
        Bundle result = new Bundle();
        result.putBoolean(SAFETY_CHECK_IMMEDIATE_RUN, runSafetyCheckImmediately);
        return result;
    }

    /**
     * @return Whether safety check need to be run immediately once the safety check settings is
     *         fully initialized.
     */
    boolean shouldRunSafetyCheckImmediately() {
        return mRunSafetyCheckImmediately;
    }

    @Override
    public void onPause() {
        super.onPause();
        mRunSafetyCheckImmediately = false;
    }
}
