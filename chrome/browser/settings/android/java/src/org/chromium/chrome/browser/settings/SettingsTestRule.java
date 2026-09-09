// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.runner.lifecycle.Stage;

import org.junit.Assert;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.components.browser_ui.settings.SettingsNavigation;

import java.util.EnumSet;

/**
 * Activity test rule that launches {@link SettingsActivity} or {@link SettingsInTabTestActivity} in
 * tests.
 *
 * <p>Note that the activity is not starting after the test rule is created. The user has to call
 * {@link #startSettingsActivity()} explicitly to launch the activity.
 *
 * @param <T> Fragment that will be attached to the activity.
 */
public class SettingsTestRule<T extends Fragment>
        extends BaseActivityTestRule<ChromeBaseAppCompatActivity> {
    private final @Nullable Class<T> mFragmentClass;
    private @Nullable Bundle mDefaultFragmentArgs;

    /**
     * Create the settings activity test rule with a specific fragment class.
     *
     * @param fragmentClass Fragment that will be attached after the activity starts.
     */
    public SettingsTestRule(@Nullable Class<T> fragmentClass) {
        super(ChromeBaseAppCompatActivity.class);
        mFragmentClass = fragmentClass;
    }

    /**
     * Create the settings activity test rule with a specific fragment class.
     *
     * @param fragmentClass Fragment that will be attached after the activity starts.
     * @param defaultFragmentArgs A bundle of default fragment arguments to be used.
     */
    public SettingsTestRule(Class<T> fragmentClass, Bundle defaultFragmentArgs) {
        super(ChromeBaseAppCompatActivity.class);
        mFragmentClass = fragmentClass;
        mDefaultFragmentArgs = defaultFragmentArgs;
    }

    @Override
    public ChromeBaseAppCompatActivity launchActivity(Intent startIntent) {
        Context context = ContextUtils.getApplicationContext();
        Class<? extends ChromeBaseAppCompatActivity> targetClass =
                SettingsInTab.isEnabled()
                        ? SettingsInTabTestActivity.class
                        : SettingsActivity.class;
        startIntent.setClass(context, targetClass);
        startIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        EnumSet<Stage> targetStages =
                (startIntent.getFlags() & Intent.FLAG_ACTIVITY_SINGLE_TOP) != 0
                        ? EnumSet.of(Stage.PAUSED, Stage.CREATED)
                        : EnumSet.of(Stage.CREATED);
        ChromeBaseAppCompatActivity activity =
                ApplicationTestUtils.waitForActivityWithClass(
                        targetClass, targetStages, () -> context.startActivity(startIntent));
        setActivity(activity);
        return activity;
    }

    /**
     * Launches the settings activity with the specified fragment.
     *
     * @return The {@link SettingsActivityInterface} for the activity that just started.
     */
    public SettingsActivityInterface startSettingsActivity() {
        return startSettingsActivity(mDefaultFragmentArgs);
    }

    /**
     * Launches the settings activity with the specified fragment and arguments.
     *
     * @param fragmentArgs A bundle of additional fragment arguments.
     * @return The {@link SettingsActivityInterface} for the activity that just started.
     */
    public SettingsActivityInterface startSettingsActivity(Bundle fragmentArgs) {
        Context context = ApplicationProvider.getApplicationContext();
        SettingsNavigation settingsNavigation =
                SettingsNavigationFactory.createSettingsNavigation();
        Intent intent =
                settingsNavigation.createSettingsIntent(context, mFragmentClass, fragmentArgs);

        launchActivity(intent);
        ApplicationTestUtils.waitForActivityState(getActivity(), Stage.RESUMED);
        return (SettingsActivityInterface) getActivity();
    }

    /**
     * @return The fragment attached to the settings activity.
     */
    @SuppressWarnings("unchecked") // Fragment type T is supplied by the test's parameterization.
    public T getFragment() {
        Assert.assertNotNull("#getFragment is called before activity launch.", getActivity());

        Fragment fragment = null;
        if (getActivity() instanceof SettingsActivityInterface host) {
            fragment = host.getMainFragment();
        }
        Assert.assertNotNull(fragment);
        return (T) fragment;
    }
}
