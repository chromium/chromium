// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.sameInstance;
import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for the methods of {@link PasswordSettingsAccessorFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PasswordSettingsAccessorFactoryTest {
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
        PasswordSettingsAccessorFactory passwordSettingsAccessorFactory =
                mock(PasswordSettingsAccessorFactory.class);
        PasswordSettingsAccessorFactory.setupFactoryForTesting(passwordSettingsAccessorFactory);
        assertEquals(
                PasswordSettingsAccessorFactory.getOrCreate(), passwordSettingsAccessorFactory);
    }
}
