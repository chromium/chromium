// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import androidx.annotation.IntDef;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The properties for download date time picker dialog UI MVC.
 */
public class DownloadDateTimePickerDialogProperties {
    /**
     * The state of the date time picker dialog.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({State.DATE, State.TIME})
    public @interface State {
        /** The user is picking the date in the dialog. */
        int DATE = 0;
        /** The user is picking the time in the dialog. */
        int TIME = 1;
    }

    /**
     * The initial date and time as a unix timestamp shown on the download date time picker dialog.
     */
    public static final ReadableObjectPropertyKey<Long> INITIAL_TIME =
            new ReadableObjectPropertyKey<>();

    /**
     * The minimum time for the user to select in the date time picker. The time to select should be
     * a future time.
     */
    public static final ReadableObjectPropertyKey<Long> MIN_TIME =
            new ReadableObjectPropertyKey<>();

    /**
     * The maximum time for the user to select in the date time picker.
     */
    public static final ReadableObjectPropertyKey<Long> MAX_TIME =
            new ReadableObjectPropertyKey<>();

    /**
     * The state of the download date time picker dialog. See {@link #STATE}.
     */
    public static final WritableIntPropertyKey STATE = new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {INITIAL_TIME, MIN_TIME, MAX_TIME, STATE};
}
