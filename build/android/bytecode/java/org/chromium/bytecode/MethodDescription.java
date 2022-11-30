// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.bytecode;

class MethodDescription {
    public final String methodName;
    public final String description;
    public final int access;
    public Boolean shouldCreateOverride;

    public MethodDescription(String methodName, String description, int access) {
        this.methodName = methodName;
        this.description = description;
        this.access = access;
        // A null value means we haven't checked the method.
        this.shouldCreateOverride = null;
    }
}
