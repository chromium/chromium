// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.options;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.ON_THIRD_PARTY_TOGGLE_CHANGED;
import static org.chromium.chrome.browser.autofill.options.AutofillOptionsProperties.THIRD_PARTY_AUTOFILL_ENABLED;

import androidx.annotation.StringRes;
import androidx.fragment.app.testing.FragmentScenario;
import androidx.lifecycle.Lifecycle.Event;
import androidx.lifecycle.LifecycleRegistry;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for autofill options settings screen. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID})
public class AutofillOptionsTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private UserPrefsJni mMockUserPrefsJni;
    @Mock
    private PrefService mPrefs;
    @Mock
    private Profile mProfile;

    private AutofillOptionsFragment mFragment;
    private AutoCloseable mCloseableMocks;
    private FragmentScenario mScenario;

    @Before
    public void setUp() {
        mCloseableMocks = MockitoAnnotations.openMocks(this);

        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mMockUserPrefsJni);
        doReturn(mPrefs).when(mMockUserPrefsJni).get(mProfile);

        mScenario = FragmentScenario.launchInContainer(AutofillOptionsFragment.class);
        mScenario.onFragment(fragment -> {
            mFragment = (AutofillOptionsFragment) fragment; // Valid until scenario is recreated.
            mFragment.setProfile(mProfile);
        });
    }

    @After
    public void tearDown() throws Exception {
        if (mScenario != null) {
            mScenario.close();
        }
        mCloseableMocks.close();
    }

    @Test
    @SmallTest
    public void constructedWithPrefAsDefaultForOption() {
        doReturn(true).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE);

        PropertyModel model = new AutofillOptionsCoordinator(mFragment).initializeNow();

        assertTrue(model.get(THIRD_PARTY_AUTOFILL_ENABLED));
    }

    @Test
    @SmallTest
    public void toggledOptionRecordedInHistogram() {
        HistogramWatcher histogramWatcher = HistogramWatcher.newSingleRecordWatcher(
                AutofillOptionsMediator.HISTOGRAM_USE_THIRD_PARTY_FILLING, true);
        AutofillOptionsCoordinator autofillOptions = new AutofillOptionsCoordinator(mFragment);
        PropertyModel model = autofillOptions.initializeNow();

        // Enabling the option should be recorded once.
        model.get(ON_THIRD_PARTY_TOGGLE_CHANGED).onResult(true);
        histogramWatcher.assertExpected();

        // Enabling the option again should be ignored.
        model.get(ON_THIRD_PARTY_TOGGLE_CHANGED).onResult(true);
        histogramWatcher.assertExpected();

        // Disabling the option should be recorded again.
        histogramWatcher = HistogramWatcher.newSingleRecordWatcher(
                AutofillOptionsMediator.HISTOGRAM_USE_THIRD_PARTY_FILLING, false);
        model.get(ON_THIRD_PARTY_TOGGLE_CHANGED).onResult(false);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void setPrefTogglesOptionOnResume() {
        doReturn(false).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE);
        AutofillOptionsCoordinator autofillOptions = new AutofillOptionsCoordinator(mFragment);
        PropertyModel model = autofillOptions.initializeNow();
        LifecycleRegistry lifecycleRegistry = new LifecycleRegistry(mFragment);
        autofillOptions.observeLifecycle(lifecycleRegistry);
        assertFalse(model.get(THIRD_PARTY_AUTOFILL_ENABLED));

        doReturn(true).when(mPrefs).getBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE);
        lifecycleRegistry.handleLifecycleEvent(Event.ON_RESUME);

        assertTrue(model.get(THIRD_PARTY_AUTOFILL_ENABLED));
    }

    @Test
    @SmallTest
    public void setsTitleAndPref() {
        AutofillOptionsCoordinator.createFor(mFragment); // Initial binding updates the pref.

        assertEquals(
                mFragment.getActivity().getTitle(), getString(R.string.autofill_options_title));
        // TODO(crbug/1469795): Implement and assert that the toggle is present.
    }

    private String getString(@StringRes int stringId) {
        return mFragment.getResources().getString(stringId);
    }
}
