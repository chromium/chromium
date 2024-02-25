// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import static org.hamcrest.Matchers.contains;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.fail;
import static org.junit.runner.Description.createTestDescription;

import org.junit.Ignore;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.InitializationError;
import org.junit.runners.model.Statement;

import org.chromium.base.test.util.AnnotationProcessingUtils.AnnotationExtractor;

import java.lang.annotation.Annotation;
import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.Arrays;
import java.util.Comparator;
import java.util.List;

/** Test for {@link AnnotationProcessingUtils}. */
@RunWith(BlockJUnit4ClassRunner.class)
public class AnnotationProcessingUtilsTest {
    @Test
    public void testGetTargetAnnotation_NotOnClassNorMethod() {
        TargetAnnotation retrievedAnnotation;

        retrievedAnnotation =
                AnnotationProcessingUtils.getAnnotation(
                        createTestDescription(
                                ClassWithoutTargetAnnotation.class, "methodWithoutAnnotation"),
                        TargetAnnotation.class);
        assertNull(retrievedAnnotation);
    }

    @Test
    public void testGetTargetAnnotation_NotOnClassButOnMethod() {
        TargetAnnotation retrievedAnnotation;

        retrievedAnnotation =
                AnnotationProcessingUtils.getAnnotation(
                        getTest(ClassWithoutTargetAnnotation.class, "methodWithTargetAnnotation"),
                        TargetAnnotation.class);
        assertNotNull(retrievedAnnotation);
    }

    @Test
    public void testGetTargetAnnotation_NotOnClassDifferentOneOnMethod() {
        TargetAnnotation retrievedAnnotation;

        retrievedAnnotation =
                AnnotationProcessingUtils.getAnnotation(
                        getTest(
                                ClassWithoutTargetAnnotation.class,
                                "methodWithAnnotatedAnnotation"),
                        TargetAnnotation.class);
        assertNull(retrievedAnnotation);
    }

    @Test
    public void testGetTargetAnnotation_OnClassButNotOnMethod() {
        TargetAnnotation retrievedAnnotation;

        retrievedAnnotation =
                AnnotationProcessingUtils.getAnnotation(
                        getTest(ClassWithAnnotation.class, "methodWithoutAnnotation"),
                        TargetAnnotation.class);
        assertNotNull(retrievedAnnotation);
        assertEquals(Location.Class, retrievedAnnotation.value());
    }

    @Test
    public void testGetTargetAnnotation_OnClassAndMethod() {
        TargetAnnotation retrievedAnnotation;

        retrievedAnnotation =
                AnnotationProcessingUtils.getAnnotation(
                        getTest(ClassWithAnnotation.class, "methodWithTargetAnnotation"),
                        TargetAnnotation.class);
        assertNotNull(retrievedAnnotation);
        assertEquals(Location.Method, retrievedAnnotation.value());
    }

    @Test
    @Ignore("Rules not supported yet.")
    public void testGetTargetAnnotation_OnRuleButNotOnMethod() {
        TargetAnnotation retrievedAnnotation;

        retrievedAnnotation =
                AnnotationProcessingUtils.getAnnotation(
                        getTest(ClassWithRule.class, "methodWithoutAnnotation"),
                        TargetAnnotation.class);
        assertNotNull(retrievedAnnotation);
        assertEquals(Location.Rule, retrievedAnnotation.value());
    }

    @Test
    @Ignore("Rules not supported yet.")
    public void testGetTargetAnnotation_OnRuleAndMethod() {
        TargetAnnotation retrievedAnnotation;

        retrievedAnnotation =
                AnnotationProcessingUtils.getAnnotation(
                        getTest(ClassWithRule.class, "methodWithTargetAnnotation"),
                        TargetAnnotation.class);
        assertNotNull(retrievedAnnotation);
        assertEquals(Location.Method, retrievedAnnotation.value());
    }

    @Test
    public void testGetMetaAnnotation_Indirectly() {
        MetaAnnotation retrievedAnnotation;

        retrievedAnnotation =
                AnnotationProcessingUtils.getAnnotation(
                        getTest(
                                ClassWithoutTargetAnnotation.class,
                                "methodWithAnnotatedAnnotation"),
                        MetaAnnotation.class);
        assertNotNull(retrievedAnnotation);
    }

    @Test
    public void testGetAllTargetAnnotations() {
        List<TargetAnnotation> retrievedAnnotations;

        retrievedAnnotations =
                AnnotationProcessingUtils.getAnnotations(
                        getTest(ClassWithAnnotation.class, "methodWithTargetAnnotation"),
                        TargetAnnotation.class);
        assertEquals(2, retrievedAnnotations.size());
        assertEquals(Location.Class, retrievedAnnotations.get(0).value());
        assertEquals(Location.Method, retrievedAnnotations.get(1).value());
    }

