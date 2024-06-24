// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.os.Build;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.model.FrameworkMethod;
import org.robolectric.annotation.Config;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for the DisableIf annotation and its SkipCheck implementation. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = 29)
public class DisableIfTest {
    private static void expectShouldSkip(boolean shouldSkip, Class<?> testClass) {
        try {
            Assert.assertEquals(
                    shouldSkip,
                    new DisableIfSkipCheck()
                            .shouldSkip(new FrameworkMethod(testClass.getMethod("target"))));
        } catch (NoSuchMethodException e) {
            throw new RuntimeException(e);
        }
    }

    @Test
    public void testSdkIsLessThanAndIsLessThan() {
        class SdkIsLessThan {
            @DisableIf.Build(sdk_is_less_than = 30)
            public void target() {}
        }
        expectShouldSkip(true, SdkIsLessThan.class);
    }

    @Test
    public void testSdkIsLessThanButIsEqual() {
        class SdkIsEqual {
            @DisableIf.Build(sdk_is_less_than = 29)
            public void target() {}
        }
        expectShouldSkip(false, SdkIsEqual.class);
    }

    @Test
    public void testSdkIsLessThanButIsGreaterThan() {
        class SdkIsGreaterThan {
            @DisableIf.Build(sdk_is_less_than = 28)
            public void target() {}
        }
        expectShouldSkip(false, SdkIsGreaterThan.class);
    }

    @Test
    public void testSdkIsGreaterThanButIsLessThan() {
        class SdkIsLessThan {
            @DisableIf.Build(sdk_is_greater_than = 30)
            public void target() {}
        }
        expectShouldSkip(false, SdkIsLessThan.class);
    }

    @Test
    public void testSdkIsGreaterThanButIsEqual() {
        class SdkIsEqual {
            @DisableIf.Build(sdk_is_greater_than = 29)
            public void target() {}
        }
        expectShouldSkip(false, SdkIsEqual.class);
    }

    @Test
    public void testSdkIsGreaterThanAndIsGreaterThan() {
        class SdkIsGreaterThan {
            @DisableIf.Build(sdk_is_greater_than = 28)
            public void target() {}
        }
        expectShouldSkip(true, SdkIsGreaterThan.class);
    }

    @Test
    public void testSdkIsEqualAndIsEqual() {
        class SdkIsEqual {
            @DisableIf.Build(sdk_equals = 29)
            public void target() {}
        }
        expectShouldSkip(true, SdkIsEqual.class);
    }

    @Test
    public void testSdkIsEqualButIsLessThan() {
        class SdkIsLessThan {
            @DisableIf.Build(sdk_equals = 30)
            public void target() {}
        }
        expectShouldSkip(false, SdkIsLessThan.class);
    }

    @Test
    public void testSdkIsEqualButIsGreaterThan() {
        class SdkIsGreaterThan {
            @DisableIf.Build(sdk_equals = 28)
            public void target() {}
        }
        expectShouldSkip(false, SdkIsGreaterThan.class);
    }

    @Test
    public void testSupportedAbiIncludesAndCpuAbiMatches() {
        class SupportedAbisCpuAbiMatch {
            @DisableIf.Build(supported_abis_includes = "foo")
            public void target() {}
        }
        String[] originalAbis = Build.SUPPORTED_ABIS;
        try {
            ReflectionHelpers.setStaticField(
                    Build.class, "SUPPORTED_ABIS", new String[] {"foo", "bar"});
            expectShouldSkip(true, SupportedAbisCpuAbiMatch.class);
        } finally {
            ReflectionHelpers.setStaticField(Build.class, "SUPPORTED_ABIS", originalAbis);
        }
    }

    @Test
    public void testSupportedAbiIncludesAndCpuAbi2Matches() {
        class SupportedAbisCpuAbi2Match {
            @DisableIf.Build(supported_abis_includes = "bar")
            public void target() {}
        }
        String[] originalAbis = Build.SUPPORTED_ABIS;
        try {
            ReflectionHelpers.setStaticField(
                    Build.class, "SUPPORTED_ABIS", new String[] {"foo", "bar"});
            expectShouldSkip(true, SupportedAbisCpuAbi2Match.class);
        } finally {
            ReflectionHelpers.setStaticField(Build.class, "SUPPORTED_ABIS", originalAbis);
        }
    }

