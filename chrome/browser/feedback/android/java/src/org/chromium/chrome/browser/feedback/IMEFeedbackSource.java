// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.provider.Settings;
import android.text.TextUtils;
import android.util.Pair;

import org.chromium.base.CollectionUtil;
import org.chromium.base.ContextUtils;

import java.util.Map;

/** Grabs feedback about the default IME input method. */
public class IMEFeedbackSource implements FeedbackSource {
    IMEFeedbackSource() {}

    @Override
    public Map<String, String> getFeedback() {
        String imePackage =
                Settings.Secure.getString(
                        ContextUtils.getApplicationContext().getContentResolver(),
                        Settings.Secure.DEFAULT_INPUT_METHOD);
        if (TextUtils.isEmpty(imePackage)) return null;
        return CollectionUtil.newHashMap(Pair.create("Default IME", imePackage));
    }
}
