// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.core.view.MenuProvider;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;

/** A MenuProvider that adds a help icon to the toolbar and handles its clicks. */
@NullMarked
public class AutofillHelpMenuProvider implements MenuProvider {
    private final ChromeBaseSettingsFragment mFragment;

    public AutofillHelpMenuProvider(ChromeBaseSettingsFragment fragment) {
        mFragment = fragment;
    }

    @Override
    public void onCreateMenu(Menu menu, MenuInflater menuInflater) {
        menu.clear();
        MenuItem help =
                menu.add(Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, R.string.menu_help);
        help.setIcon(R.drawable.ic_help_24dp);
    }

    @Override
    public boolean onMenuItemSelected(MenuItem menuItem) {
        if (menuItem.getItemId() == R.id.menu_id_targeted_help) {
            mFragment
                    .getHelpAndFeedbackLauncher()
                    .show(
                            mFragment.requireActivity(),
                            mFragment.requireActivity().getString(R.string.help_context_autofill),
                            /* url= */ null);
            return true;
        }
        return false;
    }
}
