// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.chromium.build.annotations.NullMarked;

/** Convenience static methods to access {@link BaseFeatureMap}. */
@NullMarked
public class BaseFeatureList {
    private BaseFeatureList() {}

    public static final MutableFlagWithSafeDefault sBackgroundNotPerceptibleBinding =
            new MutableFlagWithSafeDefault(
                    BaseFeatureMap.getInstance(),
                    BaseFeatures.BACKGROUND_NOT_PERCEPTIBLE_BINDING,
                    false);

    public static final MutableFlagWithSafeDefault sEffectiveBindingState =
            new MutableFlagWithSafeDefault(
                    BaseFeatureMap.getInstance(), BaseFeatures.EFFECTIVE_BINDING_STATE, false);

    public static final MutableFlagWithSafeDefault sUpdateStateBeforeUnbinding =
            new MutableFlagWithSafeDefault(
                    BaseFeatureMap.getInstance(),
                    BaseFeatures.UPDATE_STATE_BEFORE_UNBINDING,
                    false);

    public static final MutableFlagWithSafeDefault sUseSharedRebindServiceConnection =
            new MutableFlagWithSafeDefault(
                    BaseFeatureMap.getInstance(),
                    BaseFeatures.USE_SHARED_REBIND_SERVICE_CONNECTION,
                    false);

    public static final MutableIntParamWithSafeDefault sMaxDeferredSharedRebindServiceConnection =
            sUseSharedRebindServiceConnection.newIntParam("max-deferred-bindings", 10);
}
