// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.app.Activity;
import android.app.Application;
import android.app.Instrumentation;
import android.content.Context;
import android.content.ContextWrapper;
import android.os.Build.VERSION;
import android.os.Bundle;
import android.os.Looper;

import androidx.core.content.ContextCompat;
import androidx.test.InstrumentationRegistry;
import androidx.test.espresso.IdlingPolicies;
import androidx.test.internal.runner.RunnerArgs;
import androidx.test.internal.runner.TestExecutor;
import androidx.test.internal.runner.TestRequestBuilder;
import androidx.test.runner.AndroidJUnitRunner;

import org.junit.runner.Request;

import org.chromium.base.CommandLineInitUtil;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.InMemorySharedPreferencesContext;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.base.test.util.TestAnimations;
import org.chromium.testing.TestListInstrumentationRunListener;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.Field;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * <pre>
 * An Instrumentation subclass that:
 *    * Installs an InMemorySharedPreferences, and a few other try-to-make-things-less-flaky things.
 * </pre>
 */
public class BaseChromiumAndroidJUnitRunner extends AndroidJUnitRunner {
    private static final String IS_UNIT_TEST_FLAG = "BaseChromiumAndroidJUnitRunner.IsUnitTest";
    private static final String EXTRA_TIMEOUT_SCALE = "BaseChromiumAndroidJUnitRunner.TimeoutScale";
    private static final String EXTRA_TRACE_FILE = "BaseChromiumAndroidJUnitRunner.TraceFile";

    private static final String ARGUMENT_LOG_ONLY = "log";

    private static final String TAG = "BaseJUnitRunner";

    private static final long WAIT_FOR_IDLE_TIMEOUT_MS = 10000L;

    static BaseChromiumAndroidJUnitRunner sInstance;
    static Application sApplication;
    static InMemorySharedPreferencesContext sInMemorySharedPreferencesContext;
    private static boolean sTestListMode;

    public BaseChromiumAndroidJUnitRunner() {
        sInstance = this;
    }

    @Override
    public Application newApplication(ClassLoader cl, String className, Context context)
            throws ClassNotFoundException, IllegalAccessException, InstantiationException {
        // Must come before super.newApplication(), because Chrome's Application.attachBaseContext()
        // initializes command-line.
        CommandLineInitUtil.setFilenameOverrideForTesting(CommandLineFlags.getTestCmdLineFile());

        // Wrap |context| here so that calls to getSharedPreferences() from within
        // attachBaseContext() will hit our InMemorySharedPreferencesContext.
        sInMemorySharedPreferencesContext = new InMemorySharedPreferencesContext(context);
        Application ret = super.newApplication(cl, className, sInMemorySharedPreferencesContext);
        sApplication = ret;
        try {
            // There is framework code that assumes Application.getBaseContext() can be casted to
            // ContextImpl (on KitKat for broadcast receivers, refer to ActivityThread.java), so
            // invert the wrapping relationship.
            Field baseField = ContextWrapper.class.getDeclaredField("mBase");
            baseField.setAccessible(true);
            baseField.set(ret, context);
            baseField.set(sInMemorySharedPreferencesContext, ret);
        } catch (NoSuchFieldException e) {
            throw new RuntimeException(e);
        }

        // Replace the application with our wrapper here for any code that runs between
        // Application.attachBaseContext() and our BaseJUnit4TestRule (e.g. Application.onCreate()).
        ContextUtils.initApplicationContextForTests(sInMemorySharedPreferencesContext);
        return ret;
    }

    @Override
    public Context getTargetContext() {
        // The target context by default points directly at the ContextImpl, which we can't wrap.
        // Make it instead point at the Application.
        return sInMemorySharedPreferencesContext;
    }

    private static boolean isDefaultProcess() {
        return !ContextUtils.getProcessName().contains(":");
    }

    @Override
    public void onCreate(Bundle arguments) {
        if (!isDefaultProcess()) {
            super.onCreate(arguments);
            return;
        }
        if (arguments == null) {
            arguments = new Bundle();
        }
        sTestListMode = "true".equals(arguments.getString(ARGUMENT_LOG_ONLY));
        // Do not finish activities between tests so that batched tests can start
        // an activity in @BeforeClass and have it live until @AfterClass.
        arguments.putString("waitForActivitiesToComplete", "false");
        super.onCreate(arguments);
        if (!sTestListMode) {
            // Initialize before Application.onCreate() to ensure settings take effect.
            initTestRunner(arguments);
        }
    }