    @Test
    public void testGetAllTargetAnnotations_OnParentClass() {
        List<TargetAnnotation> retrievedAnnotations;

        retrievedAnnotations =
                AnnotationProcessingUtils.getAnnotations(
                        getTest(DerivedClassWithoutAnnotation.class, "newMethodWithoutAnnotation"),
                        TargetAnnotation.class);
        assertEquals(1, retrievedAnnotations.size());
        assertEquals(Location.Class, retrievedAnnotations.get(0).value());
    }

    @Test
    public void testGetAllTargetAnnotations_OnDerivedMethodAndParentClass() {
        List<TargetAnnotation> retrievedAnnotations;

        retrievedAnnotations =
                AnnotationProcessingUtils.getAnnotations(
                        getTest(
                                DerivedClassWithoutAnnotation.class,
                                "newMethodWithTargetAnnotation"),
                        TargetAnnotation.class);
        assertEquals(2, retrievedAnnotations.size());
        assertEquals(Location.Class, retrievedAnnotations.get(0).value());
        assertEquals(Location.DerivedMethod, retrievedAnnotations.get(1).value());
    }

    @Test
    public void testGetAllTargetAnnotations_OnDerivedMethodAndParentClassAndMethod() {
        List<TargetAnnotation> retrievedAnnotations;

        retrievedAnnotations =
                AnnotationProcessingUtils.getAnnotations(
                        getTest(DerivedClassWithoutAnnotation.class, "methodWithTargetAnnotation"),
                        TargetAnnotation.class);
        // We should not look at the base implementation of the method. Mostly it should not happen
        // in the context of tests.
        assertEquals(2, retrievedAnnotations.size());
        assertEquals(Location.Class, retrievedAnnotations.get(0).value());
        assertEquals(Location.DerivedMethod, retrievedAnnotations.get(1).value());
    }

    @Test
    public void testGetAllTargetAnnotations_OnDerivedParentAndParentClass() {
        List<TargetAnnotation> retrievedAnnotations;

        retrievedAnnotations =
                AnnotationProcessingUtils.getAnnotations(
                        getTest(DerivedClassWithAnnotation.class, "methodWithoutAnnotation"),
                        TargetAnnotation.class);
        assertEquals(2, retrievedAnnotations.size());
        assertEquals(Location.Class, retrievedAnnotations.get(0).value());
        assertEquals(Location.DerivedClass, retrievedAnnotations.get(1).value());
    }

    @Test
    public void testGetAllAnnotations() {
        List<Annotation> annotations;

        AnnotationExtractor annotationExtractor =
                new AnnotationExtractor(
                        TargetAnnotation.class, MetaAnnotation.class, AnnotatedAnnotation.class);
        annotations =
                annotationExtractor.getMatchingAnnotations(
                        getTest(DerivedClassWithAnnotation.class, "methodWithTwoAnnotations"));
        assertEquals(5, annotations.size());

        // Retrieved annotation order:
        // On Parent Class
        assertEquals(TargetAnnotation.class, annotations.get(0).annotationType());
        assertEquals(Location.Class, ((TargetAnnotation) annotations.get(0)).value());

        // On Class
        assertEquals(TargetAnnotation.class, annotations.get(1).annotationType());
        assertEquals(Location.DerivedClass, ((TargetAnnotation) annotations.get(1)).value());

        // Meta-annotations from method
        assertEquals(MetaAnnotation.class, annotations.get(2).annotationType());

        // On Method
        assertEquals(AnnotatedAnnotation.class, annotations.get(3).annotationType());
        assertEquals(TargetAnnotation.class, annotations.get(4).annotationType());
        assertEquals(Location.DerivedMethod, ((TargetAnnotation) annotations.get(4)).value());
    }

    @SuppressWarnings("unchecked")
    @Test
    public void testAnnotationExtractorSortOrder_UnknownAnnotations() {
        AnnotationExtractor annotationExtractor = new AnnotationExtractor(Target.class);
        Comparator<Class<? extends Annotation>> comparator =
                annotationExtractor.getTypeComparator();
        List<Class<? extends Annotation>> testList =
                Arrays.asList(Rule.class, Test.class, Override.class, Target.class, Rule.class);
        testList.sort(comparator);
        assertThat(
                "Unknown annotations should not be reordered and come before the known ones.",
                testList,
                contains(Rule.class, Test.class, Override.class, Rule.class, Target.class));
    }

