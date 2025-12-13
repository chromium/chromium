// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Token;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.tab_groups.TabGroupColorId;

/**
 * Java counterpart for TabGroupCollectionDataAndroid. This class provides access to the data of a
 * tab group collection stored in native.
 */
@NullMarked
@JNINamespace("tabs")
public class TabGroupCollectionData implements Destroyable {
    private long mNativePtr;

    private TabGroupCollectionData(long nativePtr) {
        mNativePtr = nativePtr;
    }

    @CalledByNative
    public static TabGroupCollectionData init(long nativePtr) {
        return new TabGroupCollectionData(nativePtr);
    }

    @Override
    public void destroy() {
        assert mNativePtr != 0;
        TabGroupCollectionDataJni.get().destroy(mNativePtr);
        mNativePtr = 0;
    }

    /** Returns the tab group ID. */
    public Token getTabGroupId() {
        assert mNativePtr != 0;
        return TabGroupCollectionDataJni.get().getTabGroupId(mNativePtr);
    }

    /** Returns the title of the tab group collection. */
    public String getTitle() {
        assert mNativePtr != 0;
        return TabGroupCollectionDataJni.get().getTitle(mNativePtr);
    }

    /** Returns the color of the tab group collection. */
    public @TabGroupColorId int getColor() {
        assert mNativePtr != 0;
        return TabGroupCollectionDataJni.get().getColor(mNativePtr);
    }

    /** Returns true if the tab group collection is collapsed. */
    public boolean isCollapsed() {
        assert mNativePtr != 0;
        return TabGroupCollectionDataJni.get().isCollapsed(mNativePtr);
    }

    @NativeMethods
    interface Natives {
        void destroy(long nativeTabGroupCollectionDataAndroid);

        @JniType("base::Token")
        Token getTabGroupId(long nativeTabGroupCollectionDataAndroid);

        @JniType("const std::u16string&")
        String getTitle(long nativeTabGroupCollectionDataAndroid);

        @JniType("tab_groups::TabGroupColorId")
        int getColor(long nativeTabGroupCollectionDataAndroid);

        boolean isCollapsed(long nativeTabGroupCollectionDataAndroid);
    }
}
