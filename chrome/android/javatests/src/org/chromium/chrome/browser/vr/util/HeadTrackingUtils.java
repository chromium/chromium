// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.util;

import android.content.ComponentName;
import android.content.Intent;
import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.runner.Description;

import org.chromium.chrome.browser.vr.TestVrShellDelegate;
import org.chromium.chrome.browser.vr.rules.HeadTrackingMode;
import org.chromium.chrome.browser.vr.rules.HeadTrackingMode.SupportedMode;
import org.chromium.chrome.browser.vr.rules.VrTestRule;

import java.util.Arrays;

/**
 * Utility class for interacting with the VrCore head tracking service, which allows fake head
 * poses to be submitted instead of using actual sensor data.
 *
 * Requires that either the O2 rendering path is enabled or EnableVrCoreHeadTracking is set to true
 * in the shared prefs file in order to actually work.
 */
public class HeadTrackingUtils {
    private static final ComponentName HEAD_TRACKING_COMPONENT = new ComponentName(
            "com.google.vr.vrcore", "com.google.vr.vrcore.tracking.HeadTrackingService");

    private static final String ACTION_SET_TRACKER_TYPE = "com.google.vr.vrcore.SET_TRACKER_TYPE";
    private static final String ACTION_SET_FAKE_TRACKER_MODE =
            "com.google.vr.vrcore.SET_FAKE_TRACKER_MODE";
    private static final String ACTION_SET_FAKE_TRACKER_POSE =
            "com.google.vr.vrcore.SET_FAKE_TRACKER_POSE";
    private static final String EXTRA_FAKE_TRACKER_MODE = "com.google.vr.vrcore.FAKE_TRACKER_MODE";
    private static final String EXTRA_FAKE_TRACKER_POSE = "com.google.vr.vrcore.FAKE_TRACKER_POSE";
    private static final String EXTRA_TRACKER_TYPE = "com.google.vr.vrcore.TRACKER_TYPE";

    private static final int HEAD_TRACKING_APPLICATION_DELAY_MS = 500;

    /**
     * Class for holding data necessary to set the head tracking service's head pose to an
     * arbitarary, static value. Contains either a quaternion or set of rotation Euler angles
     * describing the direction to look in and an optional position in room space if 6DOF is
     * supported.
     */
    public static class FakePose {
        private float[] mQuaternion;
        private float[] mRotationEulerAngles;
        private float[] mRoomSpacePosition;

        /**
         * Sets the head pose using a quaternion.
         *
         * @param x The X component of the quaternion.
         * @param y The Y component of the quaternion.
         * @param z The Z component of the quaternion.
         * @param w The W component of the quaternion.
         * @return The updated FakePose instance.
         */
        public FakePose setQuaternion(float x, float y, float z, float w) {
            mQuaternion = new float[] {x, y, z, w};
            mRotationEulerAngles = null;
            return this;
        }

        /**
         * Sets the head pose using Euler angles.
         *
         * @param rollDeg The head pose's roll component in degrees.
         * @param pitchDeg The head pose's pitch component in degrees.
         * @param yawDeg The head pose's yaw component in degrees.
         * @return The updated FakePose instance.
         */
        public FakePose setRotationEulerAngles(float rollDeg, float pitchDeg, float yawDeg) {
            mRotationEulerAngles = new float[] {rollDeg, pitchDeg, yawDeg};
            mQuaternion = null;
            return this;
        }

        /**
         * Sets the head pose's positional coordinates in room space.
         *
         * @param x The X position of the head pose in meters.
         * @param y The Y position of the head pose in meters.
         * @param z The Z position of the head pose in meters.
         * @return The updated FakePose instance.
         */
        public FakePose setRoomSpacePosition(float x, float y, float z) {
            mRoomSpacePosition = new float[] {x, y, z};
            return this;
        }

        /**
         * Clears any previously set room space head pose position.
         *
         * @return The updated FakePose instance.
         */
        public FakePose clearRoomSpacePosition() {
            mRoomSpacePosition = null;
            return this;
        }

        /**
         * Formats the FakePose's stored data into a format usable as an extra in an Intent.
         *
         * @return A float array containing all the data of the FakePose instance.
         */
        public float[] getDataForExtra() {
            if (mQuaternion == null && mRotationEulerAngles == null) {
                throw new IllegalArgumentException(
                        "Tried to get FakePose data without setting either quaternion/angle data");
            }
            float[] orientationArray =
                    mRotationEulerAngles == null ? mQuaternion : mRotationEulerAngles;
            if (mRoomSpacePosition == null) return orientationArray;
            float[] combinedArray = Arrays.copyOf(
                    orientationArray, orientationArray.length + mRoomSpacePosition.length);
            for (int i = 0; i < mRoomSpacePosition.length; i++) {
                combinedArray[i + orientationArray.length] = mRoomSpacePosition[i];
            }
            return combinedArray;
        }
    }

