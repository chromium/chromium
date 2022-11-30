// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common.services;

import android.os.Bundle;

/**
 * Used to communicate variations seed related information from WebView's
 * service to an embedding app.
 */
oneway interface IVariationsSeedServerCallback {
    // Notifies the embedding app that metrics related to the variations
    // service are available for reporting. See VariationsServiceMetricsHelper
    // for information on the contents of the |metrics| parameter.
    void reportVariationsServiceMetrics(in Bundle metrics);
}
