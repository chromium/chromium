// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Android wrapper of the EntityDataManager which provides access from the Java layer.
 *
 * <p>Only usable from the UI thread.
 *
 * <p>See components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h for more
 * details.
 */
@NullMarked
@JNINamespace("autofill")
public class EntityDataManager implements Destroyable {

    private long mNativeEntityDataManagerAndroid;

    EntityDataManager(Profile profile) {
        mNativeEntityDataManagerAndroid = EntityDataManagerJni.get().init(this, profile);
    }

    @Override
    public void destroy() {
        EntityDataManagerJni.get().destroy(mNativeEntityDataManagerAndroid);
        mNativeEntityDataManagerAndroid = 0;
    }

    /**
     * Removes the entity instance represented by the given GUID.
     *
     * @param guid The GUID of the entity instance to remove.
     */
    public void removeEntityInstance(String guid) {
        ThreadUtils.assertOnUiThread();
        EntityDataManagerJni.get().removeEntityInstance(mNativeEntityDataManagerAndroid, guid);
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        long init(EntityDataManager self, @JniType("Profile*") Profile profile);

        void destroy(long nativeEntityDataManagerAndroid);

        void removeEntityInstance(
                long nativeEntityDataManagerAndroid, @JniType("std::string") String guid);
    }
}
