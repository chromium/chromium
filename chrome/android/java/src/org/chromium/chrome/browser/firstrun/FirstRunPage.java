// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

/**
 * Represents first run page shown during the First Run. Actual page implementation is created
 * lazily by {@link #instantiateFragment()}.
 * @param <T> the type of the fragment that displays this FRE page.
 */
public class FirstRunPage<T extends Fragment & FirstRunFragment> {
    /** Specifies a condition for skipping a promo page during first run. */
    interface SkipPageCondition {
        /**
         * @param freProperties The up-to-date bundle that indicates what promo pages should be
         *         skipped.
         * @return true if the page should be skipped.
         */
        boolean shouldSkip(Bundle freProperties);
    }

    /** Instantiates a new fragment. */
    private final Class<T> mClazz;

    /** The condition for skipping the corresponding promo page. */
    private final @Nullable SkipPageCondition mCondition;

    /**
     * @param clazz The Class object used for instantiating a new fragment
     * @param condition Specifies the condition for skipping the corresponding promo page
     */
    public FirstRunPage(Class<T> clazz, @Nullable SkipPageCondition condition) {
        mClazz = clazz;
        mCondition = condition;
    }

    /** @param clazz The Class object used for instantiating a new fragment */
    public FirstRunPage(Class<T> clazz) {
        this(clazz, null);
    }

    /**
     * @return Whether this page should be skipped, which can happen on FRE creation or page change
     *         depending on platform and cloud policies.
     */
    public boolean shouldSkipPage(Bundle freProperties) {
        return mCondition != null && mCondition.shouldSkip(freProperties);
    }

    /**
     * Creates fragment that implements this FRE page.
     */
    public T instantiateFragment() {
        try {
            return mClazz.newInstance();
        } catch (IllegalAccessException | InstantiationException e) {
            return null;
        }
    }
}
