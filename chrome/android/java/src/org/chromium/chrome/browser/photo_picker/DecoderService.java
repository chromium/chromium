// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.photo_picker;

import org.chromium.build.annotations.IdentifierNameString;
import org.chromium.chrome.browser.base.SplitCompatService;

/** See {@link DecoderServiceImpl}. */
public class DecoderService extends SplitCompatService {
    private static @IdentifierNameString String sImplClassName =
            "org.chromium.chrome.browser.photo_picker.DecoderServiceImpl";

    public DecoderService() {
        super(sImplClassName);
    }
}
