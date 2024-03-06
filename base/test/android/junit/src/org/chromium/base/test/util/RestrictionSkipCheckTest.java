// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.text.TextUtils;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.model.FrameworkMethod;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for RestrictionSkipCheck. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class RestrictionSkipCheckTest {
    private static final String TEST_RESTRICTION_APPLIES =
            "org.chromium.base.test.util.RestrictionSkipCheckTest.TEST_RESTRICTION_APPLIES";
    private static final String TEST_RESTRICTION_DOES_NOT_APPLY =
            "org.chromium.base.test.util.RestrictionSkipCheckTest.TEST_RESTRICTION_DOES_NOT_APPLY";

    private static class TestRestrictionSkipCheck extends RestrictionSkipCheck {
        public TestRestrictionSkipCheck() {
            super(null);
        }

        @Override
        protected boolean restrictionApplies(String restriction) {
            return TextUtils.equals(restriction, TEST_RESTRICTION_APPLIES);
        }
    }

    private static class UnannotatedBaseClass {
        @Restriction({TEST_RESTRICTION_APPLIES})
        public void restrictedMethod() {}

        @Restriction({TEST_RESTRICTION_DOES_NOT_APPLY})
        public void unrestrictedMethod() {}
    }

    @Restriction({TEST_RESTRICTION_APPLIES})
    private static class RestrictedClass {
        public void unannotatedMethod() {}
    }

    @Restriction({TEST_RESTRICTION_DOES_NOT_APPLY})
    private static class UnrestrictedClass extends UnannotatedBaseClass {
        public void unannotatedMethod() {}
    }

    @Restriction({TEST_RESTRICTION_APPLIES, TEST_RESTRICTION_DOES_NOT_APPLY})
    private static class MultipleRestrictionsRestrictedClass extends UnannotatedBaseClass {
        public void unannotatedMethod() {}
    }

    private static class ExtendsRestrictedClass extends RestrictedClass {
        @Override
        public void unannotatedMethod() {}
    }

    private static class ExtendsUnrestrictedClass extends UnrestrictedClass {
        @Override
        public void unannotatedMethod() {}
    }

    private static void expectShouldSkip(boolean shouldSkip, Class<?> testClass, String methodName)
            throws Exception {
        Assert.assertEquals(
                shouldSkip,
                new TestRestrictionSkipCheck()
                        .shouldSkip(new FrameworkMethod(testClass.getMethod(methodName))));
    }

    @Test
    public void testMethodRestricted() throws Exception {
        expectShouldSkip(true, UnannotatedBaseClass.class, "restrictedMethod");
    }

    @Test
    public void testMethodUnrestricted() throws Exception {
        expectShouldSkip(false, UnannotatedBaseClass.class, "unrestrictedMethod");
    }

    @Test
    public void testClassRestricted() throws Exception {
        expectShouldSkip(true, RestrictedClass.class, "unannotatedMethod");
    }

    @Test
    public void testClassUnrestricted() throws Exception {
        expectShouldSkip(false, UnrestrictedClass.class, "unannotatedMethod");
    }

    @Test
    public void testMultipleRestrictionsClassRestricted() throws Exception {
        expectShouldSkip(true, MultipleRestrictionsRestrictedClass.class, "unannotatedMethod");
    }

    @Test
    public void testSuperclassRestricted() throws Exception {
        expectShouldSkip(true, ExtendsRestrictedClass.class, "unannotatedMethod");
    }

    @Test
    public void testSuperclassUnrestricted() throws Exception {
        expectShouldSkip(false, ExtendsUnrestrictedClass.class, "unannotatedMethod");
    }
}
