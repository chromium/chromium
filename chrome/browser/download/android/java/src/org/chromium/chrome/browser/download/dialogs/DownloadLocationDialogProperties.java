// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Properties used by the MVC model between {@link DownloadLocationDialogCoordinator} and
 * {@link DownloadLocationCustomView}.
 */
class DownloadLocationDialogProperties {
    /** The title text of the download location dialog. */
    static final PropertyModel.ReadableObjectPropertyKey<CharSequence> TITLE =
            new PropertyModel.ReadableObjectPropertyKey<>();

    /** The subtitle text of the download location dialog. */
    static final PropertyModel.ReadableObjectPropertyKey<CharSequence> SUBTITLE =
            new PropertyModel.ReadableObjectPropertyKey<>();

    /** Whether to show the subtitle in the download location dialog. Default to false. */
    static final PropertyModel.ReadableBooleanPropertyKey SHOW_SUBTITLE =
            new PropertyModel.ReadableBooleanPropertyKey();

    /** Whether to show the Incognito download warning. */
    static final PropertyModel.ReadableBooleanPropertyKey SHOW_INCOGNITO_WARNING =
            new PropertyModel.ReadableBooleanPropertyKey();

    /** The file name shown in the download location dialog. */
    static final PropertyModel.ReadableObjectPropertyKey<CharSequence> FILE_NAME =
            new PropertyModel.ReadableObjectPropertyKey<>();

    /** The file size shown under the file name, currently only used by smart suggestion. */
    static final PropertyModel.ReadableObjectPropertyKey<CharSequence> FILE_SIZE =
            new PropertyModel.ReadableObjectPropertyKey<>();

    /**
     * Whether to show location available space text, used by the smart suggestion. Default to
     * false.
     */
    static final PropertyModel.ReadableBooleanPropertyKey SHOW_LOCATION_AVAILABLE_SPACE =
            new PropertyModel.ReadableBooleanPropertyKey();

    /** Whether the don't show again checkbox is checked. Default to false. */
    static final PropertyModel.ReadableBooleanPropertyKey DONT_SHOW_AGAIN_CHECKBOX_CHECKED =
            new PropertyModel.ReadableBooleanPropertyKey();

    /** Whether to show the don't show again checkbox. Default to false. */
    static final PropertyModel.ReadableBooleanPropertyKey DONT_SHOW_AGAIN_CHECKBOX_SHOWN =
            new PropertyModel.ReadableBooleanPropertyKey();

    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                TITLE,
                SUBTITLE,
                SHOW_SUBTITLE,
                SHOW_INCOGNITO_WARNING,
                FILE_NAME,
                FILE_SIZE,
                SHOW_LOCATION_AVAILABLE_SPACE,
                DONT_SHOW_AGAIN_CHECKBOX_CHECKED,
                DONT_SHOW_AGAIN_CHECKBOX_SHOWN
            };
}
