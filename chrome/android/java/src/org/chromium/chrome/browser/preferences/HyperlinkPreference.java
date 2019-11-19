// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.content.Context;
import android.content.res.TypedArray;
import android.support.v7.preference.Preference;
import android.util.AttributeSet;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.ui.base.LocalizationUtils;

/**
 * A preference that navigates to an URL.
 */
public class HyperlinkPreference extends Preference {

    private final int mUrlResId;

    public HyperlinkPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        TypedArray a = context.obtainStyledAttributes(attrs,
                R.styleable.HyperlinkPreference, 0, 0);
        mUrlResId = a.getResourceId(R.styleable.HyperlinkPreference_url, 0);
        a.recycle();
        setSingleLineTitle(false);
    }

    @Override
    protected void onClick() {
        CustomTabActivity.showInfoPage(ContextUtils.activityFromContext(getContext()),
                LocalizationUtils.substituteLocalePlaceholder(getContext().getString(mUrlResId)));
    }
}