    /**
     * Add TestListInstrumentationRunListener when argument ask the runner to list tests info.
     *
     * <p>The running mechanism when argument has "listAllTests" is equivalent to that of {@link
     * androidx.test.runner.AndroidJUnitRunner#onStart()} except it adds only
     * TestListInstrumentationRunListener to monitor the tests.
     */
    @Override
    public void onStart() {
        if (!isDefaultProcess()) {
            throw new IllegalStateException();
        }
        Bundle arguments = InstrumentationRegistry.getArguments();
        if (sTestListMode) {
            Log.w(
                    TAG,
                    String.format(
                            "Runner will list out tests info in JSON without running tests. "
                                    + "Arguments: %s",
                            arguments.toString()));
            listTests(); // Intentionally not calling super.onStart() to avoid additional work.
        } else {
            ThreadUtils.recordInstrumentationThreadForTesting();
            // Full name required because the super class has a nested class of the same name.
            org.chromium.base.test.ActivityFinisher.finishAll();
            super.onStart();
        }
    }

    // Called on the UI thread.
    private void initTestRunner(Bundle arguments) {
        String timeoutScale = arguments.getString(EXTRA_TIMEOUT_SCALE);
        if (timeoutScale != null) {
            ScalableTimeout.setScale(Float.valueOf(timeoutScale));
        }
        CommandLineFlags.ensureInitialized();
        BaseJUnit4ClassRunner.clearJobSchedulerJobs();
        clearDataDirectory(sInMemorySharedPreferencesContext);
        setInTouchMode(true);
        // //third_party/mockito is looking for android.support.test.InstrumentationRegistry.
        // Manually set target to override. We can remove this once we roll mockito to support
        // androidx.test.
        System.setProperty(
                "org.mockito.android.target",
                sInMemorySharedPreferencesContext.getCacheDir().getPath());
        // Reduce the time Espresso waits before failing to be less than the Python test timeout.
        IdlingPolicies.setMasterPolicyTimeout(5, TimeUnit.SECONDS);
        if (arguments.getString(IS_UNIT_TEST_FLAG) != null) {
            LibraryLoader.setBrowserProcessStartupBlockedForTesting();
        }
        ResettersForTesting.enable();

        String traceOutput = arguments.getString(EXTRA_TRACE_FILE);
        if (traceOutput != null) {
            File traceOutputFile = new File(traceOutput);
            File traceOutputDir = traceOutputFile.getParentFile();

            if (traceOutputDir != null) {
                if (traceOutputDir.exists() || traceOutputDir.mkdirs()) {
                    TestTraceEvent.enable(traceOutputFile);
                }
            }
        }
    }

