// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.printing;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.ServiceImpl;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.prefs.PrefService;

/** Default implementation of {@link PrintingDelegate}. */
@NullMarked
@ServiceImpl(PrintingDelegate.class)
public final class PrintingDelegateImpl implements PrintingDelegate {
    @Override
    public boolean isPrintingEnabled(PrefService prefs) {
        return prefs.getBoolean(Pref.PRINTING_ENABLED);
    }

    @Override
    public boolean print(@Nullable Tab tab) {
        return PrintHelper.printTab(tab);
    }
}
