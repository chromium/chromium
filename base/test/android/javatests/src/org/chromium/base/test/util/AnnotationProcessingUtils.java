// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.junit.runner.Description;

import java.lang.annotation.Annotation;
import java.lang.reflect.AnnotatedElement;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Queue;
import java.util.Set;

/**
 * Utility class to help with processing annotations, going around the code to collect them, etc.
 */
public abstract class AnnotationProcessingUtils {
    /**
     * Returns the closest instance of the requested annotation or null if there is none.
     * See {@link AnnotationExtractor} for context of "closest".
     */
    @SuppressWarnings("unchecked")
    public static <A extends Annotation> A getAnnotation(Description description, Class<A> clazz) {
        AnnotationExtractor extractor = new AnnotationExtractor(clazz);
        return (A) extractor.getClosest(extractor.getMatchingAnnotations(description));
    }

    /**
     * Returns the closest instance of the requested annotation or null if there is none.
     * See {@link AnnotationExtractor} for context of "closest".
     */
    @SuppressWarnings("unchecked")
    public static <A extends Annotation> A getAnnotation(AnnotatedElement element, Class<A> clazz) {
        AnnotationExtractor extractor = new AnnotationExtractor(clazz);
        return (A) extractor.getClosest(extractor.getMatchingAnnotations(element));
    }

    /** See {@link AnnotationExtractor} for details about the output sorting order. */
    @SuppressWarnings("unchecked")
    public static <A extends Annotation> List<A> getAnnotations(
            Description description, Class<A> annotationType) {
        return (List<A>) new AnnotationExtractor(annotationType)
                .getMatchingAnnotations(description);
    }

    /** See {@link AnnotationExtractor} for details about the output sorting order. */
    @SuppressWarnings("unchecked")
    public static <A extends Annotation> List<A> getAnnotations(
            AnnotatedElement annotatedElement, Class<A> annotationType) {
        return (List<A>) new AnnotationExtractor(annotationType)
                .getMatchingAnnotations(annotatedElement);
    }

    private static boolean isChromiumAnnotation(Annotation annotation) {
        Package pkg = annotation.annotationType().getPackage();
        return pkg != null && pkg.getName().startsWith("org.chromium");
    }

    /**
     * Processes various types of annotated elements ({@link Class}es, {@link Annotation}s,
     * {@link Description}s, etc.) and extracts the targeted annotations from it. The output will be
     * sorted in BFS-like order.
     *
     * For example, for a method we would get in reverse order:
     * - the method annotations
     * - the meta-annotations present on the method annotations,
     * - the class annotations
     * - the meta-annotations present on the class annotations,
     * - the annotations present on the super class,
     * - the meta-annotations present on the super class annotations,
     * - etc.
     *
     * When multiple annotations are targeted, if more than one is picked up at a given level (for
     * example directly on the method), they will be returned in the reverse order that they were
     * provided to the constructor.
     *
     * Note: We return the annotations in reverse order because we assume that if some processing
     * is going to be made on related annotations, the later annotations would likely override
     * modifications made by the former.
     *
     * Note: While resolving meta annotations, we don't expand the explorations to annotations types
     * that have already been visited. Please file a bug and assign to dgn@ if you think it caused
     * an issue.
     */
    public static class AnnotationExtractor {
        private final List<Class<? extends Annotation>> mAnnotationTypes;
        private final Comparator<Class<? extends Annotation>> mAnnotationTypeComparator;
        private final Comparator<Annotation> mAnnotationComparator;

        @SafeVarargs
        public AnnotationExtractor(Class<? extends Annotation>... additionalTypes) {
            this(Arrays.asList(additionalTypes));
        }

        public AnnotationExtractor(List<Class<? extends Annotation>> additionalTypes) {
            assert !additionalTypes.isEmpty();
            mAnnotationTypes = Collections.unmodifiableList(additionalTypes);
            mAnnotationTypeComparator =
                    (t1, t2) -> mAnnotationTypes.indexOf(t1) - mAnnotationTypes.indexOf(t2);
            mAnnotationComparator = (t1, t2)
                    -> mAnnotationTypeComparator.compare(t1.annotationType(), t2.annotationType());
        }

        public List<Annotation> getMatchingAnnotations(Description description) {
            return getMatchingAnnotations(new AnnotatedNode.DescriptionNode(description));
        }

