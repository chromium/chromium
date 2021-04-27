// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Contains all the properties for the download later dialog {@link PropertyModel}.
 */
public class DownloadLaterDialogProperties {
    public static final PropertyModel
            .ReadableObjectPropertyKey<DownloadLaterDialogView.Controller> CONTROLLER =
            new PropertyModel.ReadableObjectPropertyKey();

    /** The initial choice of the download later dialog. */
    public static final PropertyModel.ReadableIntPropertyKey INITIAL_CHOICE =
            new PropertyModel.ReadableIntPropertyKey();

    /** The initial selection to define the don't show again checkbox. */
    public static final PropertyModel.ReadableIntPropertyKey DONT_SHOW_AGAIN_SELECTION =
            new PropertyModel.ReadableIntPropertyKey();

    /** Whether the don't show again checkbox is disabled. */
    public static final PropertyModel.WritableBooleanPropertyKey DONT_SHOW_AGAIN_DISABLED =
            new PropertyModel.WritableBooleanPropertyKey();

    /**
     * The string representing the download location. If null, no download location edit text will
     * be shown.
     */
    public static final PropertyModel.WritableObjectPropertyKey<String> LOCATION_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>();

    /**
     * The subtitle of the download later dialog.
     */
    public static final PropertyModel.ReadableObjectPropertyKey<CharSequence> SUBTITLE_TEXT =
            new PropertyModel.ReadableObjectPropertyKey<>();

    /**
     * Whether to show the option to pick date and time to start download.
     */
    public static final PropertyModel.ReadableBooleanPropertyKey SHOW_DATE_TIME_PICKER_OPTION =
            new PropertyModel.ReadableBooleanPropertyKey();

    public static final PropertyKey[] ALL_DOWNLOAD_LATER_DIALOG_PROPERTIES = new PropertyKey[] {
            CONTROLLER, INITIAL_CHOICE, DONT_SHOW_AGAIN_SELECTION, DONT_SHOW_AGAIN_DISABLED,
            LOCATION_TEXT, SUBTITLE_TEXT, SHOW_DATE_TIME_PICKER_OPTION};

    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(ALL_DOWNLOAD_LATER_DIALOG_PROPERTIES,
                    new PropertyKey[] {DownloadDateTimePickerDialogProperties.INITIAL_TIME});
}
