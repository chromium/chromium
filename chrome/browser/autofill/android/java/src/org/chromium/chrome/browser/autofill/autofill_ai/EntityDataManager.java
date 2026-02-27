// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.autofill_ai.AutofillAiOptInStatus;
import org.chromium.components.autofill.autofill_ai.EntityInstance;
import org.chromium.components.autofill.autofill_ai.EntityInstanceWithLabels;
import org.chromium.components.autofill.autofill_ai.EntityType;

import java.util.ArrayList;
import java.util.List;

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
    /** Observer of EntityDataManager events. */
    public interface EntityDataManagerObserver {
        /** Called when the entity instances are changed. */
        void onEntityInstancesChanged();
    }

    private final List<EntityDataManagerObserver> mDataObservers = new ArrayList<>();
    private long mNativeEntityDataManagerAndroid;

    EntityDataManager(Profile profile) {
        mNativeEntityDataManagerAndroid = EntityDataManagerJni.get().init(this, profile);
    }

    @Override
    public void destroy() {
        EntityDataManagerJni.get().destroy(mNativeEntityDataManagerAndroid);
        mNativeEntityDataManagerAndroid = 0;
    }

    /** Registers an EntityDataManagerObserver. */
    public void registerDataObserver(EntityDataManagerObserver observer) {
        ThreadUtils.assertOnUiThread();
        assert !mDataObservers.contains(observer);
        mDataObservers.add(observer);
    }

    /** Unregisters the provided observer. */
    public void unregisterDataObserver(EntityDataManagerObserver observer) {
        ThreadUtils.assertOnUiThread();
        assert mDataObservers.contains(observer);
        mDataObservers.remove(observer);
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

    /**
     * Returns the entity instance represented by the given GUID.
     *
     * @param guid The GUID of the entity instance to return.
     * @return The entity instance.
     */
    public @Nullable EntityInstance getEntityInstance(String guid) {
        ThreadUtils.assertOnUiThread();
        return EntityDataManagerJni.get().getEntityInstance(mNativeEntityDataManagerAndroid, guid);
    }

    /** Saves or update an entity. */
    public void addOrUpdateEntityInstance(EntityInstance entity) {
        ThreadUtils.assertOnUiThread();
        EntityDataManagerJni.get()
                .addOrUpdateEntityInstance(mNativeEntityDataManagerAndroid, entity);
    }

    /**
     * Returns a list of `EntityInstanceWithLabels`. Entities of the same type are grouped together
     * in the list. This list is used by the management page to show users all entities they have
     * stored, offering an entry point for edition and deletion.
     */
    public List<EntityInstanceWithLabels> getEntitiesWithLabels() {
        ThreadUtils.assertOnUiThread();
        return EntityDataManagerJni.get().getEntitiesWithLabels(mNativeEntityDataManagerAndroid);
    }

    public List<EntityType> getWritableEntityTypes() {
        ThreadUtils.assertOnUiThread();
        return EntityDataManagerJni.get().getWritableEntityTypes(mNativeEntityDataManagerAndroid);
    }

    public List<EntityType> getSortedEntityTypesForListDisplay() {
        ThreadUtils.assertOnUiThread();
        return EntityDataManagerJni.get()
                .getSortedEntityTypesForListDisplay(mNativeEntityDataManagerAndroid);
    }

    /** Called by C++ when there is a change in the instances. */
    @CalledByNative
    public void onEntityInstancesChanged() {
        ThreadUtils.assertOnUiThread();
        for (EntityDataManagerObserver observer : mDataObservers) {
            observer.onEntityInstancesChanged();
        }
    }

    /** Returns whether the user is eligible for Autofill AI. */
    public boolean isEligibleToAutofillAi() {
        ThreadUtils.assertOnUiThread();
        return EntityDataManagerJni.get().isEligibleToAutofillAi(mNativeEntityDataManagerAndroid);
    }

    /** Returns the opt-in status for Autofill AI. */
    public boolean getAutofillAiOptInStatus() {
        ThreadUtils.assertOnUiThread();
        return EntityDataManagerJni.get().getAutofillAiOptInStatus(mNativeEntityDataManagerAndroid);
    }

    /**
     * Sets the opt-in status for Autofill AI.
     *
     * @param optInStatus The new opt-in status.
     * @return Whether the status was successfully set.
     */
    public boolean setAutofillAiOptInStatus(@AutofillAiOptInStatus int optInStatus) {
        ThreadUtils.assertOnUiThread();
        return EntityDataManagerJni.get()
                .setAutofillAiOptInStatus(mNativeEntityDataManagerAndroid, optInStatus);
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        long init(EntityDataManager self, @JniType("Profile*") Profile profile);

        void destroy(long nativeEntityDataManagerAndroid);

        boolean isEligibleToAutofillAi(long nativeEntityDataManagerAndroid);

        boolean getAutofillAiOptInStatus(long nativeEntityDataManagerAndroid);

        boolean setAutofillAiOptInStatus(
                long nativeEntityDataManagerAndroid,
                @JniType("autofill::AutofillAiOptInStatus") @AutofillAiOptInStatus int optInStatus);

        void removeEntityInstance(
                long nativeEntityDataManagerAndroid, @JniType("std::string") String guid);

        @Nullable EntityInstance getEntityInstance(
                long nativeEntityDataManagerAndroid, @JniType("std::string") String guid);

        void addOrUpdateEntityInstance(long nativeEntityDataManagerAndroid, EntityInstance entity);

        @JniType("std::vector<EntityInstanceWithLabels>")
        List<EntityInstanceWithLabels> getEntitiesWithLabels(long nativeEntityDataManagerAndroid);

        @JniType("std::vector<autofill::EntityTypeAndroid>")
        List<EntityType> getWritableEntityTypes(long nativeEntityDataManagerAndroid);

        @JniType("std::vector<autofill::EntityTypeAndroid>")
        List<EntityType> getSortedEntityTypesForListDisplay(long nativeEntityDataManagerAndroid);
    }
}
