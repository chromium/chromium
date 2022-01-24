// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.ActivityManager;
import android.app.Application;
import android.app.Instrumentation;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.SharedPreferences;
import android.content.pm.InstrumentationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.support.test.InstrumentationRegistry;
import android.support.test.internal.runner.RunnerArgs;
import android.support.test.internal.runner.TestExecutor;
import android.support.test.internal.runner.TestLoader;
import android.support.test.internal.runner.TestRequest;
import android.support.test.internal.runner.TestRequestBuilder;
import android.support.test.runner.AndroidJUnitRunner;
import android.support.test.runner.MonitoringInstrumentation.ActivityFinisher;
import android.text.TextUtils;

import androidx.core.content.ContextCompat;

import dalvik.system.DexFile;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.LifetimeAssert;
import org.chromium.base.Log;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.multidex.ChromiumMultiDexInstaller;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.InMemorySharedPreferences;
import org.chromium.base.test.util.InMemorySharedPreferencesContext;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.build.BuildConfig;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * A custom AndroidJUnitRunner that supports multidex installer and lists out test information.
 * Also customizes various TestRunner and Instrumentation behaviors, like when Activities get
 * finished, and adds a timeout to waitForIdleSync.
 *
 * Please beware that is this not a class runner. It is declared in test apk AndroidManifest.xml
 * <instrumentation>
 */
@MainDex
public class BaseChromiumAndroidJUnitRunner extends AndroidJUnitRunner {
    private static final String LIST_ALL_TESTS_FLAG =
            "org.chromium.base.test.BaseChromiumAndroidJUnitRunner.TestList";
    private static final String LIST_TESTS_PACKAGE_FLAG =
            "org.chromium.base.test.BaseChromiumAndroidJUnitRunner.TestListPackage";
    /**
     * This flag is supported by AndroidJUnitRunner.
     *
     * See the following page for detail
     * https://developer.android.com/reference/android/support/test/runner/AndroidJUnitRunner.html
     */
    private static final String ARGUMENT_TEST_PACKAGE = "package";

    /**
     * The following arguments are corresponding to AndroidJUnitRunner command line arguments.
     * `annotation`: run with only the argument annotation
     * `notAnnotation`: run all tests except the ones with argument annotation
     * `log`: run in log only mode, do not execute tests
     *
     * For more detail, please check
     * https://developer.android.com/reference/android/support/test/runner/AndroidJUnitRunner.html
     */
    private static final String ARGUMENT_ANNOTATION = "annotation";
    private static final String ARGUMENT_NOT_ANNOTATION = "notAnnotation";
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

