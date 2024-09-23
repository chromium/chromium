// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.app.Activity;

import androidx.annotation.Nullable;

/** This interface is implemented by FRE fragments. */
public interface FirstRunFragment {
    /**
     * Notifies that the object returned by {@link #getPageDelegate()} and its dependencies have
     * been fully initialized, including native initialization.
     *
     * <p>TODO(crbug.com/40232440): Remove this method.
     *
     * @deprecated Use {@link FirstRunPageDelegate#getNativeInitializationPromise()} instead.
     */
    @Deprecated
    default void onNativeInitialized() {}

    /** @see androidx.fragment.app.Fragment#getActivity(). */
    @Nullable
    Activity getActivity();

    /**
     * Set the a11y focus when the fragment is shown on the screen.
     *
     * Android ViewPager cannot always assign the correct a11y focus automatically when switching
     * between pages. See https://crbug.com/1094064 for more detail.
     *
     * Note that this function can be called before views for the fragment is created. To avoid NPE,
     * it is suggested to add null checker inside this function implementation. See
     * https://crbug.com/1140174 for more detail.
     */
    void setInitialA11yFocus();

    /**
     * Convenience method to get {@link FirstRunPageDelegate}. Be carefully calling this in response
     * to async events, as once this fragment is detached, this will return null.
     */
    default @Nullable FirstRunPageDelegate getPageDelegate() {
        return (FirstRunPageDelegate) getActivity();
    }

    /**
     * Reset the fragment state. This can be used when the fragment is revisited with back button.
     */
    default void reset() {}
}
