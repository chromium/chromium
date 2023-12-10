// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import android.content.Intent;

/* package */ interface ChildServiceConnectionFactory {
    ChildServiceConnection createConnection(
            Intent bindIntent,
            int bindFlags,
            ChildServiceConnectionDelegate delegate,
            String instanceName);
}
