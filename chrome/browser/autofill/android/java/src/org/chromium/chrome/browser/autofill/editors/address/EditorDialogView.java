// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.address;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.editors.common.EditorViewBase;

/**
 * The editor dialog. Can be used for editing contact information, shipping address, billing
 * address.
 *
 * <p>TODO(crbug.com/41363594): Move payment specific functionality to separate class.
 */
@NullMarked
public class EditorDialogView extends EditorViewBase {

    /**
     * Builds the editor dialog.
     *
     * @param activity The activity on top of which the UI should be displayed.
     */
    public EditorDialogView(Activity activity) {
        super(activity);
    }
}
