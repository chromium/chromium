// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.chromium.base.annotations.CalledByNative;

/**
 * A simple 2-argument callback with an int and a String as arguments.
 *
 * This is used to call a 2-argument Java callback from C++ code. The generic {@link
 * org.chromium.base.Callback} cannot be used because it is limited to a single argument.
 * Alternative approaches like encoding the two arguments into one string or one array of objects
 * with different types were considered, but their downside was both a lot of boilerplate (creating
 * the composed object in C++ and checking and decoding it in Java) and lack of clarity. This
 * 2-argument callback also adds a few code lines but it is clear and the compiler does the type
 * checking.
 */
public interface IntStringCallback {
    /**
     * Invoked with the result of a computation.
     *
     * @param number Integer part of the result.
     * @param string String part of the result.
     */
    @CalledByNative
    void onResult(int number, String string);
}