    // The Instrumentation implementation of waitForIdleSync does not have a timeout and can wait
    // indefinitely in the case of animations, etc.
    //
    // You should never use this function in new code, as waitForIdleSync hides underlying issues.
    // There's almost always a better condition to wait on.
    @Override
    public void waitForIdleSync() {
        final CallbackHelper idleCallback = new CallbackHelper();
        runOnMainSync(
                () -> {
                    Looper.myQueue()
                            .addIdleHandler(
                                    () -> {
                                        idleCallback.notifyCalled();
                                        return false;
                                    });
                });

        try {
            idleCallback.waitForOnly(WAIT_FOR_IDLE_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        } catch (TimeoutException ex) {
            Log.w(TAG, "Timeout while waiting for idle main thread.");
        }
    }

    private void listTests() {
        Bundle results = new Bundle();
        try {
            TestExecutor.Builder executorBuilder = new TestExecutor.Builder(this);
            executorBuilder.addRunListener(new TestListInstrumentationRunListener(true));

            // Do not use androidx's AndroidLogOnlyBuilder.
            //
            // We require BaseJUnit4ClassRunner to implement our test skipping / restrictions logic,
            // but ARGUMENT_LOG_ONLY means that our runner will not be used.
            // Remove the argument, and have BaseJUnit4ClassRunner run in no-op mode.
            Bundle junit4Arguments = new Bundle(InstrumentationRegistry.getArguments());
            junit4Arguments.remove(ARGUMENT_LOG_ONLY);

            Request listJUnit4TestRequest = createListTestRequest(junit4Arguments);
            results.putAll(executorBuilder.build().execute(listJUnit4TestRequest));
            finish(Activity.RESULT_OK, results);
        } catch (IOException | RuntimeException e) {
            String msg = "Fatal exception when running tests";
            Log.e(TAG, msg, e);
            results.putString(
                    Instrumentation.REPORT_KEY_STREAMRESULT,
                    msg + "\n" + Log.getStackTraceString(e));
            finish(Activity.RESULT_CANCELED, results);
        }
    }

    private Request createListTestRequest(Bundle arguments) {
        TestRequestBuilder builder = new TestRequestBuilder(this, arguments);
        RunnerArgs runnerArgs =
                new RunnerArgs.Builder().fromManifest(this).fromBundle(this, arguments).build();
        builder.addFromRunnerArgs(runnerArgs);
        builder.addPathToScan(getContext().getPackageCodePath());

        // Ignore tests from framework / support library classes.
        builder.removeTestPackage("android");
        builder.setClassLoader(new ForgivingClassLoader());
        return builder.build();
    }

    static boolean shouldListTests() {
        return sTestListMode;
    }

    /**
     * ClassLoader that translates NoClassDefFoundError into ClassNotFoundException.
     *
     * <p>Required because Android's TestLoader class tries to load all classes, but catches only
     * ClassNotFoundException.
     *
     * <p>One way NoClassDefFoundError is triggered is on Android L when a class extends a
     * non-existent class. See https://crbug.com/912690.
     */
    private class ForgivingClassLoader extends ClassLoader {
        private final ClassLoader mDelegateLoader = getContext().getClassLoader();

        @Override
        public Class<?> loadClass(String name) throws ClassNotFoundException {
            try {
                var ret = mDelegateLoader.loadClass(name);
                // Prevent loading classes that should be skipped due to @MinAndroidSdkLevelon.
                // Loading them can cause NoClassDefFoundError to be thrown by junit when listing
                // methods (if methods contain types from higher sdk version).
                // E.g.: https://chromium-review.googlesource.com/c/chromium/src/+/4738415/1
                MinAndroidSdkLevel annotation = ret.getAnnotation(MinAndroidSdkLevel.class);
                if (annotation != null && annotation.value() > VERSION.SDK_INT) {
                    throw new ClassNotFoundException("Skipping due to @MinAndroidSdkLevel");
                }
                return ret;
            } catch (NoClassDefFoundError e) {
                throw new ClassNotFoundException(name, e);
            }
        }
    }

    @Override
    public void finish(int resultCode, Bundle results) {
        if (sTestListMode) {
            super.finish(resultCode, results);
            return;
        }

        // Leave animations in the default state.
        TestAnimations.setEnabled(true);

        // This will end up force stopping the package, so code after this line will not run.
        super.finish(resultCode, results);
    }

    // This method clears the data directory for the test apk, but device_utils.py clears the data
    // for the apk under test via `pm clear`. Fake module smoke tests in particular requires some
    // data to be kept for the apk under test: /sdcard/Android/data/package/files/local_testing
    private static void clearDataDirectory(Context targetContext) {
        File dataDir = ContextCompat.getDataDir(targetContext);
        File[] files = dataDir.listFiles();
        if (files == null) return;
        for (File file : files) {
            // Symlink to app's native libraries.
            if (file.getName().equals("lib")) {
                continue;
            }
            if (file.getName().equals("code_cache")) {
                continue;
            }
            // SharedPreferences handled by checkOrDeleteOnDiskSharedPreferences().
            if (file.getName().equals("shared_prefs")) {
                continue;
            }
            if (file.isDirectory()
                    && (file.getName().startsWith("app_") || file.getName().equals("cache"))) {
                // Directories are lazily created by PathUtils only once, and so can be cleared but
                // not removed.
                for (File subFile : file.listFiles()) {
                    if (!FileUtils.recursivelyDeleteFile(subFile, FileUtils.DELETE_ALL)) {
                        throw new RuntimeException(
                                "Could not delete file: " + subFile.getAbsolutePath());
                    }
                }
            } else if (!FileUtils.recursivelyDeleteFile(file, FileUtils.DELETE_ALL)) {
                throw new RuntimeException("Could not delete file: " + file.getAbsolutePath());
            }
        }
    }
}
