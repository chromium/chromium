// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import androidx.annotation.CallSuper;
import androidx.annotation.Nullable;

import org.junit.rules.ExternalResource;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import java.lang.annotation.Annotation;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.ListIterator;

/**
 * Test rule that collects specific annotations to help with test set up and tear down. It is set up
 * with a list of annotations to look for and exposes the ones picked up on the test through
 * {@link #getAnnotations()} and related methods.
 *
 * Note: The rule always apply, whether it picked up annotations or not.
 *
 * Usage:
 *
 * <pre>
 * public class Test {
 *    &#64;Rule
 *    public AnnotationRule rule = new AnnotationRule(Foo.class) {
 *          &#64;Override
 *          protected void before() { ... }
 *
 *          &#64;Override
 *          protected void after() { ... }
 *    };
 *
 *    &#64;Test
 *    &#64;Foo
 *    public void myTest() { ... }
 * }
 * </pre>
 *
 * It can also be used to trigger for multiple annotations:
 *
 * <pre>
 * &#64;DisableFoo
 * public class Test {
 *    &#64;Rule
 *    public AnnotationRule rule = new AnnotationRule(EnableFoo.class, DisableFoo.class) {
 *          &#64;Override
 *          protected void before() {
 *            // Loops through all the picked up annotations. For myTest(), it would process
 *            // DisableFoo first, then EnableFoo.
 *            for (Annotation annotation : getAnnotations()) {
 *                if (annotation instanceof EnableFoo) { ... }
 *                else if (annotation instanceof DisableFoo) { ... }
 *            }
 *          }
 *
 *          &#64;Override
 *          protected void after() {
 *            // For myTest(), would return EnableFoo as it's directly set on the method.
 *            Annotation a = getClosestAnnotation();
 *            ...
 *          }
 *    };
 *
 *    &#64;Test
 *    &#64;EnableFoo
 *    public void myTest() { ... }
 * }
 * </pre>
 *
 * @see AnnotationProcessingUtils.AnnotationExtractor
 */
public abstract class AnnotationRule extends ExternalResource {
    private final AnnotationProcessingUtils.AnnotationExtractor mAnnotationExtractor;
    private List<Annotation> mCollectedAnnotations;
    private Description mTestDescription;

    @SafeVarargs
    public AnnotationRule(Class<? extends Annotation> firstAnnotationType,
            Class<? extends Annotation>... additionalTypes) {
        List<Class<? extends Annotation>> mAnnotationTypes = new ArrayList<>();
        mAnnotationTypes.add(firstAnnotationType);
        mAnnotationTypes.addAll(Arrays.asList(additionalTypes));
        mAnnotationExtractor = new AnnotationProcessingUtils.AnnotationExtractor(mAnnotationTypes);
    }

    @CallSuper
    @Override
    public Statement apply(Statement base, Description description) {
        mTestDescription = description;

        mCollectedAnnotations = mAnnotationExtractor.getMatchingAnnotations(description);

        // Return the wrapped statement to execute before() and after().
        return super.apply(base, description);
    }

    /** @return {@link Description} of the current test. */
    protected Description getTestDescription() {
        return mTestDescription;
    }

    /**
     * @return The collected annotations that match the declared type(s).
     * @throws NullPointerException if this is called before annotations have been collected,
     * which happens when the rule is applied to the {@link Statement}.
     */
    protected List<Annotation> getAnnotations() {
        return Collections.unmodifiableList(mCollectedAnnotations);
    }

    /**
     * @return The closest annotation matching the provided type, or {@code null} if there is none.
     */
    @SuppressWarnings("unchecked")
    protected @Nullable <A extends Annotation> A getAnnotation(Class<A> annnotationType) {
        ListIterator<Annotation> iteratorFromEnd =
                mCollectedAnnotations.listIterator(mCollectedAnnotations.size());
        while (iteratorFromEnd.hasPrevious()) {
            Annotation annotation = iteratorFromEnd.previous();
            if (annnotationType.isAssignableFrom(annotation.annotationType())) {
                return (A) annotation;
            }
        }
        return null;
    }

    protected @Nullable Annotation getClosestAnnotation() {
        if (mCollectedAnnotations.isEmpty()) return null;
        return mCollectedAnnotations.get(mCollectedAnnotations.size() - 1);
    }
}
