// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.two_cell;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider;
import org.chromium.chrome.browser.setup_list.SetupListCompletable;

/** Data holder for items displayed in the educational tip bottom sheet. */
@NullMarked
public class EducationalTipBottomSheetItem {
    public final EducationalTipCardProvider provider;
    public final SetupListCompletable.@Nullable CompletionState completionState;

    public EducationalTipBottomSheetItem(
            EducationalTipCardProvider provider,
            SetupListCompletable.@Nullable CompletionState completionState) {
        this.provider = provider;
        this.completionState = completionState;
    }
}
