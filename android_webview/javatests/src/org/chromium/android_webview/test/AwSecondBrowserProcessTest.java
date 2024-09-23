// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import android.app.ActivityManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.os.Parcel;
import android.os.Process;
import android.os.RemoteException;

import androidx.test.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.common.services.ServiceHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotRevive;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Tests to ensure that it is impossible to launch two browser processes within the same
 * application. Chromium is not designed for that, and attempting to do that can cause data files
 * corruption.
 */
@RunWith(Parameterized.class)
@OnlyRunIn(EITHER_PROCESS) // These tests don't use the renderer process
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwSecondBrowserProcessTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    public AwSecondBrowserProcessTest(AwSettingsMutation param) {
        mActivityTestRule =
                new AwActivityTestRule(param.getMutation()) {
                    @Override
                    public boolean needsBrowserProcessStarted() {
                        return false;
                    }
                };
    }

    private CountDownLatch mSecondBrowserProcessLatch;
    private int mSecondBrowserServicePid;

    @After
    public void tearDown() {
        stopSecondBrowserProcess(false);
    }

    /*
     * @LargeTest
     * @Feature({"AndroidWebView"})
     * We can't test that creating second browser
     * process succeeds either, because in debug it will crash due to an assert
     * in the SQL DB code.
     */
    @Test
    @DisabledTest(message = "crbug.com/582146")
    @DoNotRevive(reason = "Second browser process currently allowed. See crbug.com/558377.")
    public void testCreatingSecondBrowserProcessFails() throws Throwable {
        startSecondBrowserProcess();
        Assert.assertFalse(tryStartingBrowserProcess());
    }

    /*
     * @LargeTest
     * @Feature({"AndroidWebView"})
     */
    @Test
    @DisabledTest(message = "crbug.com/582146")
    @DoNotRevive(reason = "Second browser process currently allowed. See crbug.com/558377.")
    public void testLockCleanupOnProcessShutdown() throws Throwable {
        startSecondBrowserProcess();
        Assert.assertFalse(tryStartingBrowserProcess());
        stopSecondBrowserProcess(true);
        Assert.assertTrue(tryStartingBrowserProcess());
    }

    private final ServiceConnection mConnection =
            new ServiceConnection() {
                @Override
                public void onServiceConnected(ComponentName className, IBinder service) {
                    Parcel result = Parcel.obtain();
                    try {
                        Assert.assertTrue(
                                service.transact(
                                        SecondBrowserProcess.CODE_START,
                                        Parcel.obtain(),
                                        result,
                                        0));
                    } catch (RemoteException e) {
                        Assert.fail("RemoteException: " + e);
                    }
                    result.readException();
                    mSecondBrowserServicePid = result.readInt();
                    Assert.assertTrue(mSecondBrowserServicePid > 0);
                    mSecondBrowserProcessLatch.countDown();
                }

                @Override
                public void onServiceDisconnected(ComponentName className) {}
            };

    private void startSecondBrowserProcess() throws Exception {
        Context context = mActivityTestRule.getActivity();
        Intent intent = new Intent(context, SecondBrowserProcess.class);
        mSecondBrowserProcessLatch = new CountDownLatch(1);
        Assert.assertNotNull(context.startService(intent));
        Assert.assertTrue(ServiceHelper.bindService(context, intent, mConnection, 0));
        Assert.assertTrue(
                mSecondBrowserProcessLatch.await(
                        AwActivityTestRule.SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        mSecondBrowserProcessLatch = null;
    }

    private void stopSecondBrowserProcess(boolean sync) {
        if (mSecondBrowserServicePid <= 0) return;
        Assert.assertTrue(isSecondBrowserServiceRunning());
        // Note that using killProcess ensures that the service record gets removed
        // from ActivityManager after the process has actually died. While using
        // Context.stopService would result in the opposite outcome.
        Process.killProcess(mSecondBrowserServicePid);
        if (sync) {
            AwActivityTestRule.pollInstrumentationThread(() -> !isSecondBrowserServiceRunning());
        }
        mSecondBrowserServicePid = 0;
    }

    private boolean tryStartingBrowserProcess() {
        final Boolean success[] = new Boolean[1];
        // The activity must be launched in order for proper webview statics to be setup.
        mActivityTestRule.getActivity();
        // runOnMainSync does not catch RuntimeExceptions, they just terminate the test.
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            try {
                                AwTestContainerView.installDrawFnFunctionTable(
                                        /* useVulkan= */ false);
                                AwBrowserProcess.start();
                                success[0] = true;
                            } catch (RuntimeException e) {
                                success[0] = false;
                            }
                        });
        Assert.assertNotNull(success[0]);
        return success[0];
    }

    // Note that both onServiceDisconnected and Binder.DeathRecipient fire prematurely for our
    // purpose. We need to ensure that the service process has actually terminated, releasing all
    // the locks. The only reliable way to do that is to scan the process list.
    private boolean isSecondBrowserServiceRunning() {
        ActivityManager activityManager =
                (ActivityManager)
                        mActivityTestRule.getActivity().getSystemService(Context.ACTIVITY_SERVICE);
        for (ActivityManager.RunningServiceInfo si : activityManager.getRunningServices(65536)) {
            if (si.pid == mSecondBrowserServicePid) return true;
        }
        return false;
    }
}
