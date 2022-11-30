// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.os.Build;
import android.webkit.PacProcessor;

import androidx.annotation.RequiresApi;

/**
 * Utility class to use new APIs that were added in R (API level 30). These need to exist in a
 * separate class so that Android framework can successfully verify glue layer classes without
 * encountering the new APIs. Note that GlueApiHelper is only for APIs that cannot go to ApiHelper
 * in base/, for reasons such as using system APIs or instantiating an adapter class that is
 * specific to glue layer.
 */
@RequiresApi(Build.VERSION_CODES.R)
public final class GlueApiHelperForR {
    private GlueApiHelperForR() {}

    public static PacProcessor getPacProcessor() {
        return PacProcessorImpl.getInstance();
    }

    public static PacProcessor createPacProcessor() {
        return PacProcessorImpl.createInstance();
    }
}
