// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.params;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Annotations for Parameterized Tests */
public class ParameterAnnotations {
    /**
     * Annotation for test methods to indicate associated {@link ParameterProvider}.
     * Note: the class referred to must be public and have a public default constructor.
     */
    @Retention(RetentionPolicy.RUNTIME)
    @Target(ElementType.METHOD)
    public @interface UseMethodParameter {
        Class<? extends ParameterProvider> value();
    }

    /**
     * Annotation for methods that should be called before running a test with method parameters.
     *
     * In order to use this, add a {@link MethodParamAnnotationRule} annotated with
     * {@code @}{@link org.junit.Rule Rule} to your test class.
     * @see ParameterProvider
     * @see UseMethodParameterAfter
     */
    @Retention(RetentionPolicy.RUNTIME)
    @Target(ElementType.METHOD)
    public @interface UseMethodParameterBefore {
        Class<? extends ParameterProvider> value();
    }

    /**
     * Annotation for methods that should be called after running a test with method parameters.
     *
     * In order to use this, add a {@link MethodParamAnnotationRule} annotated with
     * {@code @}{@link org.junit.Rule Rule} to your test class.
     * @see ParameterProvider
     * @see UseMethodParameterBefore
     */
    @Retention(RetentionPolicy.RUNTIME)
    @Target(ElementType.METHOD)
    public @interface UseMethodParameterAfter {
        Class<? extends ParameterProvider> value();
    }

    /**
     * Annotation for a {@link ParameterProvider} whose parameter sets are expensive to switch
     * between in a {@link org.chromium.base.test.util.Batch}, e.g. night mode, which requires
     * relaunching the Activity that batched tests reuse.
     *
     * <p>Tests that use the provider through {@link UseMethodParameter} are run grouped by
     * parameter set: first the tests that do not use the provider, then the tests with the first
     * parameter set, then the tests with the second parameter set, and so on, each group in method
     * order. This way the switch happens once per parameter set instead of once per test. Without
     * this annotation, each test method runs with all of its parameter sets before the next test
     * method.
     */
    @Retention(RetentionPolicy.RUNTIME)
    @Target(ElementType.TYPE)
    public @interface GroupTestsByParameterSet {}

    /** Annotation for static field of a `List<ParameterSet>` for entire test class */
    @Retention(RetentionPolicy.RUNTIME)
    @Target(ElementType.FIELD)
    public @interface ClassParameter {}

    /** Annotation for static field of a `List<ParameterSet>` of TestRule */
    @Retention(RetentionPolicy.RUNTIME)
    @Target(ElementType.FIELD)
    public @interface RuleParameter {}

    /**
     * Annotation for test class, it specifies which ParameterizeRunnerDelegate to use.
     *
     * The default ParameterizedRunnerDelegate is BaseJUnit4RunnerDelegate.class
     */
    @Retention(RetentionPolicy.RUNTIME)
    @Target(ElementType.TYPE)
    public @interface UseRunnerDelegate {
        Class<? extends ParameterizedRunnerDelegate> value();
    }
}
