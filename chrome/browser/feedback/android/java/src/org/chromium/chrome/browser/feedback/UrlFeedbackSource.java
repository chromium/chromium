// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.text.TextUtils;
import android.util.Pair;

import androidx.annotation.Nullable;

import org.chromium.base.CollectionUtil;

import java.util.Map;

/** Grabs feedback about the current URL. */
class UrlFeedbackSource implements FeedbackSource {
    private static final String URL_KEY = "URL";

    private final String mUrl;

    UrlFeedbackSource(@Nullable String url) {
        mUrl = url;
    }

    @Override
    public Map<String, String> getFeedback() {
        if (TextUtils.isEmpty(mUrl)) return null;
        return CollectionUtil.newHashMap(Pair.create(URL_KEY, mUrl));
    }
}
