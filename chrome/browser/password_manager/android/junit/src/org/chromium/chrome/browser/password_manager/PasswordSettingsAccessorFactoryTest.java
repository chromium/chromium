// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.sameInstance;
import static org.junit.Assert.assertEquals;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for the methods of {@link PasswordSettingsAccessorFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PasswordSettingsAccessorFactoryTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private PasswordSettingsAccessorFactory mPasswordSettingsAccessorFactory;

    @Test
    public void testGetOrCreateReusesExistingFactory() {
        PasswordSettingsAccessorFactory firstFactoryInstance =
                PasswordSettingsAccessorFactory.getOrCreate();
        PasswordSettingsAccessorFactory secondFactoryInstance =
                PasswordSettingsAccessorFactory.getOrCreate();
        assertThat(firstFactoryInstance, sameInstance(secondFactoryInstance));
    }

    @Test
    public void testSetupFactoryForTestingUsesTheTestingFactory() {
        PasswordSettingsAccessorFactory.setupFactoryForTesting(mPasswordSettingsAccessorFactory);
        assertEquals(
                PasswordSettingsAccessorFactory.getOrCreate(), mPasswordSettingsAccessorFactory);
    }
}
