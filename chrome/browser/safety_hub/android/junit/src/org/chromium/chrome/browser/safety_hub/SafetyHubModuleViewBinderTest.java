// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.junit.Assert.assertEquals;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;

import androidx.annotation.DrawableRes;
import androidx.preference.Preference;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class SafetyHubModuleViewBinderTest {
    private static final @DrawableRes int DEFAULT_ICON = R.drawable.ic_globe_24dp;
    private static final @DrawableRes int ERROR_ICON = R.drawable.ic_error;
    private Activity mActivity;
    private PropertyModel mPasswordCheckPropertyModel;
    private Preference mPasswordCheckPreference;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).get();
        mPasswordCheckPreference = new Preference(mActivity);

        mPasswordCheckPropertyModel =
                new PropertyModel.Builder(
                                SafetyHubModuleProperties.PASSWORD_CHECK_SAFETY_HUB_MODULE_KEYS)
                        .with(SafetyHubModuleProperties.ICON, DEFAULT_ICON)
                        .build();
        PropertyModelChangeProcessor.create(
                mPasswordCheckPropertyModel,
                mPasswordCheckPreference,
                SafetyHubModuleViewBinder::bindPasswordCheckProperties);
    }

    @Test
    public void testPasswordCheckModule_NoCompromisedPasswords() {
        mPasswordCheckPropertyModel.set(SafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT, 0);

        String expectedTitle = mActivity.getString(R.string.safety_check_passwords_safe);
        assertEquals(expectedTitle, mPasswordCheckPreference.getTitle().toString());
        assertEquals(
                DEFAULT_ICON, shadowOf(mPasswordCheckPreference.getIcon()).getCreatedFromResId());
    }

    @Test
    public void testPasswordCheckModule_CompromisedPasswordsExist() {
        int compromisedPasswordsCount = 5;
        mPasswordCheckPropertyModel.set(
                SafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT, compromisedPasswordsCount);

        String expectedTitle =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_check_passwords_compromised_exist,
                                compromisedPasswordsCount,
                                compromisedPasswordsCount);
        assertEquals(expectedTitle, mPasswordCheckPreference.getTitle().toString());
        assertEquals(
                ERROR_ICON, shadowOf(mPasswordCheckPreference.getIcon()).getCreatedFromResId());
    }
}
