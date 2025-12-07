// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.Objects;

/**
 * Holder class for a {@link TabActionListener} with a {@link TabActionButtonType} describing what
 * the listener does to determine which drawable to show in the UI.
 */
@NullMarked
public class TabActionButtonData {
    @IntDef({
        TabActionButtonType.CLOSE,
        TabActionButtonType.SELECT,
        TabActionButtonType.OVERFLOW,
        TabActionButtonType.PIN
    })
    @Retention(RetentionPolicy.SOURCE)
    @Target(ElementType.TYPE_USE)
    public @interface TabActionButtonType {
        int CLOSE = 0;
        int SELECT = 1;
        int OVERFLOW = 2;
        int PIN = 3;
    }

    public final @TabActionButtonType int type;
    public final @Nullable TabActionListener tabActionListener;

    /**
     * Constructor for a {@link TabActionButtonData}.
     *
     * @param type The type of the action.
     * @param tabActionListener The listener associated with the action.
     */
    public TabActionButtonData(
            @TabActionButtonType int type, @Nullable TabActionListener tabActionListener) {
        this.type = type;
        this.tabActionListener = tabActionListener;
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof TabActionButtonData other)) return false;
        return type == other.type && Objects.equals(tabActionListener, other.tabActionListener);
    }

    @Override
    public int hashCode() {
        return Objects.hash(this.type, this.tabActionListener);
    }
}
