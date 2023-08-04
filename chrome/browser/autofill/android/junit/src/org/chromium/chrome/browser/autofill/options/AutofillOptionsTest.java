// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.options;

import static org.junit.Assert.assertEquals;

import androidx.annotation.StringRes;
import androidx.fragment.app.testing.FragmentScenario;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.test.util.browser.Features;

/** Unit tests for autofill options settings screen. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AutofillOptionsTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private AutofillOptionsFragment mFragment;
    private FragmentScenario mScenario;

    @Before
    public void setUp() {
        mScenario = FragmentScenario.launchInContainer(AutofillOptionsFragment.class);
        mScenario.onFragment(fragment -> {
            mFragment = (AutofillOptionsFragment) fragment; // Valid until scenario is recreated.
        });
    }

    @After
    public void tearDown() throws Exception {
        if (mScenario != null) {
            mScenario.close();
        }
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
