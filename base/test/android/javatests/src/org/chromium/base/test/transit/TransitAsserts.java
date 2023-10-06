// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

/** Assertions specific to Public Transit. */
public class TransitAsserts {
    public static void assertFinalDestination(TransitStation station) {
        // TODO(crbug.com/1489446): Keep track of past stations and check that the last active
        // station was |station|.
    }
}
