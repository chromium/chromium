// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package test.missing_symbol.sub;

import test.missing_symbol.B;

public class BInMethodSignature {
    public B foo() {
        return new B();
    }
}
