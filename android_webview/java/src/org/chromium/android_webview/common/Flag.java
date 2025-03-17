// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Class to represent a commandline flag. This is used by the developer UI to pass flags-to-toggle
 * over to the WebView implementation.
 */
@NullMarked
public class Flag {
    private final String mName;
    private final String mDescription;
    private final @Nullable String mEnabledStateValue;
    private final boolean mIsBaseFeature;

    /**
     * Creates a Flag which represents a {@code base::Feature}. Flags created with this method
     * overload will not have a user-visible flag description.
     */
    public static Flag baseFeature(String name) {
        return new Flag(name, /* description= */ "", /* enabledStateValue= */ null, true);
    }

    /** Creates a Flag which represents a {@code base::Feature}. */
    public static Flag baseFeature(String name, String description) {
        return new Flag(name, description, /* enabledStateValue= */ null, true);
    }

    /** Creates a Flag which represents a commandline switch. */
    public static Flag commandLine(String name, String description) {
        return new Flag(name, description, /* enabledStateValue= */ null, false);
    }

    /** Creates a Flag which represents a commandline switch with a value applied when enabled. */
    public static Flag commandLine(String name, String description, String enabledStateValue) {
        return new Flag(name, description, enabledStateValue, false);
    }

    /**
     * Calls should use {@link #baseFeature(String, String)} or {@link
     * #commandLine(String, String)} instead.
     */
    private Flag(
            String name,
            String description,
            @Nullable String enabledStateValue,
            boolean isBaseFeature) {
        mName = name;
        mDescription = description;
        mEnabledStateValue = enabledStateValue;
        mIsBaseFeature = isBaseFeature;
    }

    public String getName() {
        return mName;
    }

    public String getDescription() {
        return mDescription;
    }

    /**
     * Fetch the value to apply to the flag when enabled, or {@code null} if the flag doesn't take a
     * value.
     */
    public @Nullable String getEnabledStateValue() {
        return mEnabledStateValue;
    }

    /**
     * Indicates whether this is a {@code base::Feature} or a commandline flag.
     *
     * @return {@code true} if this is a {@code base::Feature}, {@code false} if this is a
     * commandline flag.
     */
    public boolean isBaseFeature() {
        return mIsBaseFeature;
    }
}
