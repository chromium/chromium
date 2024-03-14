// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.Application;
import android.app.Instrumentation;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Build.VERSION;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.system.Os;
import android.text.TextUtils;

import androidx.core.content.ContextCompat;
import androidx.test.InstrumentationRegistry;
import androidx.test.internal.runner.ClassPathScanner;
import androidx.test.internal.runner.RunnerArgs;
import androidx.test.internal.runner.TestExecutor;
import androidx.test.internal.runner.TestRequestBuilder;
import androidx.test.runner.AndroidJUnitRunner;

import dalvik.system.DexFile;

import org.junit.runner.Request;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.CommandLineInitUtil;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.LifetimeAssert;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.InMemorySharedPreferences;
import org.chromium.base.test.util.InMemorySharedPreferencesContext;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.build.BuildConfig;
import org.chromium.testing.TestListInstrumentationRunListener;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Enumeration;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 *
 *
 * <pre>
 * An Instrumentation subclass that:
 *    * Supports incremental install.
 *    * Installs an InMemorySharedPreferences, and a few other try-to-make-things-less-flaky things.
 * </pre>
 */
public class BaseChromiumAndroidJUnitRunner extends AndroidJUnitRunner {
    private static final String IS_UNIT_TEST_FLAG =
            "org.chromium.base.test.BaseChromiumAndroidJUnitRunner.IsUnitTest";
    private static final String EXTRA_CLANG_COVERAGE_DEVICE_FILE =
            "org.chromium.base.test.BaseChromiumAndroidJUnitRunner.ClangCoverageDeviceFile";

    private static final String ARGUMENT_LOG_ONLY = "log";

    private static final String TAG = "BaseJUnitRunner";

    private static final int STATUS_CODE_BATCH_FAILURE = 1338;

    // The ID of the bundle value Instrumentation uses to report the crash stack, if the test
    // crashed.
    private static final String BUNDLE_STACK_ID = "stack";

    private static final long WAIT_FOR_IDLE_TIMEOUT_MS = 10000L;

    private static final long FINISH_APP_TASKS_TIMEOUT_MS = 3000L;
    private static final long FINISH_APP_TASKS_POLL_INTERVAL_MS = 100;

    static InMemorySharedPreferencesContext sInMemorySharedPreferencesContext;
    private static boolean sTestListMode;

    static {
        CommandLineInitUtil.setFilenameOverrideForTesting(CommandLineFlags.getTestCmdLineFile());
    }

