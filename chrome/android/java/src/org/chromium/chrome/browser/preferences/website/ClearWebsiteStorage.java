// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.content.Context;
import android.support.v7.preference.DialogPreference;
import android.util.AttributeSet;

/**
 * Dialog that prompts the user to clear website storage on the device.
 */
public class ClearWebsiteStorage extends DialogPreference {

    public ClearWebsiteStorage(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    public ClearWebsiteStorage(Context context, AttributeSet attrs) {
        super(context, attrs);
    }
}
