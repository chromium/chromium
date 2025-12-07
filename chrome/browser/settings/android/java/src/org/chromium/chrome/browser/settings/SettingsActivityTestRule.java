// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.runner.lifecycle.Stage;

import org.junit.Assert;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.components.browser_ui.settings.SettingsNavigation;

/**
 * Activity test rule that launch {@link SettingsActivity} in tests.
 *
 * <p>Noting that the activity is not starting after the test rule created. The user have to call
 * {@link #startSettingsActivity()} explicitly to launch the settings activity.
 *
 * @param <T> Fragment that will be attached to the SettingsActivity.
 */
public class SettingsActivityTestRule<T extends Fragment>
        extends BaseActivityTestRule<SettingsActivity> {
    private final Class<T> mFragmentClass;
    private @Nullable Bundle mDefaultFragmentArgs;

    /**
     * Create the settings activity test rule with an specific fragment class.
     * @param fragmentClass Fragment that will be attached after the activity starts.
     */
    public SettingsActivityTestRule(Class<T> fragmentClass) {
        super(SettingsActivity.class);
        mFragmentClass = fragmentClass;
    }

    /**
     * Create the settings activity test rule with an specific fragment class.
     * @param fragmentClass Fragment that will be attached after the activity starts.
     * @param defaultFragmentArgs A bundle of default fragment arguments to be used.
     */
    public SettingsActivityTestRule(Class<T> fragmentClass, @NonNull Bundle defaultFragmentArgs) {
        super(SettingsActivity.class);
        mFragmentClass = fragmentClass;
        mDefaultFragmentArgs = defaultFragmentArgs;
    }

    /**
     * Launches the settings activity with the specified fragment.
     * @return The activity that just started.
     */
    public SettingsActivity startSettingsActivity() {
        return startSettingsActivity(mDefaultFragmentArgs);
    }

    /**
     * Launches the settings activity with the specified fragment and arguments.
     *
     * @param fragmentArgs A bundle of additional fragment arguments.
     * @return The activity that just started.
     */
    public SettingsActivity startSettingsActivity(Bundle fragmentArgs) {
        Context context = ApplicationProvider.getApplicationContext();
        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        Intent intent =
                settingsNavigation.createSettingsIntent(context, mFragmentClass, fragmentArgs);
        launchActivity(intent);
        ApplicationTestUtils.waitForActivityState(getActivity(), Stage.RESUMED);
        return getActivity();
    }

    /**
     * @return The fragment attached to the SettingsActivity.
     */
    public T getFragment() {
        Assert.assertNotNull("#getFragment is called before activity launch.", getActivity());

        Fragment fragment = getActivity().getMainFragment();
        Assert.assertNotNull(fragment);
        return (T) fragment;
    }
}
