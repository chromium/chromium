// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.preference.PreferenceFragmentCompat;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;
import org.chromium.components.browser_ui.settings.SettingsItemBackgroundDecoration;
import org.chromium.components.browser_ui.settings.SettingsStylingController;

import java.util.Objects;

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
                CustomDividerFragment {
    private Profile mProfile;
    private SettingsCustomTabLauncher mCustomTabLauncher;

    /**
     * The item decoration that applies the background to the settings items. Null if the settings
     * containment feature is not enabled.
     */
    private @Nullable SettingsItemBackgroundDecoration mItemBackgroundDecoration;

    @NonNull
    @Override
    public RecyclerView onCreateRecyclerView(
            @NonNull LayoutInflater inflater,
            @NonNull ViewGroup parent,
            @Nullable Bundle savedInstanceState) {
        RecyclerView recyclerView =
                super.onCreateRecyclerView(inflater, parent, savedInstanceState);

        if (ChromeFeatureList.sAndroidSettingsContainment.isEnabled()) {
            mItemBackgroundDecoration = new SettingsItemBackgroundDecoration(getContext());
            recyclerView.addItemDecoration(mItemBackgroundDecoration);
        }
        return recyclerView;
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        if (ChromeFeatureList.sAndroidSettingsContainment.isEnabled()) {
            updateBackgrounds(getListView());
        }
    }

    /** Updates the background of all the visible preferences on the settings screen. */
    protected void updateBackgrounds(RecyclerView recyclerView) {
        recyclerView.post(
                () -> {
                    if (mItemBackgroundDecoration == null) return;
                    SettingsStylingController stylingController =
                            new SettingsStylingController(
                                    Objects.requireNonNull(getContext()), getPreferenceScreen());

                    mItemBackgroundDecoration.updateBackgroundStyleDetails(
                            stylingController.generateBackgroundStyleDetails());
                    recyclerView.invalidateItemDecorations();
                });
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

    // CustomDividerFragment implementation.
    /** Returns whether the divider should be shown. */
    @Override
    public boolean hasDivider() {
        return !ChromeFeatureList.sAndroidSettingsContainment.isEnabled();
    }

    /**
     * @return The launcher for help and feedback actions.
     */
    public HelpAndFeedbackLauncher getHelpAndFeedbackLauncher() {
        return HelpAndFeedbackLauncherFactory.getForProfile(mProfile);
    }

    /**
     * @return The launcher for CCT.
     */
    public SettingsCustomTabLauncher getCustomTabLauncher() {
        return mCustomTabLauncher;
    }
}
