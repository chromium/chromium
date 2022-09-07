// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.params;

import org.junit.runners.model.Statement;

import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameterAfter;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameterBefore;

import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.ArrayList;
import java.util.List;

/**
 * Processes {@link UseMethodParameterBefore} and {@link UseMethodParameterAfter} annotations to run
 * the corresponding methods. To use, add an instance to the test class and annotate it with
 * {@code @}{@link org.junit.Rule Rule}.
 */
public class MethodParamAnnotationRule extends MethodParamRule {
    @Override
    protected Statement applyParameterAndValues(final Statement base, Object target,
            Class<? extends ParameterProvider> parameterProvider, List<Object> values) {
        final List<Method> beforeMethods = new ArrayList<>();
        final List<Method> afterMethods = new ArrayList<>();
        for (Method m : target.getClass().getDeclaredMethods()) {
            if (!m.getReturnType().equals(Void.TYPE)) continue;
            if (!Modifier.isPublic(m.getModifiers())) continue;

            UseMethodParameterBefore beforeAnnotation =
                    m.getAnnotation(UseMethodParameterBefore.class);
            if (beforeAnnotation != null && beforeAnnotation.value().equals(parameterProvider)) {
                beforeMethods.add(m);
            }

            UseMethodParameterAfter afterAnnotation =
                    m.getAnnotation(UseMethodParameterAfter.class);
            if (afterAnnotation != null && afterAnnotation.value().equals(parameterProvider)) {
                afterMethods.add(m);
            }
        }

        if (beforeMethods.isEmpty() && afterMethods.isEmpty()) return base;

        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                for (Method m : beforeMethods) {
                    m.invoke(target, values.toArray());
                }

                base.evaluate();

                for (Method m : afterMethods) {
                    m.invoke(target, values.toArray());
                }
            }
        };
    }
}
