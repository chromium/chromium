// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

/** Exception indicating that a Criteria did not match expectations. */
public class CriteriaNotSatisfiedException extends AssertionError {
    /**
     * @param msg The reason the criteria was not met.
     */
    public CriteriaNotSatisfiedException(String msg) {
        super(msg);
    }

    /**
     * @param cause The underlying exception that prevented the Criteria.
     */
    public CriteriaNotSatisfiedException(Throwable cause) {
        super(cause);
    }
}
