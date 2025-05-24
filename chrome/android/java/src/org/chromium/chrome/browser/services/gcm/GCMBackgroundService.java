// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.services.gcm;

import org.chromium.build.annotations.IdentifierNameString;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.base.SplitCompatIntentService;

/** See {@link GCMBackgroundServiceImpl}. */
@NullMarked
public class GCMBackgroundService extends SplitCompatIntentService {
    private static final String TAG = "GCMBackgroundService";

    @SuppressWarnings("FieldCanBeFinal") // @IdentifierNameString requires non-final
    private static @IdentifierNameString String sImplClassName =
            "org.chromium.chrome.browser.services.gcm.GCMBackgroundServiceImpl";

    public GCMBackgroundService() {
        super(sImplClassName, TAG);
    }
}