    @Override
    public Application newApplication(ClassLoader cl, String className, Context context)
            throws ClassNotFoundException, IllegalAccessException, InstantiationException {
        // Wrap |context| here so that calls to getSharedPreferences() from within
        // attachBaseContext() will hit our InMemorySharedPreferencesContext.
        sInMemorySharedPreferencesContext = new InMemorySharedPreferencesContext(context);
        Application ret = super.newApplication(cl, className, sInMemorySharedPreferencesContext);
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

    @Override
    public void onCreate(Bundle arguments) {
        if (arguments == null) {
            arguments = new Bundle();
        }
        sTestListMode = "true".equals(arguments.getString(ARGUMENT_LOG_ONLY));
        // Do not finish activities between tests so that batched tests can start
        // an activity in @BeforeClass and have it live until @AfterClass.
        arguments.putString("waitForActivitiesToComplete", "false");
        super.onCreate(arguments);
    }

    /**
     * Add TestListInstrumentationRunListener when argument ask the runner to list tests info.
     *
     * The running mechanism when argument has "listAllTests" is equivalent to that of
     * {@link androidx.test.runner.AndroidJUnitRunner#onStart()} except it adds
     * only TestListInstrumentationRunListener to monitor the tests.
     */
    @Override
    public void onStart() {
        Bundle arguments = InstrumentationRegistry.getArguments();
        ResettersForTesting.enable();
        if (arguments.getString(IS_UNIT_TEST_FLAG) != null) {
            LibraryLoader.setBrowserProcessStartupBlockedForTesting();
        }

        if (sTestListMode) {
            Log.w(
                    TAG,
                    String.format(
                            "Runner will list out tests info in JSON without running tests. "
                                    + "Arguments: %s",
                            arguments.toString()));
            listTests(); // Intentionally not calling super.onStart() to avoid additional work.
        } else {
            if (arguments != null && arguments.getString(ARGUMENT_LOG_ONLY) != null) {
                Log.e(
                        TAG,
                        String.format(
                                "Runner will log the tests without running tests."
                                        + " If this cause a test run to fail, please report to"
                                        + " crbug.com/754015. Arguments: %s",
                                arguments.toString()));
            }
            finishAllAppTasks(getTargetContext());
            BaseJUnit4TestRule.clearJobSchedulerJobs();
            checkOrDeleteOnDiskSharedPreferences(false);
            clearDataDirectory(sInMemorySharedPreferencesContext);
            InstrumentationRegistry.getInstrumentation().setInTouchMode(true);
            // //third_party/mockito is looking for android.support.test.InstrumentationRegistry.
            // Manually set target to override. We can remove this once we roll mockito to support
            // androidx.test.
            System.setProperty(
                    "org.mockito.android.target",
                    InstrumentationRegistry.getTargetContext().getCacheDir().getPath());
            setClangCoverageEnvIfEnabled();
            super.onStart();
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
            idleCallback.waitForFirst((int) WAIT_FOR_IDLE_TIMEOUT_MS, TimeUnit.MILLISECONDS);
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
        } catch (IOException | RuntimeException e) {
            String msg = "Fatal exception when running tests";
            Log.e(TAG, msg, e);
            // report the exception to instrumentation out
            results.putString(
                    Instrumentation.REPORT_KEY_STREAMRESULT,
                    msg + "\n" + Log.getStackTraceString(e));
        }
        finish(Activity.RESULT_OK, results);
    }

    private Request createListTestRequest(Bundle arguments) {
        TestRequestBuilder builder;
        if (BuildConfig.IS_INCREMENTAL_INSTALL) {
            try {
                Class<?> bootstrapClass =
                        Class.forName("org.chromium.incrementalinstall.BootstrapApplication");
                DexFile[] incrementalInstallDexes =
                        (DexFile[])
                                bootstrapClass.getDeclaredField("sIncrementalDexFiles").get(null);
                builder =
                        new DexFileTestRequestBuilder(
                                this, arguments, Arrays.asList(incrementalInstallDexes));
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        } else {
            builder = new TestRequestBuilder(this, arguments);
        }
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
     * Wraps TestRequestBuilder to make it work with incremental install.
     *
     * <p>TestRequestBuilder does not know to look through the incremental install dex files, and
     * has no api for telling it to do so. This class checks to see if the list of tests was given
     * by the runner (mHasClassList), and if not overrides the auto-detection logic in build() to
     * manually scan all .dex files.
     */
    private static class DexFileTestRequestBuilder extends TestRequestBuilder {
        final List<String> mExcludedPrefixes = new ArrayList<String>();
        final List<String> mIncludedPrefixes = new ArrayList<String>();
        final List<DexFile> mDexFiles;
        boolean mHasClassList;
        private ClassLoader mClassLoader = DexFileTestRequestBuilder.class.getClassLoader();

        DexFileTestRequestBuilder(Instrumentation instr, Bundle bundle, List<DexFile> dexFiles) {
            super(instr, bundle);
            mDexFiles = dexFiles;
            mExcludedPrefixes.addAll(ClassPathScanner.getDefaultExcludedPackages());
        }

        @Override
        public TestRequestBuilder removeTestPackage(String testPackage) {
            mExcludedPrefixes.add(testPackage);
            return this;
        }

        @Override
        public TestRequestBuilder addFromRunnerArgs(RunnerArgs runnerArgs) {
            mExcludedPrefixes.addAll(runnerArgs.notTestPackages);
            mIncludedPrefixes.addAll(runnerArgs.testPackages);
            // Without clearing, You get IllegalArgumentException:
            // Ambiguous arguments: cannot provide both test package and test class(es) to run
            runnerArgs.notTestPackages.clear();
            runnerArgs.testPackages.clear();
            return super.addFromRunnerArgs(runnerArgs);
        }

        @Override
        public TestRequestBuilder addTestClass(String className) {
            mHasClassList = true;
            return super.addTestClass(className);
        }

        @Override
        public TestRequestBuilder addTestMethod(String testClassName, String testMethodName) {
            mHasClassList = true;
            return super.addTestMethod(testClassName, testMethodName);
        }

        @Override
        public TestRequestBuilder setClassLoader(ClassLoader loader) {
            mClassLoader = loader;
            return super.setClassLoader(loader);
        }

        @Override
        public Request build() {
            // If a test class was requested, then no need to iterate class loader.
            if (!mHasClassList) {
                // builder.addApkToScan uses new DexFile(path) under the hood, which on Dalvik OS's
                // assumes that the optimized dex is in the default location (crashes).
                // Perform our own dex file scanning instead as a workaround.
                scanDexFilesForTestClasses();
            }
            return super.build();
        }

        private static boolean startsWithAny(String str, List<String> prefixes) {
            for (String prefix : prefixes) {
                if (str.startsWith(prefix)) {
                    return true;
                }
            }
            return false;
        }

        private void scanDexFilesForTestClasses() {
            Log.i(TAG, "Scanning loaded dex files for test classes.");
            // Mirror TestRequestBuilder.getClassNamesFromClassPath().
            for (DexFile dexFile : mDexFiles) {
                Enumeration<String> classNames = dexFile.entries();
                while (classNames.hasMoreElements()) {
                    String className = classNames.nextElement();
                    if (!mIncludedPrefixes.isEmpty()
                            && !startsWithAny(className, mIncludedPrefixes)) {
                        continue;
                    }
                    if (startsWithAny(className, mExcludedPrefixes)) {
                        continue;
                    }
                    if (!className.endsWith("Test")) {
                        // Speeds up test listing to filter by name before
                        // trying to load the class. We have an ErrorProne
                        // check that enforces this convention:
                        // //tools/android/errorprone_plugin/src/org/chromium/tools/errorprone/plugin/TestClassNameCheck.java
                        // As of Dec 2019, this speeds up test listing on
                        // android-kitkat-arm-rel from 41s -> 23s.
                        continue;
                    }
                    if (!className.contains("$") && checkIfTest(className, mClassLoader)) {
                        addTestClass(className);
                    }
                }
            }
        }
    }

    /**
     * ClassLoader that translates NoClassDefFoundError into ClassNotFoundException.
     *
     * Required because Android's TestLoader class tries to load all classes, but catches only
     * ClassNotFoundException.
     *
     * One way NoClassDefFoundError is triggered is on Android L when a class extends a non-existent
     * class. See https://crbug.com/912690.
     */
    private static class ForgivingClassLoader extends ClassLoader {
        private final ClassLoader mDelegateLoader = getClass().getClassLoader();

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
                    throw new ClassNotFoundException();
                }
                return ret;
            } catch (NoClassDefFoundError e) {
                throw new ClassNotFoundException(name, e);
            }
        }
    }

