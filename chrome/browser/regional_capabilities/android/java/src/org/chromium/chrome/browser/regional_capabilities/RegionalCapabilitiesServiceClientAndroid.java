// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.regional_capabilities;

import android.text.TextUtils;

import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.Promise;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.search_engines.SearchEngineChoiceService;

/**
 * Singleton responsible for communicating with device APIs to expose device-level properties that
 * are relevant for determining regional capabilities.
 *
 * <p>The object is a singleton rather than being profile-scoped as device properties apply to all
 * profiles, it also allows an instance to be created before the native is initialized.
 */
@JNINamespace("regional_capabilities")
@NullMarked
public class RegionalCapabilitiesServiceClientAndroid {
    private static @Nullable RegionalCapabilitiesServiceClientAndroid sInstance;

    /** Returns the instance of the singleton. Creates the instance if needed. */
    @MainThread
    public static RegionalCapabilitiesServiceClientAndroid getInstance() {
        ThreadUtils.checkUiThread();
        if (sInstance == null) {
            sInstance = new RegionalCapabilitiesServiceClientAndroid();
        }
        return sInstance;
    }

    /** Overrides the instance of the singleton for tests. */
    @MainThread
    @VisibleForTesting
    public static void setInstanceForTests(
            @Nullable RegionalCapabilitiesServiceClientAndroid instance) {
        ThreadUtils.checkUiThread();
        sInstance = instance;
        if (instance != null) {
            ResettersForTesting.register(() -> setInstanceForTests(null)); // IN-TEST
        }
    }

    @VisibleForTesting
    @MainThread
    public RegionalCapabilitiesServiceClientAndroid() {
        ThreadUtils.checkUiThread();
    }

    private void requestDeviceCountryInternal(Callback<@Nullable String> deviceCountryCallback) {
        // TODO(crbug.com/388792357): Don't call into `SearchEngineChoiceService`, manage a
        // dedicated access to the device country instead. This will allow the other service
        // to not be created most of the time.
        Promise<String> deviceCountryPromise =
                SearchEngineChoiceService.getInstance().getDeviceCountry();
        Runnable provideDeviceCountry =
                () -> {
                    final @Nullable String deviceCountry;
                    if (deviceCountryPromise.isFulfilled()) {
                        deviceCountry = deviceCountryPromise.getResult();
                        assert !TextUtils.isEmpty(deviceCountry);
                    } else {
                        deviceCountry = null;
                    }

                    deviceCountryCallback.onResult(deviceCountry);
                };

        // Try to resolve the request synchronously when possible, so the prefs can be updated and
        // the value can be applied right away instead of at the next startup.
        if (deviceCountryPromise.isPending()) {
            deviceCountryPromise.andFinally(provideDeviceCountry);
        } else {
            provideDeviceCountry.run();
        }
    }

    @CalledByNative
    private static void requestDeviceCountry(long ptrToNativeCallback) {
        ThreadUtils.checkUiThread();
        getInstance()
                .requestDeviceCountryInternal(
                        deviceCountry ->
                                RegionalCapabilitiesServiceClientAndroidJni.get()
                                        .processDeviceCountryResponse(
                                                ptrToNativeCallback, deviceCountry));
    }

    @NativeMethods
    public interface Natives {
        void processDeviceCountryResponse(long ptrToNativeCallback, @Nullable String deviceCountry);
    }
}
