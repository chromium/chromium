// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertThrows;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link SettingsMetricsReporter}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SettingsMetricsReporterTest {
    private static final String MAIN_TAG = "main_fragment_tag";
    private static final String OTHER_TAG = "other_fragment_tag";

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarios =
            new ActivityScenarioRule<>(TestActivity.class);

    private TestActivity mActivity;
    private FragmentManager mFragmentManager;

    public static class TestSettingsFragment extends Fragment implements SettingsFragment {
        @Override
        public int getAnimationType() {
            return AnimationType.PROPERTY;
        }
    }

    public static class TestRegularFragment extends Fragment {}

    @Before
    public void setUp() {
        mActivityScenarios
                .getScenario()
                .onActivity(activity -> mActivity = (TestActivity) activity);

        SettingsMetricsReporter reporter = new SettingsMetricsReporter(MAIN_TAG);
        mFragmentManager = mActivity.getSupportFragmentManager();
        mFragmentManager.registerFragmentLifecycleCallbacks(reporter, /* recursive= */ true);
    }

    @Test
    public void testAttached_settingsFragment_logsFragmentAttached() {
        Fragment fragment = new TestSettingsFragment();

        int expectedHash = TestSettingsFragment.class.getSimpleName().hashCode();
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Settings.FragmentAttached", expectedHash)
                        .expectNoRecords("Settings.NonSettingsFragmentAttached")
                        .build();

        mFragmentManager.beginTransaction().add(fragment, OTHER_TAG).commitNow();

        histogramWatcher.assertExpected();
    }

    @Test
    public void testAttached_regularFragment_doesNotLog() {
        Fragment fragment = new TestRegularFragment();

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Settings.FragmentAttached")
                        .expectNoRecords("Settings.NonSettingsFragmentAttached")
                        .build();

        mFragmentManager.beginTransaction().add(fragment, OTHER_TAG).commitNow();

        histogramWatcher.assertExpected();
    }

    @Test
    public void testAttached_regularFragmentMatchesMainTag_logsBothAndAsserts() {
        Fragment fragment = new TestRegularFragment();

        int expectedHash = TestRegularFragment.class.getSimpleName().hashCode();
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Settings.FragmentAttached", expectedHash)
                        .expectIntRecord("Settings.NonSettingsFragmentAttached", expectedHash)
                        .build();

        FragmentTransaction transaction =
                mFragmentManager.beginTransaction().add(fragment, MAIN_TAG);
        assertThrows(AssertionError.class, () -> transaction.commitNow());

        histogramWatcher.assertExpected();
    }
}
