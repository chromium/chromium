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
