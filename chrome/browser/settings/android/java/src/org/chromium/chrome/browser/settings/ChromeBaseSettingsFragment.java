// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.os.Bundle;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.annotation.StringRes;
import androidx.core.view.MenuProvider;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.PreferenceUpdateObserver;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;

/**
 * Base class for settings in Chrome.
 *
 * <p>Common dependencies needed by the vast majority of settings screens can be added here for
 * convenience.
 */
@NullMarked
public abstract class ChromeBaseSettingsFragment extends PreferenceFragmentCompat
        implements EmbeddableSettingsPage,
                ProfileDependentSetting,
                SettingsCustomTabLauncher.SettingsCustomTabLauncherClient,
                CustomDividerFragment,
                PreferenceUpdateObserver.Provider {
    private Profile mProfile;
    private SettingsCustomTabLauncher mCustomTabLauncher;
    private @Nullable PreferenceUpdateObserver mPreferenceUpdateObserver;
    private @Nullable Context mThemedContext;
    private @Nullable MenuProvider mMenuProvider;

    @Override
    public void onAttach(Context context) {
        if (!SettingsInTab.isEnabled()) {
            super.onAttach(context);
            return;
        }

        // Ensure settings fragments inherit the same Chromium Settings theme used by
        // SettingsActivity, even though they are hosted in ChromeTabbedActivity.
        mThemedContext = new ContextThemeWrapper(context, R.style.ThemeOverlay_Chromium_Settings);
        super.onAttach(mThemedContext);
    }

    @Override
    public Context getContext() {
        return mThemedContext != null ? mThemedContext : assumeNonNull(super.getContext());
    }

    @Override
    public LayoutInflater onGetLayoutInflater(@Nullable Bundle savedInstanceState) {
        LayoutInflater inflater = super.onGetLayoutInflater(savedInstanceState);
        // Ensure we use the themed context if available.
        return inflater.cloneInContext(getContext());
    }

    /**
     * @return The profile associated with the current Settings screen.
     */
    public Profile getProfile() {
        assert mProfile != null : "Attempting to use the profile before initialization.";
        return mProfile;
    }

    @Initializer
    @Override
    public void setProfile(Profile profile) {
        mProfile = profile;
    }

    @Initializer
    @Override
    public void setCustomTabLauncher(SettingsCustomTabLauncher customTabLauncher) {
        mCustomTabLauncher = customTabLauncher;
    }

    @Override
    public void setPreferenceUpdateObserver(PreferenceUpdateObserver observer) {
        mPreferenceUpdateObserver = observer;
    }

    @Override
    public void removePreferenceUpdateObserver() {
        mPreferenceUpdateObserver = null;
    }

    // CustomDividerFragment implementation.
    /** Returns whether the divider should be shown. */
    @Override
    public boolean hasDivider() {
        return false;
    }

    /**
     * @return The launcher for help and feedback actions.
     */
    public HelpAndFeedbackLauncher getHelpAndFeedbackLauncher() {
        return HelpAndFeedbackLauncherFactory.getForProfile(mProfile);
    }

    /**
     * @return The resource ID of the help string that is valid for the current policy.
     */
    protected @StringRes int getHelpMenuStringRes() {
        return HelpAndFeedbackLauncher.getHelpMenuStringRes();
    }

    /**
     * @return The launcher for CCT.
     */
    public SettingsCustomTabLauncher getCustomTabLauncher() {
        return mCustomTabLauncher;
    }

    /**
     * Sets the {@link MenuProvider} for this fragment.
     *
     * <p>When {@link SettingsInTab} is enabled, Settings is hosted in a browser tab using a
     * standalone {@link androidx.appcompat.widget.Toolbar} managed by {@link SettingsMenuHelper},
     * rather than an Activity Action Bar. If a fragment uses AndroidX's modern {@link MenuProvider}
     * API (e.g. via {@link androidx.core.view.MenuHost#addMenuProvider}) instead of overriding
     * {@link #onCreateOptionsMenu} and {@link #onOptionsItemSelected}, call this method so the
     * fragment's options menu callbacks are forwarded to the {@link MenuProvider}.
     */
    public void setMenuProvider(MenuProvider menuProvider) {
        mMenuProvider = menuProvider;
        setHasOptionsMenu(true);
    }

    /** Returns the {@link MenuProvider} for this fragment, if any. */
    public @Nullable MenuProvider getMenuProvider() {
        return mMenuProvider;
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        if (mMenuProvider != null) {
            mMenuProvider.onCreateMenu(menu, inflater);
            return;
        }
        super.onCreateOptionsMenu(menu, inflater);
    }

    @Override
    public void onPrepareOptionsMenu(Menu menu) {
        if (mMenuProvider != null) {
            mMenuProvider.onPrepareMenu(menu);
            return;
        }
        super.onPrepareOptionsMenu(menu);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (mMenuProvider != null && mMenuProvider.onMenuItemSelected(item)) {
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    /** Notifies the observer that the preferences have been updated. */
    protected void notifyPreferencesUpdated() {
        if (mPreferenceUpdateObserver != null) {
            mPreferenceUpdateObserver.onPreferencesUpdated(this);
        }
    }
}