    private static boolean checkIfTest(String className, ClassLoader classLoader) {
        Class<?> loadedClass = tryLoadClass(className, classLoader);
        if (loadedClass != null && isTestClass(loadedClass)) {
            return true;
        }
        return false;
    }

    private static Class<?> tryLoadClass(String className, ClassLoader classLoader) {
        try {
            return Class.forName(className, false, classLoader);
        } catch (NoClassDefFoundError | ClassNotFoundException e) {
            return null;
        }
    }

    // Copied from android.support.test.runner code.
    private static boolean isTestClass(Class<?> loadedClass) {
        try {
            if (Modifier.isAbstract(loadedClass.getModifiers())) {
                Log.d(
                        TAG,
                        String.format(
                                "Skipping abstract class %s: not a test", loadedClass.getName()));
                return false;
            }
            if (loadedClass.isAnnotationPresent(RunWith.class)) {
                return true;
            }
            for (Method testMethod : loadedClass.getMethods()) {
                if (testMethod.isAnnotationPresent(org.junit.Test.class)) {
                    return true;
                }
            }
            Log.d(TAG, String.format("Skipping class %s: not a test", loadedClass.getName()));
            return false;
        } catch (Exception e) {
            // Defensively catch exceptions - Will throw runtime exception if it cannot load
            // methods.
            Log.w(TAG, String.format("%s in isTestClass for %s", e, loadedClass.getName()));
            return false;
        } catch (Error e) {
            // defensively catch Errors too
            Log.w(TAG, String.format("%s in isTestClass for %s", e, loadedClass.getName()));
            return false;
        }
    }

