// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.privacy_sandbox.ActivityTypeMapper;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridgeJni;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.search_engines.SearchEngineChoiceService;
import org.chromium.ui.base.DeviceFormFactor;

/** Tests for {@link TabbedRootUiCoordinator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// TODO(crbug.com/40112282): Enable for tablets once we support them.
@Restriction({DeviceFormFactor.PHONE})
public class TabbedRootUiCoordinatorTest {
    @Rule public ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public JniMocker mocker = new JniMocker();
    @Rule public MockitoRule mockito = MockitoJUnit.rule();

    private TabbedRootUiCoordinator mTabbedRootUiCoordinator;

    @Mock private PrivacySandboxBridgeJni mPrivacySandboxBridgeJni;
    @Mock private SearchEngineChoiceService mSearchEngineChoiceService;

    @Before
    public void setUp() {
        mocker.mock(PrivacySandboxBridgeJni.TEST_HOOKS, mPrivacySandboxBridgeJni);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SearchEngineChoiceService.setInstanceForTests(mSearchEngineChoiceService);
                    doReturn(false).when(mSearchEngineChoiceService).isDeviceChoiceDialogEligible();
                });

        TabbedRootUiCoordinator.setDisableTopControlsAnimationsForTesting(true);
        mActivityTestRule.startMainActivityOnBlankPage();
        mTabbedRootUiCoordinator =
                (TabbedRootUiCoordinator)
                        mActivityTestRule.getActivity().getRootUiCoordinatorForTesting();
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_ACTIVITY_TYPE_STORAGE)
    public void testRecordPrivacySandboxActivityTypeIncrementsRecordWhenFlagIsEnabled() {
        verify(mPrivacySandboxBridgeJni)
                .recordActivityType(
                        any(),
                        eq(
                                ActivityTypeMapper.toPrivacySandboxStorageActivityType(
                                        ActivityType.TABBED)));
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.PRIVACY_SANDBOX_ACTIVITY_TYPE_STORAGE)
    public void testRecordPrivacySandboxActivityTypeDoesNotIncrementRecordWhenFlagIsDisabled() {
        verify(mPrivacySandboxBridgeJni, never()).recordActivityType(any(), anyInt());
    }
}
