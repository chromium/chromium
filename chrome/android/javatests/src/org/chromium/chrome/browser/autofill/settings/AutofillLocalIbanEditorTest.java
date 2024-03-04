// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static com.google.common.truth.Truth.assertThat;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillLocalIbanEditorTest {
    @Rule public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule public final AutofillTestRule rule = new AutofillTestRule();

    @Rule
    public final SettingsActivityTestRule<AutofillLocalIbanEditor> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AutofillLocalIbanEditor.class);

    private AutofillTestHelper mAutofillTestHelper;

    @Before
    public void setUp() {
        mAutofillTestHelper = new AutofillTestHelper();
    }

    private AutofillLocalIbanEditor setUpDefaultAutofillLocalIbanEditorFragment() {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        AutofillLocalIbanEditor autofillLocalIbanEditorFragment =
                (AutofillLocalIbanEditor) activity.getMainFragment();
        assertThat(autofillLocalIbanEditorFragment.mDoneButton.isEnabled()).isFalse();
        return autofillLocalIbanEditorFragment;
    }

    @Test
    @MediumTest
    public void testValidIbanValueEnablesSaveButton() throws Exception {
        AutofillLocalIbanEditor autofillLocalIbanEditorFragment =
                setUpDefaultAutofillLocalIbanEditorFragment();

        // Valid Russia IBAN value.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        autofillLocalIbanEditorFragment.mValue.setText(
                                "RU0204452560040702810412345678901");
                    } catch (Exception e) {
                        Assert.fail("Failed to set IBAN");
                    }
                });

        assertThat(autofillLocalIbanEditorFragment.mDoneButton.isEnabled()).isTrue();
    }

    @Test
    @MediumTest
    public void testInvalidIbanValueDoesNotEnableSaveButton() throws Exception {
        AutofillLocalIbanEditor autofillLocalIbanEditorFragment =
                setUpDefaultAutofillLocalIbanEditorFragment();

        // Invalid Russia IBAN value.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        autofillLocalIbanEditorFragment.mValue.setText(
                                "RU0204452560040702810412345678902");
                    } catch (Exception e) {
                        Assert.fail("Failed to set IBAN");
                    }
                });

        assertThat(autofillLocalIbanEditorFragment.mDoneButton.isEnabled()).isFalse();
    }
}
