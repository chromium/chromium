// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.content.Context;

import androidx.fragment.app.Fragment;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Helpers for finding the {@link SettingsHost} that hosts a settings fragment. */
@NullMarked
public class SettingsHostUtil {
    private SettingsHostUtil() {}

    /**
     * Returns whether the given fragment is part of a settings UI that is hosted inside a browser
     * tab. Returns false if no {@link SettingsHost} can be found, which means the fragment is
     * hosted in a plain activity (e.g. a settings activity, or a test activity).
     *
     * <p>Prefer injecting the value from the code that creates the settings UI. Use this method
     * only for fragments that are instantiated by the framework and therefore cannot be given the
     * value directly.
     *
     * @param fragment The fragment whose settings host is being looked up.
     */
    public static boolean isShownInTab(Fragment fragment) {
        return isShownInTab(fragment, /* attachContext= */ null);
    }

    /**
     * Returns whether the given fragment is part of a settings UI that is hosted inside a browser
     * tab. See {@link #isShownInTab(Fragment)}.
     *
     * @param fragment The fragment whose settings host is being looked up.
     * @param attachContext The context passed to {@code Fragment.onAttach()}, or null outside of
     *     {@code onAttach()}. {@code Fragment.getActivity()} returns null during {@code
     *     onAttach()}, so callers in {@code onAttach()} must pass the context to allow the host
     *     activity to be found. Parent fragments are already set during {@code onAttach()}, so they
     *     are found either way.
     */
    public static boolean isShownInTab(Fragment fragment, @Nullable Context attachContext) {
        // Walk up the fragment tree first. The host fragment is more specific than the activity,
        // which may host other UI in addition to settings (e.g. ChromeTabbedActivity).
        for (Fragment current = fragment; current != null; current = current.getParentFragment()) {
            if (current instanceof SettingsHost host) return host.isShownInTab();
        }
        Context context = attachContext != null ? attachContext : fragment.getActivity();
        return context instanceof SettingsHost host && host.isShownInTab();
    }
}
