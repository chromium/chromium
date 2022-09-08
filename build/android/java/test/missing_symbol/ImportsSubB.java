// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package test.missing_symbol;

import test.missing_symbol.sub.SubB;

public class ImportsSubB {
    public ImportsSubB() {
        new SubB().foo();
    }
}
