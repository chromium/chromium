// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.settings.FragmentSettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.site_settings.ContentSettingsResources;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;

public abstract class SafetyHubBaseFragment extends ChromeBaseSettingsFragment
        implements FragmentSettingsLauncher {
    private SnackbarManager mSnackbarManager;
    private SettingsLauncher mSettingsLauncher;

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        // Disable animations of preference changes.
        getListView().setItemAnimator(null);
    }

    @Override
    public void setSettingsLauncher(SettingsLauncher settingsLauncher) {
        mSettingsLauncher = settingsLauncher;
    }

    public void setSnackbarManager(SnackbarManager snackbarManager) {
        mSnackbarManager = snackbarManager;
    }

    protected void showSnackbar(
            String text,
            int identifier,
            SnackbarManager.SnackbarController controller,
            Object actionData) {
        if (mSnackbarManager != null) {
            showSnackbar(mSnackbarManager, text, identifier, controller, actionData);
        }
    }

    protected void showSnackbarOnLastFocusedActivity(
            String text,
            int identifier,
            SnackbarManager.SnackbarController controller,
            Object actionData) {
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (activity instanceof SnackbarManager.SnackbarManageable) {
            SnackbarManager snackbarManager =
                    ((SnackbarManager.SnackbarManageable) activity).getSnackbarManager();
            if (snackbarManager != null) {
                showSnackbar(snackbarManager, text, identifier, controller, actionData);
            }
        }
    }

    private void showSnackbar(
            SnackbarManager snackbarManager,
            String text,
            int identifier,
            SnackbarManager.SnackbarController controller,
            Object actionData) {
        var snackbar = Snackbar.make(text, controller, Snackbar.TYPE_ACTION, identifier);
        snackbar.setAction(getString(R.string.undo), actionData);
        snackbar.setSingleLine(false);

        snackbarManager.showSnackbar(snackbar);
    }

    protected void launchSettingsActivity(Class<? extends Fragment> fragment) {
        if (mSettingsLauncher != null) {
            mSettingsLauncher.launchSettingsActivity(getContext(), fragment);
        }
    }

    protected void launchSiteSettingsActivity(@SiteSettingsCategory.Type int category) {
        if (mSettingsLauncher == null) {
            return;
        }

        Bundle extras = new Bundle();
        extras.putString(
                SingleCategorySettings.EXTRA_CATEGORY,
                SiteSettingsCategory.preferenceKey(category));
        extras.putString(
                SingleCategorySettings.EXTRA_TITLE,
                getContext().getString(ContentSettingsResources.getTitleForCategory(category)));

        mSettingsLauncher.launchSettingsActivity(
                getContext(), SingleCategorySettings.class, extras);
    }
}
