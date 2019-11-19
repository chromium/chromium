// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_DEVICE_DAYDREAM;

import android.os.Build;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExternalResource;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.vr.rules.VrModuleNotInstalled;
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction;
import org.chromium.chrome.browser.vr.util.VrTestRuleUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.components.module_installer.engine.InstallEngine;

import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.Callable;

/**
 * End-to-end tests for installing the VR DFM on Daydream-ready phones on startup.
 *
 * TODO(agrieve): This test may be better as a robolectric test.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-features=LogJsConsoleMessages"})
@MinAndroidSdkLevel(Build.VERSION_CODES.N) // Daydream is only supported on N+.
public class VrDaydreamReadyModuleInstallTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            VrTestRuleUtils.generateDefaultTestRuleParameters();
    @Rule
    public RuleChain mRuleChain;

    private final Set<String> mModulesRequestedDeferred = new HashSet<>();

    public VrDaydreamReadyModuleInstallTest(Callable<ChromeActivityTestRule> callable)
            throws Exception {
        mRuleChain =
                RuleChain.outerRule(new VrModuleInstallerRule())
                        .around(VrTestRuleUtils.wrapRuleInActivityRestrictionRule(callable.call()));
    }

    /** Tests that the install is requested deferred. */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @Restriction({RESTRICTION_TYPE_DEVICE_DAYDREAM})
    @VrModuleNotInstalled
    public void testDeferredRequestOnStartup() {
        Assert.assertTrue("VR module should have been deferred installed at startup",
                mModulesRequestedDeferred.contains("vr"));
    }

    private class VrModuleInstallerRule extends ExternalResource {
        private final InstallEngine mOldModuleInstaller;
        private final InstallEngine mStubModuleInstaller;

        public VrModuleInstallerRule() {
            mStubModuleInstaller = new InstallEngine() {
                @Override
                public void installDeferred(String moduleName) {
                    mModulesRequestedDeferred.add(moduleName);
                }
            };
            mOldModuleInstaller = VrModule.getInstallEngine();
        }

        @Override
        protected void before() {
            VrModule.setInstallEngine(mStubModuleInstaller);
        }

        @Override
        protected void after() {
            VrModule.setInstallEngine(mOldModuleInstaller);
        }
    }
}
