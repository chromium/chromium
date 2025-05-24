// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

/** Properties for the Tab Group Share Notice Bottom Sheet. */
@NullMarked
public class TabGroupShareNoticeBottomSheetProperties {
    public static final ReadableObjectPropertyKey<Runnable> COMPLETION_HANDLER =
            new ReadableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {
        COMPLETION_HANDLER,
    };
}
