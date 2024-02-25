// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.params;

import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.TestClass;

import org.chromium.base.test.params.ParameterizedRunner.ParameterizedTestInstantiationException;

import java.lang.reflect.InvocationTargetException;
import java.util.List;

/**
 * Parameterized runner delegate common that implements method that needed to be
 * delegated for parameterization purposes
 */
public final class ParameterizedRunnerDelegateCommon {
    private final TestClass mTestClass;
    private final ParameterSet mClassParameterSet;
    private final List<FrameworkMethod> mParameterizedFrameworkMethodList;

    public ParameterizedRunnerDelegateCommon(
            TestClass testClass,
            ParameterSet classParameterSet,
            List<FrameworkMethod> parameterizedFrameworkMethods) {
        mTestClass = testClass;
        mClassParameterSet = classParameterSet;
        mParameterizedFrameworkMethodList = parameterizedFrameworkMethods;
    }

    /**
     * Do not do any validation here because running the default class runner's
     * collectInitializationErrors fail due to the overridden computeTestMethod relying on a local
     * member variable
     *
     * The validation needed for parameterized tests is already done by ParameterizedRunner.
     */
    public static void collectInitializationErrors(
            @SuppressWarnings("unused") List<Throwable> errors) {}

    public List<FrameworkMethod> computeTestMethods() {
        return mParameterizedFrameworkMethodList;
    }

    private void throwInstantiationException(Exception e)
            throws ParameterizedTestInstantiationException {
        String parameterSetString =
                mClassParameterSet == null ? "null" : mClassParameterSet.toString();
        throw new ParameterizedTestInstantiationException(mTestClass, parameterSetString, e);
    }

    public Object createTest() throws ParameterizedTestInstantiationException {
        try {
            if (mClassParameterSet == null) {
                return mTestClass.getOnlyConstructor().newInstance();
            }
            return mTestClass
                    .getOnlyConstructor()
                    .newInstance(mClassParameterSet.getValues().toArray());
        } catch (InstantiationException e) {
            throwInstantiationException(e);
        } catch (IllegalAccessException e) {
            throwInstantiationException(e);
        } catch (InvocationTargetException e) {
            throwInstantiationException(e);
        }
        assert false;
        return null;
    }
}
