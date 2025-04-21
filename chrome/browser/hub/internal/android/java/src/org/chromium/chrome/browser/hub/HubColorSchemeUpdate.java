// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import org.chromium.build.annotations.NullMarked;

import java.util.Locale;
import java.util.Objects;

/** Represents color scheme updates to be made in the Hub. */
@NullMarked
public class HubColorSchemeUpdate {
    public final @HubColorScheme int newColorScheme;
    public final @HubColorScheme int previousColorScheme;

    public HubColorSchemeUpdate(
            @HubColorScheme int newColorScheme, @HubColorScheme int previousColorScheme) {
        this.newColorScheme = newColorScheme;
        this.previousColorScheme = previousColorScheme;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o instanceof HubColorSchemeUpdate that) {
            return previousColorScheme == that.previousColorScheme
                    && newColorScheme == that.newColorScheme;
        }
        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(previousColorScheme, newColorScheme);
    }

    @Override
    public String toString() {
        return String.format(
                Locale.getDefault(),
                "HubColorSchemeUpdate{newColorScheme=%d, previousColorScheme=%d}",
                newColorScheme,
                previousColorScheme);
    }
}