    @Test
    public void testSupportedAbiIncludesButNoMatch() {
        class SupportedAbisNoMatch {
            @DisableIf.Build(supported_abis_includes = "baz")
            public void target() {}
        }
        String[] originalAbis = Build.SUPPORTED_ABIS;
        try {
            ReflectionHelpers.setStaticField(
                    Build.class, "SUPPORTED_ABIS", new String[] {"foo", "bar"});
            expectShouldSkip(false, SupportedAbisNoMatch.class);
        } finally {
            ReflectionHelpers.setStaticField(Build.class, "SUPPORTED_ABIS", originalAbis);
        }
    }

    @Test
    public void testHardwareIsMatches() {
        class HardwareIsMatches {
            @DisableIf.Build(hardware_is = "hammerhead")
            public void target() {}
        }
        String originalHardware = Build.HARDWARE;
        try {
            ReflectionHelpers.setStaticField(Build.class, "HARDWARE", "hammerhead");
            expectShouldSkip(true, HardwareIsMatches.class);
        } finally {
            ReflectionHelpers.setStaticField(Build.class, "HARDWARE", originalHardware);
        }
    }

    @Test
    public void testHardwareIsDoesntMatch() {
        class HardwareIsDoesntMatch {
            @DisableIf.Build(hardware_is = "hammerhead")
            public void target() {}
        }
        String originalHardware = Build.HARDWARE;
        try {
            ReflectionHelpers.setStaticField(Build.class, "HARDWARE", "mako");
            expectShouldSkip(false, HardwareIsDoesntMatch.class);
        } finally {
            ReflectionHelpers.setStaticField(Build.class, "HARDWARE", originalHardware);
        }
    }

    @DisableIf.Build(supported_abis_includes = "foo")
    private static class DisableIfSuperclassTestCase extends SkipCheckTest {
        public void target() {}
    }

    @DisableIf.Build(hardware_is = "hammerhead")
    private static class DisableIfTestCase extends DisableIfSuperclassTestCase {
        @Override
        public void target() {}
    }

    @Test
    public void testDisableClass() {
        String originalHardware = Build.HARDWARE;
        try {
            ReflectionHelpers.setStaticField(Build.class, "HARDWARE", "hammerhead");
            expectShouldSkip(true, DisableIfTestCase.class);
        } finally {
            ReflectionHelpers.setStaticField(Build.class, "HARDWARE", originalHardware);
        }
    }

    @Test
    public void testDisableSuperClass() {
        String[] originalAbis = Build.SUPPORTED_ABIS;
        try {
            ReflectionHelpers.setStaticField(Build.class, "SUPPORTED_ABIS", new String[] {"foo"});
            expectShouldSkip(true, DisableIfTestCase.class);
        } finally {
            ReflectionHelpers.setStaticField(Build.class, "SUPPORTED_ABIS", originalAbis);
        }
    }

    @Test
    public void testTwoConditionsBothMet() {
        class TwoConditionsBothMet {
            @DisableIf.Build(sdk_is_greater_than = 28, supported_abis_includes = "foo")
            public void target() {}
        }
        String[] originalAbis = Build.SUPPORTED_ABIS;
        try {
            ReflectionHelpers.setStaticField(
                    Build.class, "SUPPORTED_ABIS", new String[] {"foo", "bar"});
            expectShouldSkip(true, TwoConditionsBothMet.class);
        } finally {
            ReflectionHelpers.setStaticField(Build.class, "SUPPORTED_ABIS", originalAbis);
        }
    }

    @Test
    public void testTwoConditionsFirstMet() {
        class TwoConditionsFirstMet {
            @DisableIf.Build(sdk_is_greater_than = 28, supported_abis_includes = "baz")
            public void target() {}
        }
        String[] originalAbis = Build.SUPPORTED_ABIS;
        try {
            ReflectionHelpers.setStaticField(
                    Build.class, "SUPPORTED_ABIS", new String[] {"foo", "bar"});
            expectShouldSkip(false, TwoConditionsFirstMet.class);
        } finally {
            ReflectionHelpers.setStaticField(Build.class, "SUPPORTED_ABIS", originalAbis);
        }
    }

