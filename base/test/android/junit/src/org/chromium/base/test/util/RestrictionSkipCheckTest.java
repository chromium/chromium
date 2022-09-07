// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.text.TextUtils;

import junit.framework.TestCase;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
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

    private static class UnannotatedBaseClass extends TestCase {
        public UnannotatedBaseClass(String name) {
            super(name);
        }
        @Restriction({TEST_RESTRICTION_APPLIES}) public void restrictedMethod() {}
        @Restriction({TEST_RESTRICTION_DOES_NOT_APPLY}) public void unrestrictedMethod() {}
    }

    @Restriction({TEST_RESTRICTION_APPLIES})
    private static class RestrictedClass extends UnannotatedBaseClass {
        public RestrictedClass(String name) {
            super(name);
        }
        public void unannotatedMethod() {}
    }

    @Restriction({TEST_RESTRICTION_DOES_NOT_APPLY})
    private static class UnrestrictedClass extends UnannotatedBaseClass {
        public UnrestrictedClass(String name) {
            super(name);
        }
        public void unannotatedMethod() {}
    }

    @Restriction({
            TEST_RESTRICTION_APPLIES,
            TEST_RESTRICTION_DOES_NOT_APPLY})
    private static class MultipleRestrictionsRestrictedClass extends UnannotatedBaseClass {
        public MultipleRestrictionsRestrictedClass(String name) {
            super(name);
        }
        public void unannotatedMethod() {}
    }

    private static class ExtendsRestrictedClass extends RestrictedClass {
        public ExtendsRestrictedClass(String name) {
            super(name);
        }
        @Override
        public void unannotatedMethod() {}
    }

    private static class ExtendsUnrestrictedClass extends UnrestrictedClass {
        public ExtendsUnrestrictedClass(String name) {
            super(name);
        }
        @Override
        public void unannotatedMethod() {}
    }

    @Test
    public void testMethodRestricted() {
        Assert.assertTrue(new TestRestrictionSkipCheck().shouldSkip(
                new UnannotatedBaseClass("restrictedMethod")));
    }

    @Test
    public void testMethodUnrestricted() {
        Assert.assertFalse(new TestRestrictionSkipCheck().shouldSkip(
                new UnannotatedBaseClass("unrestrictedMethod")));
    }

    @Test
    public void testClassRestricted() {
        Assert.assertTrue(new TestRestrictionSkipCheck().shouldSkip(
                new RestrictedClass("unannotatedMethod")));
    }

    @Test
    public void testClassUnrestricted() {
        Assert.assertFalse(new TestRestrictionSkipCheck().shouldSkip(
                new UnrestrictedClass("unannotatedMethod")));
    }

    @Test
    public void testMultipleRestrictionsClassRestricted() {
        Assert.assertTrue(new TestRestrictionSkipCheck().shouldSkip(
                new MultipleRestrictionsRestrictedClass("unannotatedMethod")));
    }

    @Test
    public void testSuperclassRestricted() {
        Assert.assertTrue(new TestRestrictionSkipCheck().shouldSkip(
                new ExtendsRestrictedClass("unannotatedMethod")));
    }

    @Test
    public void testSuperclassUnrestricted() {
        Assert.assertFalse(new TestRestrictionSkipCheck().shouldSkip(
                new ExtendsUnrestrictedClass("unannotatedMethod")));
    }
}

