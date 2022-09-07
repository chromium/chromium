// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.model.FrameworkMethod;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.lang.annotation.Annotation;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.reflect.AnnotatedElement;
import java.lang.reflect.Method;
import java.util.List;

/** Unit tests for SkipCheck. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SkipCheckTest {
    private static class TestableSkipCheck extends SkipCheck {
        public static <T extends Annotation> List<T> getAnnotationsForTesting(
                AnnotatedElement element, Class<T> annotationClass) {
            return AnnotationProcessingUtils.getAnnotations(element, annotationClass);
        }

        @Override
        public boolean shouldSkip(FrameworkMethod m) {
            return false;
        }
    }

    @Retention(RetentionPolicy.RUNTIME)
    private @interface TestAnnotation {}

    @TestAnnotation
    private class AnnotatedBaseClass {
        public void unannotatedMethod() {}
        @TestAnnotation public void annotatedMethod() {}
    }

    private class ExtendsAnnotatedBaseClass extends AnnotatedBaseClass {
        public void anotherUnannotatedMethod() {}
    }

    private class UnannotatedBaseClass {
        public void unannotatedMethod() {}
        @TestAnnotation public void annotatedMethod() {}
    }

    @Test
    public void getAnnotationsForClassNone() {
        List<TestAnnotation> annotations = TestableSkipCheck.getAnnotationsForTesting(
                UnannotatedBaseClass.class, TestAnnotation.class);
        Assert.assertEquals(0, annotations.size());
    }

    @Test
    public void getAnnotationsForClassOnClass() {
        List<TestAnnotation> annotations = TestableSkipCheck.getAnnotationsForTesting(
                AnnotatedBaseClass.class, TestAnnotation.class);
        Assert.assertEquals(1, annotations.size());
    }

    @Test
    public void getAnnotationsForClassOnSuperclass() {
        List<TestAnnotation> annotations = TestableSkipCheck.getAnnotationsForTesting(
                ExtendsAnnotatedBaseClass.class, TestAnnotation.class);
        Assert.assertEquals(1, annotations.size());
    }

    @Test
    public void getAnnotationsForMethodNone() throws NoSuchMethodException {
        Method testMethod = UnannotatedBaseClass.class.getMethod("unannotatedMethod",
                (Class[]) null);
        List<TestAnnotation> annotations = TestableSkipCheck.getAnnotationsForTesting(
                testMethod, TestAnnotation.class);
        Assert.assertEquals(0, annotations.size());
    }

    @Test
    public void getAnnotationsForMethodOnMethod() throws NoSuchMethodException {
        Method testMethod = UnannotatedBaseClass.class.getMethod("annotatedMethod",
                (Class[]) null);
        List<TestAnnotation> annotations = TestableSkipCheck.getAnnotationsForTesting(
                testMethod, TestAnnotation.class);
        Assert.assertEquals(1, annotations.size());
    }

    @Test
    public void getAnnotationsForMethodOnClass() throws NoSuchMethodException {
        Method testMethod = AnnotatedBaseClass.class.getMethod("unannotatedMethod",
                (Class[]) null);
        List<TestAnnotation> annotations = TestableSkipCheck.getAnnotationsForTesting(
                testMethod, TestAnnotation.class);
        Assert.assertEquals(1, annotations.size());
    }

    @Test
    public void getAnnotationsForMethodOnSuperclass() throws NoSuchMethodException {
        Method testMethod = ExtendsAnnotatedBaseClass.class.getMethod("unannotatedMethod",
                (Class[]) null);
        List<TestAnnotation> annotations = TestableSkipCheck.getAnnotationsForTesting(
                testMethod, TestAnnotation.class);
        Assert.assertEquals(1, annotations.size());
    }

    @Test
    public void getAnnotationsOverlapping() throws NoSuchMethodException {
        Method testMethod = AnnotatedBaseClass.class.getMethod("annotatedMethod",
                (Class[]) null);
        List<TestAnnotation> annotations = TestableSkipCheck.getAnnotationsForTesting(
                testMethod, TestAnnotation.class);
        Assert.assertEquals(2, annotations.size());
    }
}
