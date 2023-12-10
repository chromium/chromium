// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.params;

import org.junit.runners.model.FrameworkMethod;

import org.chromium.base.test.params.ParameterizedRunner.ParameterizedTestInstantiationException;

import java.util.List;

/**
 * This interface defines the methods that needs to be overriden for a Runner to
 * be used by ParameterizedRunner to generate individual runners for parameters.
 *
 * To create a ParameterizedRunnerDelegate, extends from any BlockJUnit4Runner
 * children class. You can copy all the implementation from
 * org.chromium.base.test.params.BaseJUnit4RunnerDelegate.
 */
public interface ParameterizedRunnerDelegate {
    /** Override to use DelegateCommon's implementation */
    void collectInitializationErrors(List<Throwable> errors);

    /** Override to use DelegateCommon's implementation */
    List<FrameworkMethod> computeTestMethods();

    /** Override to use DelegateCommon's implementation */
    Object createTest() throws ParameterizedTestInstantiationException;
}
