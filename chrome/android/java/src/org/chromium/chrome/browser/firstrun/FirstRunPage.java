// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import androidx.fragment.app.Fragment;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.function.BooleanSupplier;

/**
 * Represents first run page shown during the First Run. Actual page implementation is created
 * lazily by {@link #instantiateFragment()}.
 *
 * @param <T> the type of the fragment that displays this FRE page.
 */
@NullMarked
public class FirstRunPage<T extends Fragment & FirstRunFragment> {
    /** Instantiates a new fragment. */
    private final Class<T> mClazz;

    /** The condition for showing the corresponding page. */
    private final BooleanSupplier mShouldShow;

    /**
     * @param clazz The Class object used for instantiating a new fragment (a 0-argument constructor
     *         is required).
     * @param shouldShow Specifies the condition for showing the corresponding page.
     */
    public FirstRunPage(Class<T> clazz, BooleanSupplier shouldShow) {
        assert shouldShow != null;

        mClazz = clazz;
        mShouldShow = shouldShow;
    }

    /**
     * @return Whether this page should be skipped, which can happen on FRE creation or page change
     *     depending on platform and cloud policies.
     */
    public boolean shouldShow() {
        return mShouldShow.getAsBoolean();
    }

    /** Creates fragment that implements this FRE page. */
    public @Nullable T instantiateFragment() {
        try {
            return mClazz.newInstance();
        } catch (IllegalAccessException | InstantiationException e) {
            return null;
        }
    }
}
