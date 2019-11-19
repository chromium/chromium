// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics.test;

import org.junit.rules.ExternalResource;

import org.chromium.base.metrics.RecordHistogram;

/**
 * Disables histogram recording for the duration of the tests.
 */
public class DisableHistogramsRule extends ExternalResource {
    @Override
    protected void before() {
        RecordHistogram.setDisabledForTests(true);
    }

    @Override
    protected void after() {
        RecordHistogram.setDisabledForTests(false);
    }
}