        public List<Annotation> getMatchingAnnotations(AnnotatedElement annotatedElement) {
            AnnotatedNode annotatedNode;
            if (annotatedElement instanceof Method) {
                annotatedNode = new AnnotatedNode.MethodNode((Method) annotatedElement);
            } else if (annotatedElement instanceof Class) {
                annotatedNode = new AnnotatedNode.ClassNode((Class) annotatedElement);
            } else {
                throw new IllegalArgumentException("Unsupported type for " + annotatedElement);
            }

            return getMatchingAnnotations(annotatedNode);
        }

        /**
         * For a given list obtained from the extractor, returns the {@link Annotation} that would
         * be closest from the extraction point, or {@code null} if the list is empty.
         */
        @Nullable
        public Annotation getClosest(List<Annotation> annotationList) {
            return annotationList.isEmpty() ? null : annotationList.get(annotationList.size() - 1);
        }

        @VisibleForTesting
        Comparator<Class<? extends Annotation>> getTypeComparator() {
            return mAnnotationTypeComparator;
        }

        private List<Annotation> getMatchingAnnotations(AnnotatedNode annotatedNode) {
            List<Annotation> collectedAnnotations = new ArrayList<>();
            Queue<Annotation> workingSet = new LinkedList<>();
            Set<Class<? extends Annotation>> visited = new HashSet<>();

            AnnotatedNode currentAnnotationLayer = annotatedNode;
            while (currentAnnotationLayer != null) {
                queueAnnotations(currentAnnotationLayer.getAnnotations(), workingSet);

                while (!workingSet.isEmpty()) {
                    sweepAnnotations(collectedAnnotations, workingSet, visited);
                }

                currentAnnotationLayer = currentAnnotationLayer.getParent();
            }

            return collectedAnnotations;
        }

        private void queueAnnotations(List<Annotation> annotations, Queue<Annotation> workingSet) {
            Collections.sort(annotations, mAnnotationComparator);
            workingSet.addAll(annotations);
        }

        private void sweepAnnotations(List<Annotation> collectedAnnotations,
                Queue<Annotation> workingSet, Set<Class<? extends Annotation>> visited) {
            // 1. Grab node at the front of the working set.
            Annotation annotation = workingSet.remove();

            // 2. If it's an annotation of interest, put it aside for the output.
            if (mAnnotationTypes.contains(annotation.annotationType())) {
                collectedAnnotations.add(0, annotation);
            }

            // 3. Check if we can get skip some redundant iterations and avoid cycles.
            if (!visited.add(annotation.annotationType())) return;
            if (!isChromiumAnnotation(annotation)) return;

            // 4. Expand the working set
            queueAnnotations(Arrays.asList(annotation.annotationType().getDeclaredAnnotations()),
                    workingSet);
        }
    }

    /**
     * Abstraction to hide differences between Class, Method and Description with regards to their
     * annotations and what should be analyzed next.
     */
    private abstract static class AnnotatedNode {
        @Nullable
        abstract AnnotatedNode getParent();

        abstract List<Annotation> getAnnotations();

        static class DescriptionNode extends AnnotatedNode {
            final Description mDescription;

            DescriptionNode(Description description) {
                mDescription = description;
            }

            @Nullable
            @Override
            AnnotatedNode getParent() {
                return new ClassNode(mDescription.getTestClass());
            }

            @Override
            List<Annotation> getAnnotations() {
                return new ArrayList<>(mDescription.getAnnotations());
            }
        }

        static class ClassNode extends AnnotatedNode {
            final Class<?> mClass;

            ClassNode(Class<?> clazz) {
                mClass = clazz;
            }

            @Nullable
            @Override
            AnnotatedNode getParent() {
                Class<?> superClass = mClass.getSuperclass();
                return superClass == null ? null : new ClassNode(superClass);
            }

            @Override
            List<Annotation> getAnnotations() {
                return Arrays.asList(mClass.getDeclaredAnnotations());
            }
        }

        static class MethodNode extends AnnotatedNode {
            final Method mMethod;

            MethodNode(Method method) {
                mMethod = method;
            }

            @Nullable
            @Override
            AnnotatedNode getParent() {
                return new ClassNode(mMethod.getDeclaringClass());
            }

            @Override
            List<Annotation> getAnnotations() {
                return Arrays.asList(mMethod.getDeclaredAnnotations());
            }
        }
    }
}
