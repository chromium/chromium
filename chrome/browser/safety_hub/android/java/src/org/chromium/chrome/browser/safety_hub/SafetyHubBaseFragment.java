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
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.site_settings.ContentSettingsResources;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;

public abstract class SafetyHubBaseFragment extends ChromeBaseSettingsFragment {
    private OneshotSupplier<SnackbarManager> mSnackbarManagerSupplier;

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        // Disable animations of preference changes.
        getListView().setItemAnimator(null);
    }

    public void setSnackbarManagerSupplier(
            OneshotSupplier<SnackbarManager> snackbarManagerSupplier) {
        mSnackbarManagerSupplier = snackbarManagerSupplier;
    }

    protected void showSnackbar(
            String text,
            int identifier,
            SnackbarManager.SnackbarController controller,
            Object actionData) {
        if (mSnackbarManagerSupplier.hasValue()) {
            showSnackbar(mSnackbarManagerSupplier.get(), text, identifier, controller, actionData);
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

    protected void startSettings(Class<? extends Fragment> fragment) {
        SettingsNavigationFactory.createSettingsNavigation().startSettings(getContext(), fragment);
    }

    protected void launchSiteSettingsActivity(@SiteSettingsCategory.Type int category) {
        Bundle extras = new Bundle();
        extras.putString(
                SingleCategorySettings.EXTRA_CATEGORY,
                SiteSettingsCategory.preferenceKey(category));
        extras.putString(
                SingleCategorySettings.EXTRA_TITLE,
                getContext().getString(ContentSettingsResources.getTitleForCategory(category)));

        SettingsNavigationFactory.createSettingsNavigation()
                .startSettings(getContext(), SingleCategorySettings.class, extras);
    }
}
