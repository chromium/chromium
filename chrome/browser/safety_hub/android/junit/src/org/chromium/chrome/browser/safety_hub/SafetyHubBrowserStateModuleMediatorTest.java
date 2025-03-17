// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleOption;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleState;
import org.chromium.components.browser_ui.settings.CardPreference;
import org.chromium.ui.base.TestActivity;

import java.util.Arrays;
import java.util.List;

/** Robolectric tests for {@link SafetyHubBrowserStateModuleMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class SafetyHubBrowserStateModuleMediatorTest {
    static class SafetyHubModuleMediatorImpl implements SafetyHubModuleMediator {
        @ModuleState int mModuleState;

        SafetyHubModuleMediatorImpl(@ModuleState int moduleState) {
            mModuleState = moduleState;
        }

        @Override
        public @ModuleState int getModuleState() {
            return mModuleState;
        }

        @Override
        public @ModuleOption int getOption() {
            return 0;
        }

        @Override
        public boolean isManaged() {
            return false;
        }

        @Override
        public void setUpModule() {}

        @Override
        public void updateModule() {}

        @Override
        public void destroy() {}

        @Override
        public void setExpandState(boolean expanded) {}
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Activity mActivity;
    private CardPreference mPreference;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).get();
        mPreference = new CardPreference(mActivity, null);
    }

    @Test
    public void oneUnsafeState() {
        List<SafetyHubModuleMediator> moduleMediators =
                Arrays.asList(
                        new SafetyHubModuleMediatorImpl(ModuleState.WARNING),
                        new SafetyHubModuleMediatorImpl(ModuleState.INFO),
                        new SafetyHubModuleMediatorImpl(ModuleState.SAFE));

        SafetyHubBrowserStateModuleMediator moduleMediator =
                new SafetyHubBrowserStateModuleMediator(mPreference, moduleMediators);
        moduleMediator.setUpModule();
        moduleMediator.updateModule();

        assertFalse(mPreference.isVisible());
    }

    @Test
    public void multipleUnsafeState() {
        List<SafetyHubModuleMediator> moduleMediators =
                Arrays.asList(
                        new SafetyHubModuleMediatorImpl(ModuleState.WARNING),
                        new SafetyHubModuleMediatorImpl(ModuleState.UNAVAILABLE),
                        new SafetyHubModuleMediatorImpl(ModuleState.SAFE));

        SafetyHubBrowserStateModuleMediator moduleMediator =
                new SafetyHubBrowserStateModuleMediator(mPreference, moduleMediators);
        moduleMediator.setUpModule();
        moduleMediator.updateModule();

        assertFalse(mPreference.isVisible());
    }

    @Test
    public void safeState() {
        List<SafetyHubModuleMediator> moduleMediators =
                Arrays.asList(
                        new SafetyHubModuleMediatorImpl(ModuleState.SAFE),
                        new SafetyHubModuleMediatorImpl(ModuleState.SAFE),
                        new SafetyHubModuleMediatorImpl(ModuleState.SAFE));

        SafetyHubBrowserStateModuleMediator moduleMediator =
                new SafetyHubBrowserStateModuleMediator(mPreference, moduleMediators);
        moduleMediator.setUpModule();
        moduleMediator.updateModule();

        assertTrue(mPreference.isVisible());
    }
}
