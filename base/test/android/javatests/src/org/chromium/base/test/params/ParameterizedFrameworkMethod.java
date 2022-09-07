// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.params;

import org.junit.runners.model.FrameworkMethod;

import java.lang.annotation.Annotation;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

/**
 * Custom FrameworkMethod that includes a {@code ParameterSet} that
 * represents the parameters for this test method
 */
public class ParameterizedFrameworkMethod extends FrameworkMethod {
    private ParameterSet mParameterSet;
    private String mName;

    public ParameterizedFrameworkMethod(
            Method method, ParameterSet parameterSet, String classParameterSetName) {
        super(method);
        mParameterSet = parameterSet;
        String postFix = "";
        if (classParameterSetName != null && !classParameterSetName.isEmpty()) {
            postFix += "_" + classParameterSetName;
        }
        if (parameterSet != null && !parameterSet.getName().isEmpty()) {
            postFix += "_" + parameterSet.getName();
        }
        mName = postFix.isEmpty() ? method.getName() : method.getName() + "_" + postFix;
    }

    @Override
    public String getName() {
        return mName;
    }

    @Override
    public Object invokeExplosively(Object target, Object... params) throws Throwable {
        if (mParameterSet != null) {
            return super.invokeExplosively(target, mParameterSet.getValues().toArray());
        }
        return super.invokeExplosively(target, params);
    }

    static List<FrameworkMethod> wrapAllFrameworkMethods(
            Collection<FrameworkMethod> frameworkMethods, String classParameterSetName) {
        List<FrameworkMethod> results = new ArrayList<>();
        for (FrameworkMethod frameworkMethod : frameworkMethods) {
            results.add(new ParameterizedFrameworkMethod(
                    frameworkMethod.getMethod(), null, classParameterSetName));
        }
        return results;
    }

    @Override
    public boolean equals(Object obj) {
        if (obj instanceof ParameterizedFrameworkMethod) {
            ParameterizedFrameworkMethod method = (ParameterizedFrameworkMethod) obj;
            return super.equals(obj) && method.getParameterSet().equals(getParameterSet())
                    && method.getName().equals(getName());
        }
        return false;
    }

    /**
     * Override hashCode method to distinguish two ParameterizedFrameworkmethod with same
     * Method object.
     */
    @Override
    public int hashCode() {
        int result = 17;
        result = 31 * result + super.hashCode();
        result = 31 * result + getName().hashCode();
        if (getParameterSet() != null) {
            result = 31 * result + getParameterSet().hashCode();
        }
        return result;
    }

    Annotation[] getTestAnnotations() {
        // TODO(yolandyan): add annotation from the ParameterSet, enable
        // test writing to add SkipCheck for an individual parameter
        return getMethod().getAnnotations();
    }

    public ParameterSet getParameterSet() {
        return mParameterSet;
    }
}
