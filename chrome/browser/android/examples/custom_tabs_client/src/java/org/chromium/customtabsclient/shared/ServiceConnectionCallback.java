// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.customtabsclient.shared;

import androidx.browser.customtabs.CustomTabsClient;

import org.chromium.build.annotations.NullMarked;

/** Callback for events when connecting and disconnecting from Custom Tabs Service. */
@NullMarked
public interface ServiceConnectionCallback {
    /**
     * Called when the service is connected.
     * @param client a CustomTabsClient
     */
    void onServiceConnected(CustomTabsClient client);

    /** Called when the service is disconnected. */
    void onServiceDisconnected();
}