    @Override
    public void finish(int resultCode, Bundle results) {
        if (sTestListMode) {
            super.finish(resultCode, results);
            return;
        }

        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                finishAllAppTasks(getTargetContext());
            }
            finishAllActivities();
        } catch (Exception e) {
            // Ignore any errors finishing Activities so that otherwise passing tests don't fail
            // during tear down due to framework issues. See crbug.com/653731.
        }

        try {
            writeClangCoverageProfileIfEnabled();
            checkOrDeleteOnDiskSharedPreferences(true);

            // There is a bug on L and below that DestroyActivitiesRule does not cause onStop and
            // onDestroy. On other versions, DestroyActivitiesRule may still fail flakily. Ignore
            // lifetime asserts if that is the case.
            if (!ApplicationStatus.isInitialized()
                    || ApplicationStatus.isEveryActivityDestroyed()) {
                LifetimeAssert.assertAllInstancesDestroyedForTesting();
            } else {
                LifetimeAssert.resetForTesting();
            }
        } catch (Exception e) {
            // It's not possible (as far as I know) to update already reported test results, so we
            // send another status update have the instrumentation test instance parse it.
            Bundle b = new Bundle();
            b.putString(BUNDLE_STACK_ID, Log.getStackTraceString(e));
            InstrumentationRegistry.getInstrumentation().sendStatus(STATUS_CODE_BATCH_FAILURE, b);
        }

        // This will end up force stopping the package, so code after this line will not run.
        super.finish(resultCode, results);
    }

    /** Finishes all tasks Chrome has listed in Android's Overview. */
    private void finishAllAppTasks(final Context context) {
        // Close all of the tasks one by one.
        ActivityManager activityManager =
                (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        for (ActivityManager.AppTask task : activityManager.getAppTasks()) {
            task.finishAndRemoveTask();
        }
        long endTime =
                SystemClock.uptimeMillis()
                        + ScalableTimeout.scaleTimeout(FINISH_APP_TASKS_TIMEOUT_MS);
        while (activityManager.getAppTasks().size() != 0 && SystemClock.uptimeMillis() < endTime) {
            try {
                Thread.sleep(FINISH_APP_TASKS_POLL_INTERVAL_MS);
            } catch (InterruptedException e) {
            }
        }
    }

    private void finishAllActivities() {
        // This mirrors the default logic of the test runner for finishing Activities when
        // ApplicationStatus isn't initialized. However, we keep Chromium's logic for finishing
        // Activities below both because it's worked historically and we don't want to risk breaking
        // things, and because the ActivityFinisher does some filtering on which Activities it
        // chooses to finish which could potentially cause issues.
        if (!ApplicationStatus.isInitialized()) {
            runOnMainSync(() -> new ActivityFinisher().run());
            super.waitForActivitiesToComplete();
            return;
        }
        Handler mainHandler = new Handler(Looper.getMainLooper());
        CallbackHelper allDestroyedCalledback = new CallbackHelper();
        ApplicationStatus.ActivityStateListener activityStateListener =
                new ApplicationStatus.ActivityStateListener() {
                    @Override
                    public void onActivityStateChange(Activity activity, int newState) {
                        switch (newState) {
                            case ActivityState.DESTROYED:
                                if (ApplicationStatus.isEveryActivityDestroyed()) {
                                    // Allow onDestroy to finish running before we notify.
                                    mainHandler.post(
                                            () -> {
                                                allDestroyedCalledback.notifyCalled();
                                            });
                                    ApplicationStatus.unregisterActivityStateListener(this);
                                }
                                break;
                            case ActivityState.CREATED:
                                if (!activity.isFinishing()) {
                                    // This is required to ensure we finish any activities created
                                    // after doing the bulk finish operation below.
                                    activity.finishAndRemoveTask();
                                }
                                break;
                        }
                    }
                };

        mainHandler.post(
                () -> {
                    if (ApplicationStatus.isEveryActivityDestroyed()) {
                        allDestroyedCalledback.notifyCalled();
                    } else {
                        ApplicationStatus.registerStateListenerForAllActivities(
                                activityStateListener);
                    }
                    for (Activity a : ApplicationStatus.getRunningActivities()) {
                        if (!a.isFinishing()) a.finishAndRemoveTask();
                    }
                });
        try {
            allDestroyedCalledback.waitForFirst();
        } catch (TimeoutException e) {
            // There appears to be a framework bug on K and L where onStop and onDestroy are not
            // called for a handful of tests. We ignore these exceptions.
            Log.w(TAG, "Activity failed to be destroyed after a test");

            runOnMainSync(
                    () -> {
                        // Make sure subsequent tests don't have these notifications firing.
                        ApplicationStatus.unregisterActivityStateListener(activityStateListener);
                    });
        }
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
            if (file.getName().equals("incremental-install-files")) {
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

    private static boolean isSharedPrefFileAllowed(File f) {
        // WebView prefs need to stay because webview tests have no (good) way of hooking
        // SharedPreferences for instantiated WebViews.
        String[] allowlist =
                new String[] {
                    "WebViewChromiumPrefs.xml",
                    "org.chromium.android_webview.devui.MainActivity.xml",
                    "AwComponentUpdateServicePreferences.xml",
                    "ComponentsProviderServicePreferences.xml",
                    "org.chromium.webengine.test.instrumentation_test_apk_preferences.xml",
                    "AwOriginVisitLoggerPrefs.xml",
                };
        for (String name : allowlist) {
            // SharedPreferences may also access a ".bak" backup file from a previous run. See
            // https://crbug.com/1462105#c4 and
            // https://cs.android.com/android/platform/superproject/main/+/main:frameworks/base/core/java/android/app/SharedPreferencesImpl.java;l=213;drc=6f7c5e0914a18e6adafaa319e670363772e51691
            // for details.
            String backupName = name + ".bak";

            if (f.getName().equals(name) || f.getName().equals(backupName)) {
                return true;
            }
        }
        return false;
    }

    private void checkOrDeleteOnDiskSharedPreferences(boolean check) {
        File dataDir = ContextCompat.getDataDir(InstrumentationRegistry.getTargetContext());
        File prefsDir = new File(dataDir, "shared_prefs");
        File[] files = prefsDir.listFiles();
        if (files == null) {
            return;
        }
        ArrayList<File> badFiles = new ArrayList<>();
        for (File f : files) {
            if (isSharedPrefFileAllowed(f)) {
                continue;
            }
            if (check) {
                badFiles.add(f);
            } else {
                f.delete();
            }
        }
        if (!badFiles.isEmpty()) {
            String errorMsg =
                    "Found unexpected shared preferences file(s) after test ran.\n"
                        + "All code should use ContextUtils.getApplicationContext() when accessing"
                        + " SharedPreferences so that tests are hooked to use"
                        + " InMemorySharedPreferences. This could also mean needing to override"
                        + " getSharedPreferences() on custom Context subclasses (e.g."
                        + " ChromeBaseAppCompatActivity does this to make Preferences screens"
                        + " work).\n\n";

            SharedPreferences testPrefs =
                    ContextUtils.getApplicationContext()
                            .getSharedPreferences("test", Context.MODE_PRIVATE);
            if (!(testPrefs instanceof InMemorySharedPreferences)) {
                errorMsg +=
                        String.format(
                                "ContextUtils.getApplicationContext() was set to type \"%s\", which"
                                    + " does not delegate to InMemorySharedPreferencesContext (this"
                                    + " is likely the issues).\n\n",
                                ContextUtils.getApplicationContext().getClass().getName());
            }

            errorMsg += "Files:\n * " + TextUtils.join("\n * ", badFiles);
            throw new AssertionError(errorMsg);
        }
    }

    /** Configure the required environment variable if Clang coverage argument exists. */
    private void setClangCoverageEnvIfEnabled() {
        String clangProfileFile =
                InstrumentationRegistry.getArguments().getString(EXTRA_CLANG_COVERAGE_DEVICE_FILE);
        if (clangProfileFile != null) {
            try {
                Os.setenv("LLVM_PROFILE_FILE", clangProfileFile, /* override= */ true);
            } catch (Exception e) {
                Log.w(TAG, "failed to set LLVM_PROFILE_FILE", e);
            }
        }
    }

    /**
     * Invoke __llvm_profile_dump() to write raw clang coverage profile to device.
     * Noop if the required build flag is not set.
     */
    private void writeClangCoverageProfileIfEnabled() {
        if (BuildConfig.WRITE_CLANG_PROFILING_DATA && LibraryLoader.getInstance().isInitialized()) {
            ClangProfiler.writeClangProfilingProfile();
        }
    }
}