    @SuppressWarnings("unchecked")
    @Test
    public void testAnnotationExtractorSortOrder_KnownAnnotations() {
        AnnotationExtractor annotationExtractor =
                new AnnotationExtractor(Test.class, Target.class, Rule.class);
        Comparator<Class<? extends Annotation>> comparator =
                annotationExtractor.getTypeComparator();
        List<Class<? extends Annotation>> testList =
                Arrays.asList(Rule.class, Test.class, Override.class, Target.class, Rule.class);
        testList.sort(comparator);
        assertThat(
                "Known annotations should be sorted in the same order as provided to the extractor",
                testList,
                contains(Override.class, Test.class, Target.class, Rule.class, Rule.class));
    }

    private static Description getTest(Class<?> klass, String testName) {
        Description description = null;
        try {
            description = new DummyTestRunner(klass).describe(testName);
        } catch (InitializationError initializationError) {
            initializationError.printStackTrace();
            fail("DummyTestRunner initialization failed:" + initializationError.getMessage());
        }
        if (description == null) {
            fail("Not test named '" + testName + "' in class" + klass.getSimpleName());
        }
        return description;
    }

    // region Test Data: Annotations and dummy test classes
    private enum Location {
        Unspecified,
        Class,
        Method,
        Rule,
        DerivedClass,
        DerivedMethod
    }

    @Retention(RetentionPolicy.RUNTIME)
    @Target({ElementType.TYPE, ElementType.METHOD})
    private @interface TargetAnnotation {
        Location value() default Location.Unspecified;
    }

    @Retention(RetentionPolicy.RUNTIME)
    @Target({ElementType.ANNOTATION_TYPE, ElementType.TYPE, ElementType.METHOD})
    private @interface MetaAnnotation {}

    @Retention(RetentionPolicy.RUNTIME)
    @Target({ElementType.TYPE, ElementType.METHOD})
    @MetaAnnotation
    private @interface AnnotatedAnnotation {}

    private @interface SimpleAnnotation {}

    @SimpleAnnotation
    private static class ClassWithoutTargetAnnotation {
        @Test
        public void methodWithoutAnnotation() {}

        @Test
        @TargetAnnotation
        public void methodWithTargetAnnotation() {}

        @Test
        @AnnotatedAnnotation
        public void methodWithAnnotatedAnnotation() {}
    }

    @TargetAnnotation(Location.Class)
    private static class ClassWithAnnotation {
        @Test
        public void methodWithoutAnnotation() {}

        @Test
        @TargetAnnotation(Location.Method)
        public void methodWithTargetAnnotation() {}

        @Test
        @MetaAnnotation
        public void methodWithMetaAnnotation() {}

        @Test
        @AnnotatedAnnotation
        public void methodWithAnnotatedAnnotation() {}
    }

    private static class DerivedClassWithoutAnnotation extends ClassWithAnnotation {
        @Test
        public void newMethodWithoutAnnotation() {}

        @Test
        @TargetAnnotation(Location.DerivedMethod)
        public void newMethodWithTargetAnnotation() {}

        @Test
        @Override
        @TargetAnnotation(Location.DerivedMethod)
        public void methodWithTargetAnnotation() {}
    }

    @TargetAnnotation(Location.DerivedClass)
    private static class DerivedClassWithAnnotation extends ClassWithAnnotation {
        @Test
        public void newMethodWithoutAnnotation() {}

        @Test
        @AnnotatedAnnotation
        @TargetAnnotation(Location.DerivedMethod)
        public void methodWithTwoAnnotations() {}
    }

    private static class ClassWithRule {
        @Rule Rule1 mRule = new Rule1();

        @Test
        public void methodWithoutAnnotation() {}

        @Test
        @TargetAnnotation
        public void methodWithTargetAnnotation() {}
    }

    @TargetAnnotation(Location.Rule)
    @MetaAnnotation
    private static class Rule1 implements TestRule {
        @Override
        public Statement apply(Statement statement, Description description) {
            return null;
        }
    }

    private static class DummyTestRunner extends BlockJUnit4ClassRunner {
        public DummyTestRunner(Class<?> klass) throws InitializationError {
            super(klass);
        }

        @Override
        protected void collectInitializationErrors(List<Throwable> errors) {
            // Do nothing. BlockJUnit4ClassRunner requires the class to be public, but we don't
            // want/need it.
        }

        public Description describe(String testName) {
            List<FrameworkMethod> tests = getTestClass().getAnnotatedMethods(Test.class);
            for (FrameworkMethod testMethod : tests) {
                if (testMethod.getName().equals(testName)) return describeChild(testMethod);
            }
            return null;
        }
    }

    // endregion
}
