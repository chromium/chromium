// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions;

import android.content.Context;
import android.content.res.Resources;

import androidx.annotation.PluralsRes;
import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.util.TextResolver;

/** A {@link TextResolver} that resolves either a normal or plural string resource. */
@NullMarked
public class ResourceTextResolver implements TextResolver {
    private final @StringRes int mStringResId;
    private final @PluralsRes int mPluralResId;
    private final int mCount;

    /**
     * @param stringResId The string resource ID.
     */
    public ResourceTextResolver(@StringRes int stringResId) {
        mStringResId = stringResId;
        mPluralResId = Resources.ID_NULL;
        mCount = 0;
    }

    /**
     * @param pluralResId The plural string resource ID.
     * @param count The quantity used for resolving the plural string.
     */
    public ResourceTextResolver(@PluralsRes int pluralResId, int count) {
        mStringResId = Resources.ID_NULL;
        mPluralResId = pluralResId;
        mCount = count;
    }

    @Override
    public CharSequence resolve(Context context) {
        if (mStringResId != Resources.ID_NULL) {
            return context.getString(mStringResId);
        } else if (mPluralResId != Resources.ID_NULL) {
            return context.getResources().getQuantityString(mPluralResId, mCount, mCount);
        }
        return "";
    }
}
