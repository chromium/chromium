// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.junit.Assert.assertArrayEquals;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.setup_list.SetupListModuleUtils;

import java.util.Collection;

/** Test relating to {@link HomeTipsModulesProvider} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HomeTipsModulesProviderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Test
    @SmallTest
    public void testGetModulesToRegister_returnsSetupListWhenActive() {
        Collection<Integer> expectedModules = SetupListModuleUtils.getRankedModuleTypes();
        Collection<Integer> actualModules =
                HomeTipsModulesProvider.getModuleTypesToRegister(/* isSetupListActive= */ true);

        assertArrayEquals(expectedModules.toArray(), actualModules.toArray());
    }

    @Test
    @SmallTest
    public void testGetModulesToRegister_returnsEducationalTipsWhenInactive() {
        Collection<Integer> expectedModules = EducationalTipModuleUtils.getModuleTypes();
        Collection<Integer> actualModules =
                HomeTipsModulesProvider.getModuleTypesToRegister(/* isSetupListActive= */ false);

        assertArrayEquals(expectedModules.toArray(), actualModules.toArray());
    }
}