    @Override
    public Application newApplication(ClassLoader cl, String className, Context context)
            throws ClassNotFoundException, IllegalAccessException, InstantiationException {
        Context targetContext = super.getTargetContext();
        boolean hasUnderTestApk =
                !getContext().getPackageName().equals(targetContext.getPackageName());
        // When there is an under-test APK, BuildConfig belongs to it and does not indicate whether
        // the test apk is multidex. In this case, just assume it is.
        boolean isTestMultidex = hasUnderTestApk || BuildConfig.IS_MULTIDEX_ENABLED;
        if (isTestMultidex) {
            if (hasUnderTestApk) {
                // Need hacks to have multidex work when there is an under-test apk :(.
                ChromiumMultiDexInstaller.install(
                        new BaseChromiumRunnerCommon.MultiDexContextWrapper(
                                getContext(), targetContext));
                BaseChromiumRunnerCommon.reorderDexPathElements(cl, getContext(), targetContext);
            } else {
                ChromiumMultiDexInstaller.install(getContext());
            }
        }

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

    /**
     * Add TestListInstrumentationRunListener when argument ask the runner to list tests info.
     *
     * The running mechanism when argument has "listAllTests" is equivalent to that of
     * {@link android.support.test.runner.AndroidJUnitRunner#onStart()} except it adds
     * only TestListInstrumentationRunListener to monitor the tests.
     */
    @Override
    public void onStart() {
        Bundle arguments = InstrumentationRegistry.getArguments();
        if (shouldListTests()) {
            Log.w(TAG,
                    String.format("Runner will list out tests info in JSON without running tests. "
                                    + "Arguments: %s",
                            arguments.toString()));
            listTests(); // Intentionally not calling super.onStart() to avoid additional work.
        } else {
            if (arguments != null && arguments.getString(ARGUMENT_LOG_ONLY) != null) {
                Log.e(TAG,
                        String.format("Runner will log the tests without running tests."
                                        + " If this cause a test run to fail, please report to"
                                        + " crbug.com/754015. Arguments: %s",
                                arguments.toString()));
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                finishAllAppTasks(getTargetContext());
            }
            checkOrDeleteOnDiskSharedPreferences(false);
            clearDataDirectory(sInMemorySharedPreferencesContext);
            InstrumentationRegistry.getInstrumentation().setInTouchMode(true);
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
        runOnMainSync(() -> {
            Looper.myQueue().addIdleHandler(() -> {
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

    // TODO(yolandyan): Move this to test harness side once this class gets removed
    private void addTestListPackage(Bundle bundle) {
        PackageManager pm = getContext().getPackageManager();
        InstrumentationInfo info;
        try {
            info = pm.getInstrumentationInfo(getComponentName(), PackageManager.GET_META_DATA);
        } catch (NameNotFoundException e) {
            Log.e(TAG, String.format("Could not find component %s", getComponentName()));
            throw new RuntimeException(e);
        }
        Bundle metaDataBundle = info.metaData;
        if (metaDataBundle != null && metaDataBundle.getString(LIST_TESTS_PACKAGE_FLAG) != null) {
            bundle.putString(
                    ARGUMENT_TEST_PACKAGE, metaDataBundle.getString(LIST_TESTS_PACKAGE_FLAG));
        }
    }

    private void listTests() {
        Bundle results = new Bundle();
        TestListInstrumentationRunListener listener = new TestListInstrumentationRunListener();
        try {
            TestExecutor.Builder executorBuilder = new TestExecutor.Builder(this);
            executorBuilder.addRunListener(listener);
            Bundle junit3Arguments = new Bundle(InstrumentationRegistry.getArguments());
            junit3Arguments.putString(ARGUMENT_NOT_ANNOTATION, "org.junit.runner.RunWith");
            addTestListPackage(junit3Arguments);
            TestRequest listJUnit3TestRequest = createListTestRequest(junit3Arguments);
            results = executorBuilder.build().execute(listJUnit3TestRequest);

            Bundle junit4Arguments = new Bundle(InstrumentationRegistry.getArguments());
            junit4Arguments.putString(ARGUMENT_ANNOTATION, "org.junit.runner.RunWith");
            addTestListPackage(junit4Arguments);

            // Do not use Log runner from android test support.
            //
            // Test logging and execution skipping is handled by BaseJUnit4ClassRunner,
            // having ARGUMENT_LOG_ONLY in argument bundle here causes AndroidJUnitRunner
            // to use its own log-only class runner instead of BaseJUnit4ClassRunner.
            junit4Arguments.remove(ARGUMENT_LOG_ONLY);

            TestRequest listJUnit4TestRequest = createListTestRequest(junit4Arguments);
            results.putAll(executorBuilder.build().execute(listJUnit4TestRequest));
            listener.saveTestsToJson(
                    InstrumentationRegistry.getArguments().getString(LIST_ALL_TESTS_FLAG));
        } catch (IOException | RuntimeException e) {
            String msg = "Fatal exception when running tests";
            Log.e(TAG, msg, e);
            // report the exception to instrumentation out
            results.putString(Instrumentation.REPORT_KEY_STREAMRESULT,
                    msg + "\n" + Log.getStackTraceString(e));
        }
        finish(Activity.RESULT_OK, results);
    }

    private TestRequest createListTestRequest(Bundle arguments) {
        ArrayList<DexFile> dexFiles = new ArrayList<>();
        try {
            Class<?> bootstrapClass =
                    Class.forName("org.chromium.incrementalinstall.BootstrapApplication");
            DexFile[] incrementalInstallDexes =
                    (DexFile[]) bootstrapClass.getDeclaredField("sIncrementalDexFiles").get(null);
            dexFiles.addAll(Arrays.asList(incrementalInstallDexes));
        } catch (Exception e) {
            // Not an incremental apk.
            if (BuildConfig.IS_MULTIDEX_ENABLED
                    && Build.VERSION.SDK_INT <= Build.VERSION_CODES.KITKAT) {
                // Test listing fails for test classes that aren't in the main dex
                // (crbug.com/903820).
                addClassloaderDexFiles(dexFiles, getClass().getClassLoader());
            }
        }
        RunnerArgs runnerArgs =
                new RunnerArgs.Builder().fromManifest(this).fromBundle(arguments).build();
        TestRequestBuilder builder;
        if (!dexFiles.isEmpty()) {
            builder = new DexFileTestRequestBuilder(this, arguments, dexFiles);
        } else {
            builder = new TestRequestBuilder(this, arguments);
        }
        builder.addFromRunnerArgs(runnerArgs);
        builder.addApkToScan(getContext().getPackageCodePath());

        // Ignore tests from framework / support library classes.
        builder.removeTestPackage("android");
        builder.setClassLoader(new ForgivingClassLoader());
        return builder.build();
    }

    static boolean shouldListTests() {
        Bundle arguments = InstrumentationRegistry.getArguments();
        return arguments != null && arguments.getString(LIST_ALL_TESTS_FLAG) != null;
    }

    /**
     * Wraps TestRequestBuilder to make it work with incremental install and for multidex <= K.
     *
     * TestRequestBuilder does not know to look through the incremental install dex files, and has
     * no api for telling it to do so. This class checks to see if the list of tests was given
     * by the runner (mHasClassList), and if not overrides the auto-detection logic in build()
     * to manually scan all .dex files.
     *
     * On <= K, classes not in the main dex file are missed, so we manually list them by grabbing
     * the loaded DexFiles from the ClassLoader.
     */
    private static class DexFileTestRequestBuilder extends TestRequestBuilder {
        final List<String> mExcludedPrefixes = new ArrayList<String>();
        final List<String> mIncludedPrefixes = new ArrayList<String>();
        final List<DexFile> mDexFiles;
        boolean mHasClassList;

        DexFileTestRequestBuilder(Instrumentation instr, Bundle bundle, List<DexFile> dexFiles) {
            super(instr, bundle);
            mDexFiles = dexFiles;
            try {
                Field excludedPackagesField =
                        TestRequestBuilder.class.getDeclaredField("DEFAULT_EXCLUDED_PACKAGES");
                excludedPackagesField.setAccessible(true);
                mExcludedPrefixes.addAll(Arrays.asList((String[]) excludedPackagesField.get(null)));
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
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
        public TestRequest build() {
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
            TestLoader loader = new TestLoader();
            loader.setClassLoader(new ForgivingClassLoader());
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
                    if (!className.contains("$") && loader.loadIfTest(className) != null) {
                        addTestClass(className);
                    }
                }
            }
        }
    }

    private static Object getField(Class<?> clazz, Object instance, String name)
            throws ReflectiveOperationException {
        Field field = clazz.getDeclaredField(name);
        field.setAccessible(true);
        return field.get(instance);
    }

    private static void addClassloaderDexFiles(List<DexFile> dexFiles, ClassLoader cl) {
        // The main apk appears in the classpath twice sometimes, so check for apk path rather
        // than comparing DexFile instances (e.g. on kitkat without an apk-under-test).
        Set<String> apkPaths = new HashSet<>();
        try {
            Object pathList = getField(cl.getClass().getSuperclass(), cl, "pathList");
            Object[] dexElements =
                    (Object[]) getField(pathList.getClass(), pathList, "dexElements");
            for (Object dexElement : dexElements) {
                DexFile dexFile = (DexFile) getField(dexElement.getClass(), dexElement, "dexFile");
                // Prevent adding the main apk twice, and also skip any system libraries added due
                // to <uses-library> manifest entries.
                String apkPath = dexFile.getName();
                if (!apkPaths.contains(apkPath) && !apkPath.startsWith("/system")) {
                    dexFiles.add(dexFile);
                    apkPaths.add(apkPath);
                }
            }
        } catch (Exception e) {
            // No way to recover and test listing will fail.
            throw new RuntimeException(e);
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
                return mDelegateLoader.loadClass(name);
            } catch (NoClassDefFoundError e) {
                throw new ClassNotFoundException(name, e);
            }
        }
    }

    @Override
    public void finish(int resultCode, Bundle results) {
        if (shouldListTests()) {
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
            checkOrDeleteOnDiskSharedPreferences(true);
            UmaRecorderHolder.resetForTesting();

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

    // Since we prevent the default runner's behaviour of finishing Activities between tests, don't
    // finish Activities, don't have the runner wait for them to finish either (as this will add a 2
    // second timeout to each test).
    @Override
    protected void waitForActivitiesToComplete() {}

    // Note that in this class we cannot use ThreadUtils to post tasks as some tests initialize the
    // browser in ways that cause tasks posted through PostTask to not run. This function should be
    // used instead.
    @Override
    public void runOnMainSync(Runnable runner) {
        if (runner.getClass() == ActivityFinisher.class) {
            // This is a gross hack.
            // Without migrating to the androidx runner, we have no way to prevent
            // MonitoringInstrumentation from trying to kill our activities, and we rely on
            // MonitoringInstrumentation for many things like result reporting.
            // In order to allow batched tests to reuse Activities, drop the ActivityFinisher tasks
            // without running them.
            return;
        }
        super.runOnMainSync(runner);
    }

    /** Finishes all tasks Chrome has listed in Android's Overview. */
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private void finishAllAppTasks(final Context context) {
        // Close all of the tasks one by one.
        ActivityManager activityManager =
                (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        for (ActivityManager.AppTask task : activityManager.getAppTasks()) {
            task.finishAndRemoveTask();
        }
        long endTime = SystemClock.uptimeMillis()
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
        CallbackHelper allDestroyedCalledback = new CallbackHelper();
        ApplicationStatus.ActivityStateListener activityStateListener =
                new ApplicationStatus.ActivityStateListener() {
                    @Override
                    public void onActivityStateChange(Activity activity, int newState) {
                        switch (newState) {
                            case ActivityState.DESTROYED:
                                if (ApplicationStatus.isEveryActivityDestroyed()) {
                                    allDestroyedCalledback.notifyCalled();
                                    ApplicationStatus.unregisterActivityStateListener(this);
                                }
                                break;
                            case ActivityState.CREATED:
                                if (!activity.isFinishing()) {
                                    // This is required to ensure we finish any activities created
                                    // after doing the bulk finish operation below.
                                    ApiCompatibilityUtils.finishAndRemoveTask(activity);
                                }
                                break;
                        }
                    }
                };

        new Handler(Looper.getMainLooper()).post(() -> {
            if (ApplicationStatus.isEveryActivityDestroyed()) {
                allDestroyedCalledback.notifyCalled();
            } else {
                ApplicationStatus.registerStateListenerForAllActivities(activityStateListener);
            }
            for (Activity a : ApplicationStatus.getRunningActivities()) {
                if (!a.isFinishing()) ApiCompatibilityUtils.finishAndRemoveTask(a);
            }
        });
        try {
            allDestroyedCalledback.waitForFirst();
        } catch (TimeoutException e) {
            // There appears to be a framework bug on K and L where onStop and onDestroy are not
            // called for a handful of tests. We ignore these exceptions.
            Log.w(TAG, "Activity failed to be destroyed after a test");

            runOnMainSync(() -> {
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
            // E.g. Legacy multidex files.
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

    private void checkOrDeleteOnDiskSharedPreferences(boolean check) {
        File dataDir = ContextCompat.getDataDir(InstrumentationRegistry.getTargetContext());
        File prefsDir = new File(dataDir, "shared_prefs");
        File[] files = prefsDir.listFiles();
        if (files == null) {
            return;
        }
        ArrayList<File> badFiles = new ArrayList<>();
        for (File f : files) {
            // Multidex support library prefs need to stay or else multidex extraction will occur
            // needlessly.
            // WebView prefs need to stay because webview tests have no (good) way of hooking
            // SharedPreferences for instantiated WebViews.
            if (!f.getName().endsWith("multidex.version.xml")
                    && !f.getName().equals("WebViewChromiumPrefs.xml")) {
                if (check) {
                    badFiles.add(f);
                } else {
                    f.delete();
                }
            }
        }
        if (!badFiles.isEmpty()) {
            String errorMsg = "Found unexpected shared preferences file(s) after test ran.\n"
                    + "All code should use ContextUtils.getApplicationContext() when accessing "
                    + "SharedPreferences so that tests are hooked to use InMemorySharedPreferences."
                    + " This could also mean needing to override getSharedPreferences() on custom "
                    + "Context subclasses (e.g. ChromeBaseAppCompatActivity does this to make "
                    + "Preferences screens work).\n\n";

            SharedPreferences testPrefs = ContextUtils.getApplicationContext().getSharedPreferences(
                    "test", Context.MODE_PRIVATE);
            if (!(testPrefs instanceof InMemorySharedPreferences)) {
                errorMsg += String.format(
                        "ContextUtils.getApplicationContext() was set to type \"%s\", which does "
                                + "not delegate to InMemorySharedPreferencesContext (this is "
                                + "likely the issues).\n\n",
                        ContextUtils.getApplicationContext().getClass().getName());
            }

            errorMsg += "Files:\n * " + TextUtils.join("\n * ", badFiles);
            throw new AssertionError(errorMsg);
        }
    }
}
