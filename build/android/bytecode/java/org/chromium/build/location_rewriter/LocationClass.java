// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build.location_rewriter;

import org.objectweb.asm.Opcodes;

/** Class that contains information related to {@link org.chromium.base.task.Location} */
public final class LocationClass {
    public static final String NAME = "org/chromium/base/task/Location";
    public static final String DESCRIPTOR = "L" + NAME + ";";
    public static final VisitableMethod FROM_METHOD =
            new VisitableMethod(
                    Opcodes.INVOKESTATIC,
                    NAME,
                    "from",
                    "(Ljava/lang/String;Ljava/lang/String;I)L" + NAME + ";",
                    false);
}
