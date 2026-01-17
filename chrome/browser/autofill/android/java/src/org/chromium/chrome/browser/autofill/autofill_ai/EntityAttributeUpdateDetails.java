// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

/** Represents information of an entity attribute displayed in the save/update prompt. */
@JNINamespace("autofill")
@NullMarked
public class EntityAttributeUpdateDetails {
    private final String mAttributeName;
    private final String mAttributeValue;
    private final String mOldAttributeValue;
    public final @EntityAttributeUpdateType int mUpdateType;

    @CalledByNative
    public EntityAttributeUpdateDetails(
            @JniType("std::u16string") String attributeName,
            @JniType("std::u16string") String attributeValue,
            @JniType("std::u16string") String oldAttributeValue,
            @EntityAttributeUpdateType int updateType) {
        this.mAttributeName = attributeName;
        this.mAttributeValue = attributeValue;
        this.mOldAttributeValue = oldAttributeValue;
        this.mUpdateType = updateType;
    }

    @CalledByNative
    public @JniType("std::u16string") String getAttributeName() {
        return mAttributeName;
    }

    @CalledByNative
    public @JniType("std::u16string") String getAttributeValue() {
        return mAttributeValue;
    }

    @CalledByNative
    public @JniType("std::u16string") String getOldAttributeValue() {
        return mOldAttributeValue;
    }

    @CalledByNative
    public @EntityAttributeUpdateType int getUpdateType() {
        return mUpdateType;
    }
}
