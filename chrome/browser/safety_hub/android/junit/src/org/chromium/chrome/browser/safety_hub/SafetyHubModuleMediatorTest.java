// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;

import androidx.annotation.DrawableRes;

import org.junit.Before;
import org.junit.Test;
import org.junit.experimental.runners.Enclosed;
import org.junit.runner.RunWith;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleOption;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleMediator.ModuleState;
import org.chromium.ui.base.TestActivity;

import java.util.Arrays;
import java.util.Collection;

/** Robolectric tests for {@link SafetyHubModuleMediator}. */
@RunWith(Enclosed.class)
public class SafetyHubModuleMediatorTest {
    static class SafetyHubModuleMediatorImpl implements SafetyHubModuleMediator {
        @ModuleState int mModuleState;
        boolean mIsManaged;
        @ModuleOption int mModuleOption;

        boolean mExpanded;

        SafetyHubModuleMediatorImpl(
                @ModuleState int moduleState, boolean isManaged, @ModuleOption int moduleOption) {
            mModuleState = moduleState;
            mIsManaged = isManaged;
            mModuleOption = moduleOption;
        }

        @Override
        public @ModuleState int getModuleState() {
            return mModuleState;
        }

        @Override
        public @ModuleOption int getOption() {
            return mModuleOption;
        }

        @Override
        public boolean isManaged() {
            return mIsManaged;
        }

        @Override
        public void setUpModule() {}

        @Override
        public void updateModule() {}

        @Override
        public void destroy() {}

        @Override
        public void setExpandState(boolean expanded) {
            mExpanded = expanded;
        }

        public boolean isExpanded() {
            return mExpanded;
        }
    }

    @RunWith(ParameterizedRobolectricTestRunner.class)
    @Batch(Batch.UNIT_TESTS)
    public static class SafetyHubModuleMediatorParamTests {
        @Parameters
        public static Collection<Object> data() {
            return Arrays.asList(
                    new Object[] {
                        ModuleOption.UPDATE_CHECK,
                        ModuleOption.ACCOUNT_PASSWORDS,
                        ModuleOption.SAFE_BROWSING,
                        ModuleOption.UNUSED_PERMISSIONS,
                        ModuleOption.NOTIFICATION_REVIEW
                    });
        }

        @Parameter public @ModuleOption int mModuleOption;

        public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

        private static final @DrawableRes int SAFE_ICON = R.drawable.material_ic_check_24dp;
        private static final @DrawableRes int INFO_ICON = R.drawable.btn_info;
        private static final @DrawableRes int MANAGED_ICON = R.drawable.ic_business;
        private static final @DrawableRes int WARNING_ICON = R.drawable.ic_error;

        private Activity mActivity;

        @Before
        public void setUp() {
            mActivity = Robolectric.buildActivity(TestActivity.class).get();
        }

        @Test
        public void hasSafeState_icon() {
            SafetyHubModuleMediatorImpl mediator =
                    new SafetyHubModuleMediatorImpl(
                            ModuleState.SAFE, /* isManaged= */ false, mModuleOption);

            assertEquals(SAFE_ICON, shadowOf(mediator.getIcon(mActivity)).getCreatedFromResId());
        }

        @Test
        public void hasStateState_expandStatus() {
            SafetyHubModuleMediatorImpl mediator =
                    new SafetyHubModuleMediatorImpl(
                            ModuleState.SAFE, /* isManaged= */ false, mModuleOption);

            mediator.setModuleExpandState(/* noOtherNonManagedWarningState= */ false);
            assertEquals(false, mediator.isExpanded());

            mediator.setModuleExpandState(/* noOtherNonManagedWarningState= */ true);
            assertEquals(false, mediator.isExpanded());
        }

        @Test
        public void hasInfoState_icon() {
            SafetyHubModuleMediatorImpl mediator =
                    new SafetyHubModuleMediatorImpl(
                            ModuleState.INFO, /* isManaged= */ false, mModuleOption);

            assertEquals(INFO_ICON, shadowOf(mediator.getIcon(mActivity)).getCreatedFromResId());
        }

