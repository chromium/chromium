// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.tray;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * Properties of a single download row in the download toolbar button tray. Shared by the completion
 * popup and the full tray list so both render rows identically.
 */
@NullMarked
class DownloadTrayItemProperties {
    /** The file-type icon drawable resource. */
    static final WritableIntPropertyKey ICON = new WritableIntPropertyKey();

    /** The file name. */
    static final WritableObjectPropertyKey<CharSequence> TITLE = new WritableObjectPropertyKey<>();

    /** The status line below the file name, e.g. the download's size and state. */
    static final WritableObjectPropertyKey<CharSequence> DESCRIPTION =
            new WritableObjectPropertyKey<>();

    /** Invoked when the row is clicked, to open the download. */
    static final WritableObjectPropertyKey<View.OnClickListener> ON_OPEN =
            new WritableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {ICON, TITLE, DESCRIPTION, ON_OPEN};

    private DownloadTrayItemProperties() {}
}
