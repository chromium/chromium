// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

/** A {@link android.app.Service} that creates a new {@link EmbeddedTestServer} when bound. */
public class AwEmbeddedTestServerService extends Service {
    @Override
    public IBinder onBind(Intent intent) {
        return new AwEmbeddedTestServerImpl(this);
    }
}
