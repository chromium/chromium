// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Properties used by the MVC model between {@link OpenDownloadDialogCoordinator} and {@link
 * OpenDownloadCustomView}.
 */
class OpenDownloadDialogProperties {
    /** The title text of the open download dialog. */
    static final PropertyModel.ReadableObjectPropertyKey<CharSequence> TITLE =
            new PropertyModel.ReadableObjectPropertyKey<>();

    /** The subtitle text of the open download dialog. */
    static final PropertyModel.ReadableObjectPropertyKey<CharSequence> SUBTITLE =
            new PropertyModel.ReadableObjectPropertyKey<>();

    /** Whether the auto open checkbox is checked. Default to false. */
    static final PropertyModel.ReadableBooleanPropertyKey AUTO_OPEN_CHECKBOX_CHECKED =
            new PropertyModel.ReadableBooleanPropertyKey();

    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {TITLE, SUBTITLE, AUTO_OPEN_CHECKBOX_CHECKED};
}
