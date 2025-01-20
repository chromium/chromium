// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.regional_capabilities.test.util;

import static org.mockito.Mockito.doReturn;

import android.os.Handler;
import android.os.Looper;

import org.jni_zero.CalledByNative;
import org.mockito.Mockito;

import org.chromium.base.Promise;
import org.chromium.base.test.util.LooperUtils;
import org.chromium.chrome.browser.regional_capabilities.RegionalCapabilitiesServiceClientAndroid;
import org.chromium.components.search_engines.SearchEngineChoiceService;
import org.chromium.components.search_engines.SearchEngineCountryDelegate;

import java.lang.reflect.InvocationTargetException;
import java.util.concurrent.atomic.AtomicBoolean;

final class RegionalCapabilitiesServiceTestUtil {
    private final SearchEngineCountryDelegate mMockDelegate;
    private final Promise<String> mDeviceCountry = new Promise<>();

    /** Stubs {@link RegionalCapabilitiesServiceClientAndroid} for native tests. */
    @CalledByNative
    public RegionalCapabilitiesServiceTestUtil() {
        // Currently `RegionalCapabilitiesService` defers to `SearchEngineChoiceService`, so we
        // need to instantiate both services.
        mMockDelegate = Mockito.mock(SearchEngineCountryDelegate.class);
        doReturn(mDeviceCountry).when(mMockDelegate).getDeviceCountry();
        SearchEngineChoiceService.setInstanceForTests(new SearchEngineChoiceService(mMockDelegate));
        RegionalCapabilitiesServiceClientAndroid.setInstanceForTests(
                new RegionalCapabilitiesServiceClientAndroid());
    }

    /** Restores the global state after the test completes. */
    @CalledByNative
    public void destroy() {
        RegionalCapabilitiesServiceClientAndroid.setInstanceForTests(null);
        SearchEngineChoiceService.setInstanceForTests(null);
    }

    /**
     * Fulfills the promise returned by `SearchEngineChoiceCountry` to simulate a response from the
     * device APIs.
     *
     * @param deviceCountry the result of the device country request.
     */
    @CalledByNative
    public void returnDeviceCountry(String deviceCountry) {
        mDeviceCountry.fulfill(deviceCountry);
        // `Promise` posts callback tasks on Android Looper which is not integrated with native
        // RunLoop in NativeTest. Run these tasks synchronously now.
        // TODO(crbug.com/40723709): remove this hack once Promise uses PostTask.
        runLooperTasks();
    }

    /**
     * Rejects the promise returned by `SearchEngineChoiceCountry` to simulate a failure of the
     * request to the device APIs.
     */
    @CalledByNative
    public void triggerDeviceCountryFailure() {
        mDeviceCountry.reject();
        // `Promise` posts callback tasks on Android Looper which is not integrated with native
        // RunLoop in NativeTest. Run these tasks synchronously now.
        // TODO(crbug.com/40723709): remove this hack once Promise uses PostTask.
        runLooperTasks();
    }

    /**
     * Runs all tasks that are currently posted on the {@link Looper}'s message queue on the current
     * thread.
     */
    private static void runLooperTasks() {
        AtomicBoolean called = new AtomicBoolean(false);
        new Handler(Looper.myLooper())
                .post(
                        () -> {
                            called.set(true);
                        });

        do {
            try {
                LooperUtils.runSingleNestedLooperTask();
            } catch (IllegalArgumentException
                    | IllegalAccessException
                    | SecurityException
                    | InvocationTargetException e) {
                throw new RuntimeException(e);
            }
        } while (!called.get());
    }
}