    @Test
    public void testTwoConditionsSecondMet() {
        class TwoConditionsSecondMet {
            @DisableIf.Build(sdk_is_greater_than = 30, supported_abis_includes = "foo")
            public void target() {}
        }
        String[] originalAbis = Build.SUPPORTED_ABIS;
        try {
            ReflectionHelpers.setStaticField(
                    Build.class, "SUPPORTED_ABIS", new String[] {"foo", "bar"});
            expectShouldSkip(false, TwoConditionsSecondMet.class);
        } finally {
            ReflectionHelpers.setStaticField(Build.class, "SUPPORTED_ABIS", originalAbis);
        }
    }

    @Test
    public void testTwoConditionsNeitherMet() {
        class TwoConditionsNeitherMet {
            @DisableIf.Build(sdk_is_greater_than = 30, supported_abis_includes = "baz")
            public void target() {}
        }
        String[] originalAbis = Build.SUPPORTED_ABIS;
        try {
            ReflectionHelpers.setStaticField(
                    Build.class, "SUPPORTED_ABIS", new String[] {"foo", "bar"});
            expectShouldSkip(false, TwoConditionsNeitherMet.class);
        } finally {
            ReflectionHelpers.setStaticField(Build.class, "SUPPORTED_ABIS", originalAbis);
        }
    }

    @Test
    public void testTwoAnnotationsBothMet() {
        class TwoAnnotationsBothMet {
            @DisableIf.Build(supported_abis_includes = "foo")
            @DisableIf.Build(sdk_is_greater_than = 28)
            public void target() {}
        }
        String[] originalAbis = Build.SUPPORTED_ABIS;
        try {
            ReflectionHelpers.setStaticField(
                    Build.class, "SUPPORTED_ABIS", new String[] {"foo", "bar"});
            expectShouldSkip(true, TwoAnnotationsBothMet.class);
        } finally {
            ReflectionHelpers.setStaticField(Build.class, "SUPPORTED_ABIS", originalAbis);
        }
    }

    @Test
    public void testTwoAnnotationsFirstMet() {
        class TwoAnnotationsFirstMet {
            @DisableIf.Build(supported_abis_includes = "foo")
            @DisableIf.Build(sdk_is_greater_than = 30)
            public void target() {}
        }
        String[] originalAbis = Build.SUPPORTED_ABIS;
        try {
            ReflectionHelpers.setStaticField(
                    Build.class, "SUPPORTED_ABIS", new String[] {"foo", "bar"});
            expectShouldSkip(true, TwoAnnotationsFirstMet.class);
        } finally {
            ReflectionHelpers.setStaticField(Build.class, "SUPPORTED_ABIS", originalAbis);
        }
    }

    @Test
    public void testTwoAnnotationsSecondMet() {
        class TwoAnnotationsSecondMet {
            @DisableIf.Build(supported_abis_includes = "baz")
            @DisableIf.Build(sdk_is_greater_than = 28)
            public void target() {}
        }
        String[] originalAbis = Build.SUPPORTED_ABIS;
        try {
            ReflectionHelpers.setStaticField(
                    Build.class, "SUPPORTED_ABIS", new String[] {"foo", "bar"});
            expectShouldSkip(true, TwoAnnotationsSecondMet.class);
        } finally {
            ReflectionHelpers.setStaticField(Build.class, "SUPPORTED_ABIS", originalAbis);
        }
    }

    @Test
    public void testTwoAnnotationsNeitherMet() {
        class TestTwoAnnotationsNeitherMet {
            @DisableIf.Build(supported_abis_includes = "baz")
            @DisableIf.Build(sdk_is_greater_than = 30)
            public void target() {}
        }
        String[] originalAbis = Build.SUPPORTED_ABIS;
        try {
            ReflectionHelpers.setStaticField(
                    Build.class, "SUPPORTED_ABIS", new String[] {"foo", "bar"});
            expectShouldSkip(false, TestTwoAnnotationsNeitherMet.class);
        } finally {
            ReflectionHelpers.setStaticField(Build.class, "SUPPORTED_ABIS", originalAbis);
        }
    }
}
