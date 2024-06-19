// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.app.ActivityManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.StrictMode;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.BuildConfig;

import java.io.BufferedReader;
import java.io.FileReader;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Exposes system related information about the current device. */
@JNINamespace("base::android")
public class SysUtils {
    // A device reporting strictly more total memory in megabytes cannot be considered 'low-end'.
    private static final int LOW_MEMORY_DEVICE_THRESHOLD_MB = 1024;
    private static final String TAG = "SysUtils";

    private static Boolean sLowEndDevice;
    private static Integer sAmountOfPhysicalMemoryKB;

    private SysUtils() {}

    /**
     * Return the amount of physical memory on this device in kilobytes.
     * @return Amount of physical memory in kilobytes, or 0 if there was
     *         an error trying to access the information.
     */
    private static int detectAmountOfPhysicalMemoryKB() {
        // Extract total memory RAM size by parsing /proc/meminfo, note that
        // this is exactly what the implementation of sysconf(_SC_PHYS_PAGES)
        // does. However, it can't be called because this method must be
        // usable before any native code is loaded.

        // An alternative is to use ActivityManager.getMemoryInfo(), but this
        // requires a valid ActivityManager handle, which can only come from
        // a valid Context object, which itself cannot be retrieved
        // during early startup, where this method is called. And making it
        // an explicit parameter here makes all call paths _much_ more
        // complicated.

        Pattern pattern = Pattern.compile("^MemTotal:\\s+([0-9]+) kB$");
        // Synchronously reading files in /proc in the UI thread is safe.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try (FileReader fileReader = new FileReader("/proc/meminfo")) {
            try (BufferedReader reader = new BufferedReader(fileReader)) {
                for (; ; ) {
                    String line = reader.readLine();
                    if (line == null) {
                        Log.w(TAG, "/proc/meminfo lacks a MemTotal entry?");
                        break;
                    }
                    Matcher m = pattern.matcher(line);
                    if (!m.find()) continue;
                    int totalMemoryKB = Integer.parseInt(m.group(1));
                    if (totalMemoryKB <= 1024) {
                        Log.w(TAG, "Invalid /proc/meminfo total size in kB: " + m.group(1));
                        break;
                    }
                    return totalMemoryKB;
                }
            }
        } catch (Exception e) {
            Log.w(TAG, "Cannot get total physical size from /proc/meminfo", e);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
        return 0;
    }

    /**
     * @return Whether or not this device should be considered a low end device.
     */
    @CalledByNative
    public static boolean isLowEndDevice() {
        // Do not cache in tests since command-line flags can change.
        if (sLowEndDevice == null || BuildConfig.IS_FOR_TEST) {
            sLowEndDevice = detectLowEndDevice();
        }
        return sLowEndDevice;
    }

    /**
     * @return amount of physical ram detected in KB, or 0 if detection failed.
     */
    @CalledByNative
    public static int amountOfPhysicalMemoryKB() {
        if (sAmountOfPhysicalMemoryKB == null) {
            sAmountOfPhysicalMemoryKB = detectAmountOfPhysicalMemoryKB();
        }
        return sAmountOfPhysicalMemoryKB;
    }

    /**
     * @return Whether or not the system has low available memory.
     */
    @CalledByNative
    public static boolean isCurrentlyLowMemory() {
        ActivityManager am =
                (ActivityManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.ACTIVITY_SERVICE);
        ActivityManager.MemoryInfo info = new ActivityManager.MemoryInfo();
        try {
            am.getMemoryInfo(info);
            return info.lowMemory;
        } catch (Exception e) {
            // Occurs on Redmi devices when called from isolated processes.
            // https://crbug.com/1480655
            // And on the devices in https://crbug.com/347207010
            return false;
        }
    }

    public static boolean hasCamera(final Context context) {
        final PackageManager pm = context.getPackageManager();
        return pm.hasSystemFeature(PackageManager.FEATURE_CAMERA_ANY);
    }

    private static boolean detectLowEndDevice() {
        assert CommandLine.isInitialized();
        if (CommandLine.getInstance().hasSwitch(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)) {
            return true;
        }
        if (CommandLine.getInstance().hasSwitch(BaseSwitches.DISABLE_LOW_END_DEVICE_MODE)) {
            return false;
        }

        // If this logic changes, update the comments above base::SysUtils::IsLowEndDevice.
        int physicalMemoryKb = amountOfPhysicalMemoryKB();
        boolean isLowEnd;
        if (physicalMemoryKb <= 0) {
            isLowEnd = false;
        } else {
            isLowEnd = physicalMemoryKb / 1024 <= LOW_MEMORY_DEVICE_THRESHOLD_MB;
        }

        return isLowEnd;
    }

    /**
     * Creates a new trace event to log the number of minor / major page faults, if tracing is
     * enabled.
     */
    public static void logPageFaultCountToTracing() {
        SysUtilsJni.get().logPageFaultCountToTracing();
    }

    @NativeMethods
    interface Natives {
        void logPageFaultCountToTracing();
    }
}
