// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import java.util.Locale;
import java.util.Objects;

class HubColorSchemeUpdate {
    public final @HubColorScheme int newColorScheme;
    public final @HubColorScheme int previousColorScheme;
    public final boolean animate;

    public HubColorSchemeUpdate(
            @HubColorScheme int newColorScheme,
            @HubColorScheme int previousColorScheme,
            boolean animate) {
        this.newColorScheme = newColorScheme;
        this.previousColorScheme = previousColorScheme;
        this.animate = animate;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o instanceof HubColorSchemeUpdate that) {
            return previousColorScheme == that.previousColorScheme
                    && newColorScheme == that.newColorScheme
                    && animate == that.animate;
        }
        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(previousColorScheme, newColorScheme, animate);
    }

    @Override
    public String toString() {
        return String.format(
                Locale.getDefault(),
                "HubColorSchemeUpdate{newColorScheme=%d, previousColorScheme=%d, animate=%b}",
                newColorScheme,
                previousColorScheme,
                animate);
    }
}