    /**
     * Checks for the presence of a HeadTrackingMode annotation, and if found, sets the tracking
     * mode to the specified value. If no annotation is found, the tracking mode is left at whatever
     * the existing value is.
     *
     * @param rule The VrTestRule used by the current test case.
     * @param desc The JUnit4 Description for the current test case.
     */
    public static void checkForAndApplyHeadTrackingModeAnnotation(
            VrTestRule rule, Description desc) {
        // This is even more broken on standalone devices, and can't be disabled at the shared
        // preference level, so no-op here.
        if (TestVrShellDelegate.isOnStandalone()) return;
        // Check if the test has a HeadTrackingMode annotation
        HeadTrackingMode annotation = desc.getAnnotation(HeadTrackingMode.class);
        if (annotation == null) return;
        applyHeadTrackingModeInternal(rule, annotation.value());
    }

    /**
     * Sets the tracker type to the given mode and waits long enough to safely assume that the
     * service has started.
     *
     * @param rule The VrTestRule used by the current test case.
     * @param mode The HeadTrackingMode.SupportedMode value to set the fake head tracker mode to.
     */
    public static void applyHeadTrackingMode(VrTestRule rule, @SupportedMode int mode) {
        applyHeadTrackingModeInternal(rule, mode);
        // TODO(bsheedy): Remove this sleep if the head tracking service ever exposes a way to be
        // notified when a setting has been applied.
        SystemClock.sleep(HEAD_TRACKING_APPLICATION_DELAY_MS);
    }

    /**
     * Sets the head pose to the pose described by the given FakePose and waits long enough to
     * safely assume that the pose has taken effect.
     *
     * @param rule The VrTestRule used by the current test case.
     * @param pose The FakePose instance containing the pose data that will be sent to the head
     *        tracking service.
     */
    public static void setHeadPose(VrTestRule rule, FakePose pose) {
        restartHeadTrackingServiceIfNecessary(rule);
        // Set the head pose to the given value
        Intent poseIntent = new Intent(ACTION_SET_FAKE_TRACKER_POSE);
        poseIntent.putExtra(EXTRA_FAKE_TRACKER_POSE, pose.getDataForExtra());
        poseIntent.setComponent(HEAD_TRACKING_COMPONENT);
        Assert.assertTrue("Could not set head pose",
                InstrumentationRegistry.getContext().startService(poseIntent) != null);
        rule.setTrackerDirty();
        // TODO(bsheedy): Remove this sleep. Could either expose poses up to Java and wait until
        // we receive a pose that's the same as the one we set or see if the head tracking service
        // adds the requested functionality of sending a notification when it's done applying
        // settings.
        SystemClock.sleep(HEAD_TRACKING_APPLICATION_DELAY_MS);
    }

    /**
     * Reverts the tracking type back to values that a regular user would have (using real sensor
     * data).
     *
     * Only meant to be called by rules after a test has run. Reseting the tracker to use sensor
     * data during a test technically works, but messes up orientation if done while still in VR.
     * until VR is exited and re-entered.
     */
    public static void revertTracker() {
        Intent typeIntent = new Intent(ACTION_SET_TRACKER_TYPE);
        typeIntent.putExtra(EXTRA_TRACKER_TYPE, "sensor");
        typeIntent.setComponent(HEAD_TRACKING_COMPONENT);
        InstrumentationRegistry.getContext().startService(typeIntent);
    }

    public static String supportedModeToString(@SupportedMode int mode) {
        switch (mode) {
            case SupportedMode.FROZEN:
                return "frozen";
            case SupportedMode.SWEEP:
                return "sweep";
            case SupportedMode.ROTATE:
                return "rotate";
            case SupportedMode.CIRCLE_STRAFE:
                return "circle_strafe";
            case SupportedMode.MOTION_SICKNESS:
                return "motion_sickness";
            default:
                return "unknown_mode";
        }
    }

    private static void applyHeadTrackingModeInternal(VrTestRule rule, @SupportedMode int mode) {
        restartHeadTrackingServiceIfNecessary(rule);
        // Set the fake tracker mode to the given value.
        Intent modeIntent = new Intent(ACTION_SET_FAKE_TRACKER_MODE);
        modeIntent.putExtra(EXTRA_FAKE_TRACKER_MODE, supportedModeToString(mode));
        modeIntent.setComponent(HEAD_TRACKING_COMPONENT);
        Assert.assertTrue("Could not set head tracking mode",
                InstrumentationRegistry.getContext().startService(modeIntent) != null);
        rule.setTrackerDirty();
    }

    private static void restartHeadTrackingServiceIfNecessary(VrTestRule rule) {
        // If the tracker has already been dirtied, then we can assume that the tracker type
        // has already been set to "fake".
        if (rule.isTrackerDirty()) return;

        // VR sessions from previous tests can somehow interfere with the setting of the tracker
        // type, even if said previous sessions did not touch the head tracking service. Killing
        // the service before attempting to set the tracker type appears to work around this
        // issue.
        // TODO(https://crbug.com/829127): Remove this once the root cause is fixed.
        Intent stopIntent = new Intent();
        stopIntent.setComponent(HEAD_TRACKING_COMPONENT);
        InstrumentationRegistry.getContext().stopService(stopIntent);

        // Set the tracker tracker type to "fake".
        Intent typeIntent = new Intent(ACTION_SET_TRACKER_TYPE);
        typeIntent.putExtra(EXTRA_TRACKER_TYPE, "fake");
        typeIntent.setComponent(HEAD_TRACKING_COMPONENT);
        Assert.assertTrue("Could not restart head tracking service",
                InstrumentationRegistry.getContext().startService(typeIntent) != null);
        rule.setTrackerDirty();
    }
}