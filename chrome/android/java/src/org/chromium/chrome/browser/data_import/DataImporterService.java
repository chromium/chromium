// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_import;

import org.chromium.build.annotations.IdentifierNameString;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.base.SplitCompatService;

/** See {@link DataImporterServiceImpl}. */
@NullMarked
public class DataImporterService extends SplitCompatService {
    @SuppressWarnings("FieldCanBeFinal") // @IdentifierNameString requires non-final
    private static @IdentifierNameString String sImplClassName =
            "org.chromium.chrome.browser.data_import.DataImporterServiceImpl";

    public DataImporterService() {
        super(sImplClassName, /* inOnDemandSplit= */ true);
    }
}
