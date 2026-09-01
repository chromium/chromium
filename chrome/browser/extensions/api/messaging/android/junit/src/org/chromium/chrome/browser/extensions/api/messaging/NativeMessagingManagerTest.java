// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.extensions.api.messaging;

import android.content.ComponentName;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.os.IBinder;
import android.os.RemoteException;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.extensions.api.messaging.NativeMessagingConnection.DisconnectionReason;
import org.chromium.chrome.browser.profiles.Profile;

/** Unit tests for {@link NativeMessagingManager} and {@link NativeMessagingConnection}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NativeMessagingManagerTest {
    private static final String TARGET_PACKAGE = "com.example.extensionreceiver";
    private static final String EXTENSION_ID = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

    // Used for addPort calls where no expected signing certificates are
    // provided so certificate checks are skipped.
    private static final byte[][] NO_CERTIFICATES = new byte[0][];

    // A mock 32-byte SHA-256 certificate fingerprint that matches the target package.
    private static final byte[] SIGNED_CERT_BYTES =
            new byte[] {
                (byte) 0x11, (byte) 0x22, (byte) 0x33, (byte) 0x44,
                (byte) 0x55, (byte) 0x66, (byte) 0x77, (byte) 0x88,
                (byte) 0x99, (byte) 0xaa, (byte) 0xbb, (byte) 0xcc,
                (byte) 0xdd, (byte) 0xee, (byte) 0xff, (byte) 0x00,
                (byte) 0x11, (byte) 0x22, (byte) 0x33, (byte) 0x44,
                (byte) 0x55, (byte) 0x66, (byte) 0x77, (byte) 0x88,
                (byte) 0x99, (byte) 0xaa, (byte) 0xbb, (byte) 0xcc,
                (byte) 0xdd, (byte) 0xee, (byte) 0xff, (byte) 0x00
            };

    // A mock 32-byte SHA-256 certificate fingerprint that does not match the target package.
    private static final byte[] OTHER_CERT_BYTES =
            new byte[] {
                (byte) 0x01, (byte) 0x02, (byte) 0x03, (byte) 0x04,
                (byte) 0x05, (byte) 0x06, (byte) 0x07, (byte) 0x08,
                (byte) 0x09, (byte) 0x0a, (byte) 0x0b, (byte) 0x0c,
                (byte) 0x0d, (byte) 0x0e, (byte) 0x0f, (byte) 0x10,
                (byte) 0x01, (byte) 0x02, (byte) 0x03, (byte) 0x04,
                (byte) 0x05, (byte) 0x06, (byte) 0x07, (byte) 0x08,
                (byte) 0x09, (byte) 0x0a, (byte) 0x0b, (byte) 0x0c,
                (byte) 0x0d, (byte) 0x0e, (byte) 0x0f, (byte) 0x10
            };

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private PackageManager mMockPackageManager;

    // Mocks the C++ bridge for NativeMessagingManager to prevent an UnsatisfiedLinkError.
    @Mock private NativeMessagingManager.Natives mNativeMessagingManagerJni;

    private TestContext mTestContext;
    private NativeMessagingManager mManager;

    private static class TestContext extends ContextWrapper {
        private ServiceConnection mServiceConnection;
        private Intent mBindIntent;
        private boolean mBindServiceResult = true;
        private PackageManager mPackageManager;

        public TestContext(Context base) {
            super(base);
        }

        public void setBindServiceResult(boolean result) {
            mBindServiceResult = result;
        }

        public void setPackageManager(PackageManager packageManager) {
            mPackageManager = packageManager;
        }

        @Override
        public PackageManager getPackageManager() {
            return mPackageManager != null ? mPackageManager : super.getPackageManager();
        }

        @Override
        public boolean bindService(Intent service, ServiceConnection conn, int flags) {
            mBindIntent = service;
            mServiceConnection = conn;
            return mBindServiceResult;
        }

        public void triggerServiceConnected(IBinder service) {
            mServiceConnection.onServiceConnected(
                    new ComponentName(TARGET_PACKAGE, "NativeMessageService"), service);
        }

        public void triggerServiceDisconnected() {
            mServiceConnection.onServiceDisconnected(
                    new ComponentName(TARGET_PACKAGE, "NativeMessageService"));
        }

        public void triggerNullBinding() {
            mServiceConnection.onNullBinding(
                    new ComponentName(TARGET_PACKAGE, "NativeMessageService"));
        }
    }

    @Before
    public void setUp() {
        NativeMessagingManagerJni.setInstanceForTesting(mNativeMessagingManagerJni);
        Mockito.when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        mTestContext = new TestContext(RuntimeEnvironment.application);
        mTestContext.setPackageManager(mMockPackageManager);
        ContextUtils.initApplicationContextForTests(mTestContext);
        mManager = NativeMessagingManager.getForProfile(mProfile);
    }

    @After
    public void tearDown() {
        if (mManager != null) {
            mManager.destroy();
        }
    }

    @Test
    public void testConnectAndDisconnectService() {
        String error =
                mManager.addPort(
                        TARGET_PACKAGE,
                        EXTENSION_ID,
                        /* isVerifiedExtension= */ true,
                        NO_CERTIFICATES,
                        new NativeMessageAndroidPort());
        Assert.assertNull(error);

        NativeMessagingConnection connection = mManager.getConnectionForTesting(TARGET_PACKAGE);
        Assert.assertNotNull(connection);
        Assert.assertTrue(connection.isBound());
        Assert.assertNull(connection.getServiceForTesting());

        IBrowserNativeMessageService fakeService =
                new IBrowserNativeMessageService.Stub() {
                    @Override
                    public void connectExtension(
                            String extensionId,
                            android.os.Bundle info,
                            IConnectExtensionCallback callback)
                            throws RemoteException {
                        callback.onError("Failed");
                    }
                };
        mTestContext.triggerServiceConnected(fakeService.asBinder());

        IBrowserNativeMessageService service = connection.getServiceForTesting();
        Assert.assertNotNull(service);

        // Simulate app service crash / disconnect.
        mTestContext.triggerServiceDisconnected();

        Assert.assertFalse(connection.isBound());
        Assert.assertNull(connection.getServiceForTesting());
        Assert.assertNull(mManager.getConnectionForTesting(TARGET_PACKAGE));
    }

    @Test
    public void testConnectNullBinding() {
        String error =
                mManager.addPort(
                        TARGET_PACKAGE,
                        EXTENSION_ID,
                        /* isVerifiedExtension= */ true,
                        NO_CERTIFICATES,
                        new NativeMessageAndroidPort());
        Assert.assertNull(error);

        NativeMessagingConnection connection = mManager.getConnectionForTesting(TARGET_PACKAGE);
        Assert.assertNotNull(connection);
        Assert.assertTrue(connection.isBound());

        var reasonWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Extensions.NativeMessaging.Android.DisconnectionReason",
                        DisconnectionReason.NULL_BINDING);
        var durationWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "Extensions.NativeMessaging.Android.UnexpectedDisconnectionDuration")
                        .build();

        mTestContext.triggerNullBinding();

        Assert.assertFalse(connection.isBound());
        Assert.assertNull(mManager.getConnectionForTesting(TARGET_PACKAGE));
        reasonWatcher.assertExpected();
        durationWatcher.assertExpected();
    }

    @Test
    public void testConnectAppDoesNotExist() {
        mTestContext.setBindServiceResult(false);

        String error =
                mManager.addPort(
                        "com.nonexistent.app",
                        EXTENSION_ID,
                        /* isVerifiedExtension= */ true,
                        NO_CERTIFICATES,
                        new NativeMessageAndroidPort());
        Assert.assertNotNull(error);
        Assert.assertEquals("Unable to connect to com.nonexistent.app.", error);
        Assert.assertNull(mManager.getConnectionForTesting("com.nonexistent.app"));
    }

    @Test
    public void testOnExtensionUnloaded() {
        String error =
                mManager.addPort(
                        TARGET_PACKAGE,
                        EXTENSION_ID,
                        /* isVerifiedExtension= */ true,
                        NO_CERTIFICATES,
                        new NativeMessageAndroidPort());
        Assert.assertNull(error);

        NativeMessagingConnection connection = mManager.getConnectionForTesting(TARGET_PACKAGE);
        Assert.assertNotNull(connection);
        Assert.assertNotNull(connection.getSessionForTesting(EXTENSION_ID));

        mManager.onExtensionUnloaded(EXTENSION_ID);

        // Check that unloading an extension means the connection has no more
        // active sub-sessions so it disconnects.
        Assert.assertNull(mManager.getConnectionForTesting(TARGET_PACKAGE));
    }

    @Test
    public void testCertificateValidationSuccess() {
        Mockito.when(
                        mMockPackageManager.hasSigningCertificate(
                                Mockito.eq(TARGET_PACKAGE),
                                Mockito.eq(SIGNED_CERT_BYTES),
                                Mockito.eq(PackageManager.CERT_INPUT_SHA256)))
                .thenReturn(true);

        String error =
                mManager.addPort(
                        TARGET_PACKAGE,
                        EXTENSION_ID,
                        /* isVerifiedExtension= */ true,
                        new byte[][] {SIGNED_CERT_BYTES},
                        new NativeMessageAndroidPort());
        Assert.assertNull(error);
        Assert.assertNotNull(mManager.getConnectionForTesting(TARGET_PACKAGE));
    }

    @Test
    public void testCertificateValidationFailure() {
        Mockito.when(
                        mMockPackageManager.hasSigningCertificate(
                                Mockito.eq(TARGET_PACKAGE),
                                Mockito.eq(OTHER_CERT_BYTES),
                                Mockito.eq(PackageManager.CERT_INPUT_SHA256)))
                .thenReturn(false);

        String error =
                mManager.addPort(
                        TARGET_PACKAGE,
                        EXTENSION_ID,
                        /* isVerifiedExtension= */ true,
                        new byte[][] {OTHER_CERT_BYTES},
                        new NativeMessageAndroidPort());
        Assert.assertEquals("Unable to connect to " + TARGET_PACKAGE + ".", error);
        Assert.assertNull(mManager.getConnectionForTesting(TARGET_PACKAGE));
    }

    @Test
    public void testCertificateValidationMultipleCertificatesWithOneMatching() {
        Mockito.when(
                        mMockPackageManager.hasSigningCertificate(
                                Mockito.eq(TARGET_PACKAGE),
                                Mockito.eq(OTHER_CERT_BYTES),
                                Mockito.eq(PackageManager.CERT_INPUT_SHA256)))
                .thenReturn(false);
        Mockito.when(
                        mMockPackageManager.hasSigningCertificate(
                                Mockito.eq(TARGET_PACKAGE),
                                Mockito.eq(SIGNED_CERT_BYTES),
                                Mockito.eq(PackageManager.CERT_INPUT_SHA256)))
                .thenReturn(true);

        String error =
                mManager.addPort(
                        TARGET_PACKAGE,
                        EXTENSION_ID,
                        /* isVerifiedExtension= */ true,
                        new byte[][] {OTHER_CERT_BYTES, SIGNED_CERT_BYTES},
                        new NativeMessageAndroidPort());
        Assert.assertNull(error);
        Assert.assertNotNull(mManager.getConnectionForTesting(TARGET_PACKAGE));
    }
}