        @Test
        public void hasInfoState_expandedStatus() {
            SafetyHubModuleMediatorImpl mediator =
                    new SafetyHubModuleMediatorImpl(
                            ModuleState.INFO, /* isManaged= */ false, mModuleOption);

            mediator.setModuleExpandState(/* noOtherNonManagedWarningState= */ false);
            assertEquals(true, mediator.isExpanded());

            mediator.setModuleExpandState(/* noOtherNonManagedWarningState= */ true);
            assertEquals(false, mediator.isExpanded());
        }

        @Test
        public void hasInfoState_icon_managed() {
            SafetyHubModuleMediatorImpl mediator =
                    new SafetyHubModuleMediatorImpl(
                            ModuleState.INFO, /* isManaged= */ true, mModuleOption);

            assertEquals(MANAGED_ICON, shadowOf(mediator.getIcon(mActivity)).getCreatedFromResId());
        }

        @Test
        public void hasInfoState_expandedStatus_managed() {
            SafetyHubModuleMediatorImpl mediator =
                    new SafetyHubModuleMediatorImpl(
                            ModuleState.INFO, /* isManaged= */ true, mModuleOption);

            mediator.setModuleExpandState(/* noOtherNonManagedWarningState= */ false);
            assertEquals(true, mediator.isExpanded());

            mediator.setModuleExpandState(/* noOtherNonManagedWarningState= */ true);
            assertEquals(false, mediator.isExpanded());
        }

        @Test
        public void hasWarningState_icon() {
            SafetyHubModuleMediatorImpl mediator =
                    new SafetyHubModuleMediatorImpl(
                            ModuleState.WARNING, /* isManaged= */ false, mModuleOption);

            assertEquals(WARNING_ICON, shadowOf(mediator.getIcon(mActivity)).getCreatedFromResId());
        }

        @Test
        public void hasWarningState_expandedStatus() {
            SafetyHubModuleMediatorImpl mediator =
                    new SafetyHubModuleMediatorImpl(
                            ModuleState.WARNING, /* isManaged= */ false, mModuleOption);

            mediator.setModuleExpandState(/* noOtherNonManagedWarningState= */ false);
            assertEquals(true, mediator.isExpanded());

            mediator.setModuleExpandState(/* noOtherNonManagedWarningState= */ true);
            assertEquals(true, mediator.isExpanded());
        }

        @Test
        public void hasWarningState_icon_managed() {
            SafetyHubModuleMediatorImpl mediator =
                    new SafetyHubModuleMediatorImpl(
                            ModuleState.WARNING, /* isManaged= */ true, mModuleOption);

            assertEquals(MANAGED_ICON, shadowOf(mediator.getIcon(mActivity)).getCreatedFromResId());
        }

        @Test
        public void hasWarningState_expandedStatus_managed() {
            SafetyHubModuleMediatorImpl mediator =
                    new SafetyHubModuleMediatorImpl(
                            ModuleState.WARNING, /* isManaged= */ true, mModuleOption);

            mediator.setModuleExpandState(/* noOtherNonManagedWarningState= */ false);
            assertEquals(true, mediator.isExpanded());

            mediator.setModuleExpandState(/* noOtherNonManagedWarningState= */ true);
            assertEquals(false, mediator.isExpanded());
        }
    }

