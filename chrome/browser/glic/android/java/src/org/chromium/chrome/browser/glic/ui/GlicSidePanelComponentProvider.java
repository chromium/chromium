// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic.ui;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab_bottom_sheet.CoBrowseComponentProvider;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;

/** Concrete implementation of {@link CoBrowseComponentProvider} for Glic side panel. */
@JNINamespace("glic")
@NullMarked
public class GlicSidePanelComponentProvider implements CoBrowseComponentProvider {

    /** JNI static factory method to create the provider. */
    @CalledByNative
    private static GlicSidePanelComponentProvider createProvider() {
        return new GlicSidePanelComponentProvider();
    }

    private GlicSidePanelComponentProvider() {}

    @Override
    public boolean setupPlaceholderView(TextViewWithCompoundDrawables placeholder) {
        GlicUiUtils.setupPlaceholderView(placeholder);
        return true;
    }
}
