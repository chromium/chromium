// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.tray;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * Properties of a single download row in the download toolbar button tray. Shared by the completion
 * popup and the full tray list so both render rows identically.
 */
@NullMarked
class DownloadTrayItemProperties {
    /** The view type of the rows in the download tray list, which all share one layout. */
    static final int ROW_TYPE = 0;

    /** The {@link #PROGRESS} of a download whose size, and so how far along it is, is unknown. */
    static final int PROGRESS_INDETERMINATE = -1;

    /** The file-type icon drawable resource. */
    static final WritableIntPropertyKey ICON = new WritableIntPropertyKey();

    /** The file name. */
    static final WritableObjectPropertyKey<CharSequence> TITLE = new WritableObjectPropertyKey<>();

    /** The status line below the file name, e.g. the download's size and state. */
    static final WritableObjectPropertyKey<CharSequence> DESCRIPTION =
            new WritableObjectPropertyKey<>();

    /** Invoked when the row is clicked, to open the download; null if there is nothing to open. */
    static final WritableObjectPropertyKey<View.@Nullable OnClickListener> ON_OPEN =
            new WritableObjectPropertyKey<>();

    /** Whether the download is in progress, which shows the progress bar. */
    static final WritableBooleanPropertyKey IS_IN_PROGRESS = new WritableBooleanPropertyKey();

    /**
     * How much of a download in progress has been received, from 0 to 100, or {@link
     * #PROGRESS_INDETERMINATE}, as desktop's progress bar value.
     */
    static final WritableIntPropertyKey PROGRESS = new WritableIntPropertyKey();

    /** Whether the download is paused. */
    static final WritableBooleanPropertyKey IS_PAUSED = new WritableBooleanPropertyKey();

    static final PropertyKey[] ALL_KEYS = {
        ICON, TITLE, DESCRIPTION, ON_OPEN, IS_IN_PROGRESS, PROGRESS, IS_PAUSED
    };

    private DownloadTrayItemProperties() {}
}