    @RunWith(BaseRobolectricTestRunner.class)
    @Batch(Batch.UNIT_TESTS)
    public static class SafetyHubModuleMediatorSingleTests {
        @Test
        public void order_mixedStates() {
            SafetyHubModuleMediatorImpl warningMediator =
                    new SafetyHubModuleMediatorImpl(
                            ModuleState.WARNING, /* isManaged= */ false, ModuleOption.UPDATE_CHECK);
            SafetyHubModuleMediatorImpl warningManagedMediator =
                    new SafetyHubModuleMediatorImpl(
                            ModuleState.WARNING, /* isManaged= */ true, ModuleOption.UPDATE_CHECK);

            SafetyHubModuleMediatorImpl infoMediator =
                    new SafetyHubModuleMediatorImpl(
                            ModuleState.INFO, /* isManaged= */ false, ModuleOption.UPDATE_CHECK);
            SafetyHubModuleMediatorImpl unavailableMediator =
                    new SafetyHubModuleMediatorImpl(
                            ModuleState.UNAVAILABLE,
                            /* isManaged= */ false,
                            ModuleOption.UPDATE_CHECK);
            SafetyHubModuleMediatorImpl infoManagedMediator =
                    new SafetyHubModuleMediatorImpl(
                            ModuleState.INFO, /* isManaged= */ true, ModuleOption.UPDATE_CHECK);

            SafetyHubModuleMediatorImpl safeMediator =
                    new SafetyHubModuleMediatorImpl(
                            ModuleState.SAFE, /* isManaged= */ false, ModuleOption.UPDATE_CHECK);

            int[] actualOrder =
                    new int[] {
                        warningMediator.getOrder(),
                        unavailableMediator.getOrder(),
                        warningManagedMediator.getOrder(),
                        infoMediator.getOrder(),
                        infoManagedMediator.getOrder(),
                        safeMediator.getOrder(),
                    };
            Arrays.sort(actualOrder);

            // Verify the actual order of modules reflects the expected order.
            assertArrayEquals(
                    actualOrder,
                    new int[] {
                        warningMediator.getOrder(),
                        unavailableMediator.getOrder(),
                        warningManagedMediator.getOrder(),
                        infoMediator.getOrder(),
                        infoManagedMediator.getOrder(),
                        safeMediator.getOrder(),
                    });
        }

        @Test
        public void order_mixedModuleOptions() {
            SafetyHubModuleMediatorImpl updateCheckMediator =
                    new SafetyHubModuleMediatorImpl(
                            ModuleState.SAFE, /* isManaged= */ false, ModuleOption.UPDATE_CHECK);
            SafetyHubModuleMediatorImpl accountPasswordsMediator =
                    new SafetyHubModuleMediatorImpl(
                            ModuleState.SAFE,
                            /* isManaged= */ false,
                            ModuleOption.ACCOUNT_PASSWORDS);
            SafetyHubModuleMediatorImpl safeBrowsingMediator =
                    new SafetyHubModuleMediatorImpl(
                            ModuleState.SAFE, /* isManaged= */ false, ModuleOption.SAFE_BROWSING);
            SafetyHubModuleMediatorImpl unusedPermissionsMediator =
                    new SafetyHubModuleMediatorImpl(
                            ModuleState.SAFE,
                            /* isManaged= */ false,
                            ModuleOption.UNUSED_PERMISSIONS);
            SafetyHubModuleMediatorImpl notificationReviewMediator =
                    new SafetyHubModuleMediatorImpl(
                            ModuleState.SAFE,
                            /* isManaged= */ false,
                            ModuleOption.NOTIFICATION_REVIEW);

            int[] actualOrder =
                    new int[] {
                        updateCheckMediator.getOrder(),
                        accountPasswordsMediator.getOrder(),
                        safeBrowsingMediator.getOrder(),
                        unusedPermissionsMediator.getOrder(),
                        notificationReviewMediator.getOrder()
                    };
            Arrays.sort(actualOrder);

            // Verify the actual order of modules reflects the expected order.
            assertArrayEquals(
                    actualOrder,
                    new int[] {
                        updateCheckMediator.getOrder(),
                        accountPasswordsMediator.getOrder(),
                        safeBrowsingMediator.getOrder(),
                        unusedPermissionsMediator.getOrder(),
                        notificationReviewMediator.getOrder()
                    });
        }
    }
}
