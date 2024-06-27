// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.net.Uri;
import android.os.Bundle;
import android.os.RemoteException;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.browser.trusted.TrustedWebActivityCallback;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.chrome.browser.dependency_injection.ChromeAppComponent;
import org.chromium.components.embedder_support.util.Origin;

import java.util.concurrent.TimeoutException;

/**
 * Tests TrustedWebActivityClient location delegation.
 *
 * <p>The control flow in these tests is a bit complicated since attempting to connect to a test
 * TrustedWebActivityService in the chrome_public_test results in a ClassLoader error (see
 * https://crbug.com/841178#c1). Therefore we must put the test TrustedWebActivityService in
 * chrome_public_test_support.
 *
 * <p>The general flow of these tests is as follows: 1. Call a method on TrustedWebActivityClient.
 * 2. This calls through to TestTrustedWebActivityService. 3. TestTrustedWebActivityService notify
 * the result with TrustedWebActivityCallback.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class TrustedWebActivityClientLocationDelegationTest {
    private static final Uri SCOPE = Uri.parse("https://www.example.com/notifications");
    private static final Origin ORIGIN = Origin.create(SCOPE);

    private static final String TEST_SUPPORT_PACKAGE = "org.chromium.chrome.tests.support";

    private static final String EXTRA_NEW_LOCATION_AVAILABLE_CALLBACK = "onNewLocationAvailable";
    private static final String EXTRA_NEW_LOCATION_ERROR_CALLBACK = "onNewLocationError";

    private TrustedWebActivityClient mClient;

    @Before
    public void setUp() throws TimeoutException, RemoteException {
        ChromeAppComponent component = ChromeApplicationImpl.getComponent();
        mClient = component.resolveTrustedWebActivityClient();

        // TestTrustedWebActivityService is in the test support apk.
        component.resolvePermissionManager().addDelegateApp(ORIGIN, TEST_SUPPORT_PACKAGE);
    }

    /** Tests {@link TrustedWebActivityClient#checkLocationPermission} */
    @Test
    @SmallTest
    public void testCheckLocationPermission() throws TimeoutException {
        CallbackHelper locationPermission = new CallbackHelper();

        TrustedWebActivityClient.PermissionCallback callback =
                (app, settingValue) -> locationPermission.notifyCalled();

        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> mClient.checkLocationPermission(SCOPE.toString(), callback));
        locationPermission.waitForOnly();
    }

    /** Tests {@link TrustedWebActivityClient#startListeningLocationUpdates} */
    @Test
    @SmallTest
    public void testStartListeningLocationUpdates() throws TimeoutException {
        CallbackHelper locationUpdate = new CallbackHelper();

        TrustedWebActivityCallback locationUpdateCallback =
                new TrustedWebActivityCallback() {
                    @Override
                    public void onExtraCallback(String callbackName, @Nullable Bundle bundle) {
                        if (TextUtils.equals(callbackName, EXTRA_NEW_LOCATION_AVAILABLE_CALLBACK)) {
                            Assert.assertNotNull(bundle);
                            Assert.assertTrue(bundle.containsKey("latitude"));
                            Assert.assertTrue(bundle.containsKey("longitude"));
                            Assert.assertTrue(bundle.containsKey("timeStamp"));
                            locationUpdate.notifyCalled();
                        }
                    }
                };
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () ->
                        mClient.startListeningLocationUpdates(
                                SCOPE.toString(),
                                /* highAccuracy= */ false,
                                locationUpdateCallback));
        locationUpdate.waitForOnly();
    }

    /** Tests {@link TrustedWebActivityClient#startListeningLocationUpdates} */
    @Test
    @SmallTest
    public void testStartLocationUpdatesNoConnection() throws TimeoutException {
        Origin otherOrigin = Origin.createOrThrow("https://www.websitewithouttwa.com/");
        CallbackHelper locationError = new CallbackHelper();

        TrustedWebActivityCallback locationUpdateCallback =
                new TrustedWebActivityCallback() {
                    @Override
                    public void onExtraCallback(String callbackName, @Nullable Bundle bundle) {
                        if (TextUtils.equals(callbackName, EXTRA_NEW_LOCATION_ERROR_CALLBACK)) {
                            Assert.assertNotNull(bundle);
                            Assert.assertTrue(bundle.containsKey("message"));
                            locationError.notifyCalled();
                        }
                    }
                };
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () ->
                        mClient.startListeningLocationUpdates(
                                otherOrigin.toString(),
                                /* highAccuracy= */ false,
                                locationUpdateCallback));
        locationError.waitForOnly();
    }
}
