// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;
import android.support.test.rule.ActivityTestRule;

import androidx.fragment.app.Fragment;

import org.hamcrest.Matchers;
import org.junit.Assert;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
/**
 * Activity test rule that launch {@link SettingsActivity} in tests.
 *
 * Noting that the activity is not starting after the test rule created. The user have to call
 * {@link #startSettingsActivity()} explicitly to launch the settings activity.
 *
 * @param <T> Fragment that will be attached to the SettingsActivity.
 */
public class SettingsActivityTestRule<T extends Fragment>
        extends ActivityTestRule<SettingsActivity> {
    private final Class<T> mFragmentClass;

    /**
     * Create the settings activity test rule with an specific fragment class.
     * @param fragmentClass Fragment that will be attached after the activity starts.
     */
    public SettingsActivityTestRule(Class<T> fragmentClass) {
        this(fragmentClass, false);
    }

    /**
     * Create the settings activity test rule with an specific fragment class.
     * @param fragmentClass Fragment that will be attached after the activity starts.
     * @param initialTouchMode Whether in touch mode after the activity starts.
     */
    public SettingsActivityTestRule(Class<T> fragmentClass, boolean initialTouchMode) {
        super(SettingsActivity.class, initialTouchMode, false);
        mFragmentClass = fragmentClass;
    }

    /**
     * Launches the settings activity with the specified fragment.
     * @return The activity that just started.
     */
    public SettingsActivity startSettingsActivity() {
        return startSettingsActivity(null);
    }

    /**
     * Launches the settings activity with the specified fragment and arguments.
     * @param fragmentArgs A bundle of additional fragment arguments.
     * @return The activity that just started.
     */
    public SettingsActivity startSettingsActivity(Bundle fragmentArgs) {
        Context context = InstrumentationRegistry.getTargetContext();
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        Intent intent = settingsLauncher.createSettingsActivityIntent(
                context, mFragmentClass.getName(), fragmentArgs);
        SettingsActivity activity = super.launchActivity(intent);
        Assert.assertNotNull(activity);

        return activity;
    }

    /**
     * We need to ensure that SettingsActivity gets destroyed in the TestRule because sometimes
     * it uses the mock signin environment like fake AccountManagerFacade, if the activity starts
     * with the stub then it also needs to finish with it. That's why we need to wait till the
     * activity state becomes destroyed before tearing down the mock signin environment.
     */
    @Override
    protected void afterActivityFinished() {
        super.afterActivityFinished();
        waitTillActivityIsDestroyed();
    }

    /**
     * Block the execution till the SettingsActivity is destroyed.
     */
    public void waitTillActivityIsDestroyed() {
        SettingsActivity activity = getActivity();
        if (activity != null) {
            CriteriaHelper.pollUiThread(() -> {
                Criteria.checkThat(ApplicationStatus.getStateForActivity(activity),
                        Matchers.is(ActivityState.DESTROYED));
            });
        }
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
