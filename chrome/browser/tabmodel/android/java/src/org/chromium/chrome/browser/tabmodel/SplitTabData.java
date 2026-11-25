// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import com.google.errorprone.annotations.DoNotMock;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.split_tabs.SplitTabId;
import org.chromium.components.split_tabs.SplitTabVisualData;

import java.util.List;
import java.util.Objects;

/**
 * Java counterpart to native {@code split_tabs::SplitTabData}.
 *
 * <p>Unlike {@link SplitTabId} and {@link SplitTabVisualData}, this class lives in {@code
 * //chrome/browser/tabmodel/} rather than {@code //components/} because it references {@link Tab}
 * ({@code //chrome/browser/tab:java}), and {@code //components/} cannot depend on {@code
 * //chrome/browser/}.
 */
@NullMarked
@DoNotMock("This is a simple value object.")
public final class SplitTabData {
    public final SplitTabId splitId;
    public final List<Tab> tabs;
    public final SplitTabVisualData visualData;

    /**
     * Constructs a {@link SplitTabData}.
     *
     * @param splitId The {@link SplitTabId} identifying the Split View.
     * @param tabs The ordered list of {@link Tab}s in the Split View.
     * @param visualData The {@link SplitTabVisualData} describing layout and divider ratio.
     */
    @CalledByNative
    public SplitTabData(
            @JniType("split_tabs::SplitTabId") SplitTabId splitId,
            @JniType("std::vector<TabAndroid*>") List<Tab> tabs,
            @JniType("split_tabs::SplitTabVisualData") SplitTabVisualData visualData) {
        this.splitId = splitId;
        this.tabs = List.copyOf(tabs);
        this.visualData = visualData;
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (this == obj) {
            return true;
        }

        if (!(obj instanceof SplitTabData other)) {
            return false;
        }

        return Objects.equals(splitId, other.splitId)
                && Objects.equals(tabs, other.tabs)
                && Objects.equals(visualData, other.visualData);
    }

    @Override
    public int hashCode() {
        return Objects.hash(splitId, tabs, visualData);
    }
}
