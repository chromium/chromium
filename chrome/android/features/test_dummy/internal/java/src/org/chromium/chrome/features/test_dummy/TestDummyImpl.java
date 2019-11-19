// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.test_dummy;

import android.app.Activity;
import android.content.Intent;
import android.support.v7.app.AlertDialog;

import androidx.annotation.IntDef;

import org.chromium.base.Log;
import org.chromium.base.annotations.NativeMethods;

import java.io.IOException;
import java.io.InputStream;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Locale;

/** Test dummy implementation. */
public class TestDummyImpl implements TestDummy {
    private static final String TAG = "TestDummyImpl";

    @IntDef({TestCase.EXECUTE_JAVA, TestCase.EXECUTE_NATIVE})
    @Retention(RetentionPolicy.SOURCE)
    private @interface TestCase {
        int EXECUTE_JAVA = 0;
        int EXECUTE_NATIVE = 1;
        int LOAD_JAVA_RESOURCE = 2;
        int LOAD_NATIVE_RESOURCE = 3;
    }

    @Override
    public void launch(Intent intent, Activity activity) {
        @TestCase
        int testCase = intent.getExtras().getInt("test_case");
        switch (testCase) {
            case TestCase.EXECUTE_JAVA:
                executeJava(activity);
                break;
            case TestCase.EXECUTE_NATIVE:
                executeNative(activity);
                break;
            case TestCase.LOAD_JAVA_RESOURCE:
                loadJavaResource(activity);
                break;
            case TestCase.LOAD_NATIVE_RESOURCE:
                loadNativeResource(activity);
                break;
            default:
                throw new RuntimeException("Unknown test case " + testCase);
        }
    }

    @NativeMethods
    interface Natives {
        int execute();
        String loadResource();
    }

    private void showDoneDialog(Activity activity, @TestCase int testCase, boolean pass) {
        String message =
                String.format(Locale.US, "Test Case %d: %s", testCase, pass ? "pass" : "fail");
        AlertDialog.Builder builder = new AlertDialog.Builder(activity);
        builder.setTitle("Test Dummy Result");
        builder.setMessage(message);
        builder.setCancelable(true);
        builder.create().show();
        Log.i(TAG, message); // Useful for local testing and grepping.
    }

    private void executeJava(Activity activity) {
        showDoneDialog(activity, TestCase.EXECUTE_JAVA, true);
    }

    private void executeNative(Activity activity) {
        int value = TestDummyImplJni.get().execute();
        boolean pass = (value == 123);
        showDoneDialog(activity, TestCase.EXECUTE_NATIVE, pass);
    }

    private void loadJavaResource(Activity activity) {
        InputStream stream = activity.getResources().openRawResource(R.raw.dummy_resource);
        StringBuilder stringBuilder = new StringBuilder();
        boolean result;
        // Note: Not using BufferedReader(-).readLine() in order to capture the content of the
        // entire resource file.
        try {
            int character;
            while ((character = stream.read()) != -1) {
                stringBuilder.append((char) character);
            }
            result = stringBuilder.toString().equals("hello world");
        } catch (IOException e) {
            Log.e(TAG, "Failed to load resource: %s", e);
            result = false;
        }
        showDoneDialog(activity, TestCase.LOAD_JAVA_RESOURCE, result);
    }

    private void loadNativeResource(Activity activity) {
        boolean result = TestDummyImplJni.get().loadResource().equals("Hello, World!");
        showDoneDialog(activity, TestCase.LOAD_NATIVE_RESOURCE, result);
    }
}
