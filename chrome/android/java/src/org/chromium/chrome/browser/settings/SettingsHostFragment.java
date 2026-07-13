// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Hosts settings preference fragments inside a native page. See {@link SettingsPage}. */
@NullMarked
public class SettingsHostFragment extends Fragment
        implements PreferenceFragmentCompat.OnPreferenceStartFragmentCallback {

    public static final String SETTINGS_NATIVE_PAGE_TAG = "settings_native_page";

    private static final int CONTAINER_ID = View.generateViewId();

    private @Nullable Context mThemedContext;

    SettingsHostFragment() {
        assert ChromeFeatureList.sSettingsInTab.isEnabled()
                : "SettingsInTab feature must be enabled to use SettingsHostFragment.";
    }

    @Override
    public void onAttach(Context context) {
        // Ensure child fragments inherit the same Chromium Settings theme used by SettingsActivity.
        // For example, this ensures the left column category labels are styled correctly.
        mThemedContext = new ContextThemeWrapper(context, R.style.Theme_Chromium_Settings);
        super.onAttach(mThemedContext);
    }

    @Override
    public Context getContext() {
        return mThemedContext != null ? mThemedContext : assumeNonNull(super.getContext());
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater,
            @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        FrameLayout frameLayout = new FrameLayout(requireContext());
        frameLayout.setId(CONTAINER_ID);
        return frameLayout;
    }

    @Override
    public void onViewCreated(View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        if (savedInstanceState == null) {
            Fragment initialFragment = createInitialFragment();
            getChildFragmentManager()
                    .beginTransaction()
                    .add(CONTAINER_ID, initialFragment)
                    .commitAllowingStateLoss();
        }
    }

    /**
     * Creates the initial fragment to be shown in the settings page. Allows overrides for testing
     * to use simpler fragments.
     */
    protected Fragment createInitialFragment() {
        return new MultiColumnSettings();
    }

    @Override
    public boolean onPreferenceStartFragment(
            PreferenceFragmentCompat caller, Preference preference) {
        String fragmentClass = preference.getFragment();
        if (fragmentClass == null) return false;

        Fragment fragment =
                Fragment.instantiate(requireContext(), fragmentClass, preference.getExtras());
        return showFragment(fragment, /* addToBackStack= */ true, /* tag= */ null);
    }

    /**
     * Returns whether the fragment is attached to an activity. This method can be mocked in tests,
     * unlike Fragment#isAdded(), which is final.
     */
    boolean isAttachedToActivity() {
        return isAdded();
    }

    /** Returns the currently active fragment hosted by this fragment. */
    public @Nullable Fragment getActiveFragment() {
        return getChildFragmentManager().findFragmentById(CONTAINER_ID);
    }

    /** Returns the active {@link SettingsHostFragment} if attached to the activity, or null. */
    public static @Nullable SettingsHostFragment get(@Nullable Activity activity) {
        if (!(activity instanceof FragmentActivity fragmentActivity)) return null;
        Fragment fragment =
                fragmentActivity
                        .getSupportFragmentManager()
                        .findFragmentByTag(SETTINGS_NATIVE_PAGE_TAG);
        if (fragment instanceof SettingsHostFragment settingsHostFragment
                && settingsHostFragment.isAttachedToActivity()) {
            return settingsHostFragment;
        }
        return null;
    }

    /**
     * Shows a fragment inside the settings native page container or detail pane. Does nothing if
     * the settings tab is not open (and returns false).
     *
     * @param fragment The settings fragment to show. If null, the main settings page will show.
     * @param addToBackStack Whether to add the fragment to the back stack.
     * @param tag The tag to use for the fragment.
     *     <p>TODO(crbug.com/521895796): Provide a mechanism to open settings if it is not open.
     */
    public boolean showFragment(
            @Nullable Fragment fragment, boolean addToBackStack, @Nullable String tag) {
        if (!isAttachedToActivity()) return false;

        Fragment activeFragment = getActiveFragment();
        if (activeFragment instanceof MultiColumnSettings multiColumnSettings) {
            if (fragment == null || fragment instanceof MainSettings) {
                if (multiColumnSettings.getSlidingPaneLayout().isSlideable()) {
                    multiColumnSettings.getSlidingPaneLayout().closePane();
                }
                return true;
            }
            multiColumnSettings.showDetailFragment(fragment, addToBackStack, tag);
            return true;
        }

        if (fragment == null) {
            fragment = createInitialFragment();
        }

        var transaction = getChildFragmentManager().beginTransaction();
        transaction.replace(CONTAINER_ID, fragment);
        if (addToBackStack) {
            transaction.addToBackStack(tag);
        }
        transaction.commitAllowingStateLoss();
        return true;
    }
}
