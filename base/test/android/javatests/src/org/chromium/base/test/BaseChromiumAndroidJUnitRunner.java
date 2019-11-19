// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test;

import android.app.Activity;
import android.app.Application;
import android.app.Instrumentation;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.pm.InstrumentationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Build;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;
import android.support.test.internal.runner.RunnerArgs;
import android.support.test.internal.runner.TestExecutor;
import android.support.test.internal.runner.TestLoader;
import android.support.test.internal.runner.TestRequest;
import android.support.test.internal.runner.TestRequestBuilder;
import android.support.test.runner.AndroidJUnitRunner;

import dalvik.system.DexFile;

import org.chromium.base.BuildConfig;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.multidex.ChromiumMultiDexInstaller;
import org.chromium.base.test.util.InMemorySharedPreferencesContext;

import java.io.IOException;
import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * A custom AndroidJUnitRunner that supports multidex installer and list out test information.
 *
 * This class is the equivalent of BaseChromiumInstrumentationTestRunner in JUnit3. Please
 * beware that is this not a class runner. It is declared in test apk AndroidManifest.xml
 * <instrumentation>
 *
 * TODO(yolandyan): remove this class after all tests are converted to JUnit4. Use class runner
 * for test listing.
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
        if (arguments != null && arguments.getString(LIST_ALL_TESTS_FLAG) != null) {
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
            super.onStart();
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

    static boolean shouldListTests(Bundle arguments) {
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
}
