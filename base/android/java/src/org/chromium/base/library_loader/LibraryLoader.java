// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

import static org.chromium.base.metrics.CachedMetrics.EnumeratedHistogramSample;

import android.annotation.SuppressLint;
import android.content.Context;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.os.StrictMode;
import android.os.SystemClock;
import android.support.annotation.NonNull;
import android.support.v4.content.ContextCompat;
import android.system.Os;

import org.chromium.base.BuildConfig;
import org.chromium.base.BuildInfo;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.base.SysUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.compat.ApiHelperForM;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

import javax.annotation.Nullable;

/**
 * This class provides functionality to load and register the native libraries.
 * Callers are allowed to separate loading the libraries from initializing them.
 * This may be an advantage for Android Webview, where the libraries can be loaded
 * by the zygote process, but then needs per process initialization after the
 * application processes are forked from the zygote process.
 *
 * The libraries may be loaded and initialized from any thread. Synchronization
 * primitives are used to ensure that overlapping requests from different
 * threads are handled sequentially.
 *
 * See also base/android/library_loader/library_loader_hooks.cc, which contains
 * the native counterpart to this class.
 */
@MainDex
@JNINamespace("base::android")
public class LibraryLoader {
    private static final String TAG = "LibraryLoader";

    // Set to true to enable debug logs.
    private static final boolean DEBUG = false;

    // Experience shows that on some devices, the PackageManager fails to properly extract
    // native shared libraries to the /data partition at installation or upgrade time,
    // which creates all kind of chaos (https://crbug.com/806998).
    //
    // We implement a fallback when we detect the issue by manually extracting the library
    // into Chromium's own data directory, then retrying to load the new library from here.
    //
    // This will work for any device running K-. Starting with Android L, render processes
    // cannot access the file system anymore, and extraction will always fail for them.
    // However, the issue doesn't seem to appear in the field for Android L.
    //
    // Also, starting with M, the issue doesn't exist if shared libraries are stored
    // uncompressed in the APK (as Chromium does), because the system linker can access them
    // directly, and the PackageManager will thus never extract them in the first place.
    static public final boolean PLATFORM_REQUIRES_NATIVE_FALLBACK_EXTRACTION =
            Build.VERSION.SDK_INT <= VERSION_CODES.KITKAT;

    // Location of extracted native libraries.
    private static final String LIBRARY_DIR = "native_libraries";

    // SharedPreferences key for "don't prefetch libraries" flag
    private static final String DONT_PREFETCH_LIBRARIES_KEY = "dont_prefetch_libraries";

    private static final EnumeratedHistogramSample sRelinkerCountHistogram =
            new EnumeratedHistogramSample("ChromiumAndroidLinker.RelinkerFallbackCount", 2);

    // The singleton instance of LibraryLoader. Never null (not final for tests).
    private static LibraryLoader sInstance = new LibraryLoader();

    // One-way switch becomes true when the libraries are initialized (
    // by calling nativeLibraryLoaded, which forwards to LibraryLoaded(...) in
    // library_loader_hooks.cc).
    // Note that this member should remain a one-way switch, since it accessed from multiple
    // threads without a lock.
    private volatile boolean mInitialized;

    // One-way switch that becomes true once
    // {@link asyncPrefetchLibrariesToMemory} has been called.
    private final AtomicBoolean mPrefetchLibraryHasBeenCalled = new AtomicBoolean();

    // Guards all fields below.
    private final Object mLock = new Object();

    private NativeLibraryPreloader mLibraryPreloader;
    private boolean mLibraryPreloaderCalled;

    // One-way switch becomes true when the libraries are loaded.
    private boolean mLoaded;

    // One-way switch becomes true when the Java command line is switched to
    // native.
    private boolean mCommandLineSwitched;

    // One-way switches recording attempts to use Relro sharing in the browser.
    // The flags are used to report UMA stats later.
    private boolean mIsUsingBrowserSharedRelros;
    private boolean mLoadAtFixedAddressFailed;

    // One-way switch becomes true if the Chromium library was loaded from the
    // APK file directly.
    private boolean mLibraryWasLoadedFromApk;

    // The type of process the shared library is loaded in.
    private @LibraryProcessType int mLibraryProcessType;

    // The number of milliseconds it took to load all the native libraries, which
    // will be reported via UMA. Set once when the libraries are done loading.
    private long mLibraryLoadTimeMs;

    // The return value of NativeLibraryPreloader.loadLibrary(), which will be reported
    // via UMA, it is initialized to the invalid value which shouldn't showup in UMA
    // report.
    private int mLibraryPreloaderStatus = -1;

    /**
     * Call this method to determine if this chromium project must
     * use this linker. If not, System.loadLibrary() should be used to load
     * libraries instead.
     */
    public static boolean useCrazyLinker() {
        // TODO(digit): Remove this early return GVR is loadable.
        // A non-monochrome APK (such as ChromePublic.apk or ChromeModernPublic.apk) on N+ cannot
        // use the Linker because the latter is incompatible with the GVR library. Fall back
        // to using System.loadLibrary() or System.load() at the cost of no RELRO sharing.
        //
        // A non-monochrome APK (such as ChromePublic.apk) can be installed on N+ in these
        // circumstances:
        // * installing APK manually
        // * after OTA from M to N
        // * side-installing Chrome (possibly from another release channel)
        // * Play Store bugs leading to incorrect APK flavor being installed
        //
        if (Build.VERSION.SDK_INT >= VERSION_CODES.N) return false;

        // The auto-generated NativeLibraries.sUseLinker variable will be true if the
        // build has not explicitly disabled Linker features.
        return NativeLibraries.sUseLinker;
    }

    /**
     * Call this method to determine if the chromium project must load the library
     * directly from a zip file.
     */
    private static boolean isInZipFile() {
        // The auto-generated NativeLibraries.sUseLibraryInZipFile variable will be true
        // iff the library remains embedded in the APK zip file on the target.
        return NativeLibraries.sUseLibraryInZipFile;
    }

    /**
     * Set native library preloader, if set, the NativeLibraryPreloader.loadLibrary will be invoked
     * before calling System.loadLibrary, this only applies when not using the chromium linker.
     *
     * @param loader the NativeLibraryPreloader, it shall only be set once and before the
     *               native library loaded.
     */
    public void setNativeLibraryPreloader(NativeLibraryPreloader loader) {
        synchronized (mLock) {
            assert mLibraryPreloader == null && !mLoaded;
            mLibraryPreloader = loader;
        }
    }

    public static LibraryLoader getInstance() {
        return sInstance;
    }

    private LibraryLoader() {}

    /**
     *  This method blocks until the library is fully loaded and initialized.
     *
     * @param processType the process the shared library is loaded in.
     */
    public void ensureInitialized(@LibraryProcessType int processType) throws ProcessInitException {
        synchronized (mLock) {
            if (mInitialized) {
                // Already initialized, nothing to do.
                return;
            }
            loadAlreadyLocked(ContextUtils.getApplicationContext());
            initializeAlreadyLocked(processType);
        }
    }

    /**
     * Calls native library preloader (see {@link #setNativeLibraryPreloader}) with the app
     * context. If there is no preloader set, this function does nothing.
     * Preloader is called only once, so calling it explicitly via this method means
     * that it won't be (implicitly) called during library loading.
     */
    public void preloadNow() {
        preloadNowOverrideApplicationContext(ContextUtils.getApplicationContext());
    }

    /**
     * Similar to {@link #preloadNow}, but allows specifying app context to use.
     */
    public void preloadNowOverrideApplicationContext(Context appContext) {
        synchronized (mLock) {
            if (!useCrazyLinker()) {
                preloadAlreadyLocked(appContext);
            }
        }
    }

    private void preloadAlreadyLocked(Context appContext) {
        try (TraceEvent te = TraceEvent.scoped("LibraryLoader.preloadAlreadyLocked")) {
            // Preloader uses system linker, we shouldn't preload if Chromium linker is used.
            assert !useCrazyLinker();
            if (mLibraryPreloader != null && !mLibraryPreloaderCalled) {
                mLibraryPreloaderStatus = mLibraryPreloader.loadLibrary(appContext);
                mLibraryPreloaderCalled = true;
            }
        }
    }

    /**
     * Checks if library is fully loaded and initialized.
     */
    public boolean isInitialized() {
        return mInitialized;
    }

    /**
     * Loads the library and blocks until the load completes. The caller is responsible
     * for subsequently calling ensureInitialized().
     * May be called on any thread, but should only be called once. Note the thread
     * this is called on will be the thread that runs the native code's static initializers.
     * See the comment in doInBackground() for more considerations on this.
     *
     * @throws ProcessInitException if the native library failed to load.
     */
    public void loadNow() throws ProcessInitException {
        loadNowOverrideApplicationContext(ContextUtils.getApplicationContext());
    }

    /**
     * Override kept for callers that need to load from a different app context. Do not use unless
     * specifically required to load from another context that is not the current process's app
     * context.
     *
     * @param appContext The overriding app context to be used to load libraries.
     * @throws ProcessInitException if the native library failed to load with this context.
     */
    public void loadNowOverrideApplicationContext(Context appContext) throws ProcessInitException {
        synchronized (mLock) {
            if (mLoaded && appContext != ContextUtils.getApplicationContext()) {
                throw new IllegalStateException("Attempt to load again from alternate context.");
            }
            loadAlreadyLocked(appContext);
        }
    }

    /**
     * Initializes the library here and now: must be called on the thread that the
     * native will call its "main" thread. The library must have previously been
     * loaded with loadNow.
     *
     * @param processType the process the shared library is loaded in.
     */
    public void initialize(@LibraryProcessType int processType) throws ProcessInitException {
        synchronized (mLock) {
            initializeAlreadyLocked(processType);
        }
    }

    /**
     * Disables prefetching for subsequent runs. The value comes from "DontPrefetchLibraries"
     * finch experiment, and is pushed on every run. I.e. the effect of the finch experiment
     * lags by one run, which is the best we can do considering that prefetching happens way
     * before finch is initialized. Note that since LibraryLoader is in //base, it can't depend
     * on ChromeFeatureList, and has to rely on external code pushing the value.
     *
     * @param dontPrefetch whether not to prefetch libraries
     */
    public static void setDontPrefetchLibrariesOnNextRuns(boolean dontPrefetch) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(DONT_PREFETCH_LIBRARIES_KEY, dontPrefetch)
                .apply();
    }

    /**
     * @return whether not to prefetch libraries (see setDontPrefetchLibrariesOnNextRun()).
     */
    private static boolean isNotPrefetchingLibraries() {
        // This might be the first time getAppSharedPreferences() is used, so relax strict mode
        // to allow disk reads.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            return ContextUtils.getAppSharedPreferences().getBoolean(
                    DONT_PREFETCH_LIBRARIES_KEY, false);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
    }

    /** Prefetches the native libraries in a background thread.
     *
     * Launches an AsyncTask that, through a short-lived forked process, reads a
     * part of each page of the native library.  This is done to warm up the
     * page cache, turning hard page faults into soft ones.
     *
     * This is done this way, as testing shows that fadvise(FADV_WILLNEED) is
     * detrimental to the startup time.
     */
    public void asyncPrefetchLibrariesToMemory() {
        SysUtils.logPageFaultCountToTracing();
        if (isNotPrefetchingLibraries()) return;

        final boolean coldStart = mPrefetchLibraryHasBeenCalled.compareAndSet(false, true);

        // Collection should start close to the native library load, but doesn't have
        // to be simultaneous with it. Also, don't prefetch in this case, as this would
        // skew the results.
        if (coldStart && CommandLine.getInstance().hasSwitch("log-native-library-residency")) {
            // nativePeriodicallyCollectResidency() sleeps, run it on another thread,
            // and not on the AsyncTask thread pool.
            new Thread(LibraryLoader::nativePeriodicallyCollectResidency).start();
            return;
        }

        new LibraryPrefetchTask(coldStart).executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    private static class LibraryPrefetchTask extends AsyncTask<Void> {
        private final boolean mColdStart;

        public LibraryPrefetchTask(boolean coldStart) {
            mColdStart = coldStart;
        }

        @Override
        protected Void doInBackground() {
            int percentage = nativePercentageOfResidentNativeLibraryCode();
            try (TraceEvent e = TraceEvent.scoped("LibraryLoader.asyncPrefetchLibrariesToMemory",
                         Integer.toString(percentage))) {
                // Arbitrary percentage threshold. If most of the native library is already
                // resident (likely with monochrome), don't bother creating a prefetch process.
                boolean prefetch = mColdStart && percentage < 90;
                if (prefetch) {
                    nativeForkAndPrefetchNativeLibrary();
                }
                if (percentage != -1) {
                    String histogram = "LibraryLoader.PercentageOfResidentCodeBeforePrefetch"
                            + (mColdStart ? ".ColdStartup" : ".WarmStartup");
                    RecordHistogram.recordPercentageHistogram(histogram, percentage);
                }
            }
            return null;
        }
    }

    // Helper for loadAlreadyLocked(). Load a native shared library with the Chromium linker.
    // Sets UMA flags depending on the results of loading.
    private void loadLibraryWithCustomLinkerAlreadyLocked(
            Linker linker, @Nullable String zipFilePath, String libFilePath) {
        assert Thread.holdsLock(mLock);
        if (linker.isUsingBrowserSharedRelros()) {
            // If the browser is set to attempt shared RELROs then we try first with shared
            // RELROs enabled, and if that fails then retry without.
            mIsUsingBrowserSharedRelros = true;
            try {
                linker.loadLibrary(libFilePath);
            } catch (UnsatisfiedLinkError e) {
                Log.w(TAG, "Failed to load native library with shared RELRO, retrying without");
                mLoadAtFixedAddressFailed = true;
                linker.loadLibraryNoFixedAddress(libFilePath);
            }
        } else {
            // No attempt to use shared RELROs in the browser, so load as normal.
            linker.loadLibrary(libFilePath);
        }

        // Loaded successfully, so record if we loaded directly from an APK.
        if (zipFilePath != null) {
            mLibraryWasLoadedFromApk = true;
        }
    }

    static void incrementRelinkerCountHitHistogram() {
        sRelinkerCountHistogram.record(1);
    }

    static void incrementRelinkerCountNotHitHistogram() {
        sRelinkerCountHistogram.record(0);
    }

    // Experience shows that on some devices, the system sometimes fails to extract native libraries
    // at installation or update time from the APK. This function will extract the library and
    // return the extracted file path.
    static String getExtractedLibraryPath(Context appContext, String libName) {
        assert PLATFORM_REQUIRES_NATIVE_FALLBACK_EXTRACTION;
        Log.w(TAG, "Failed to load libName %s, attempting fallback extraction then trying again",
                libName);
        String libraryEntry = LibraryLoader.makeLibraryPathInZipFile(libName, false, false);
        return extractFileIfStale(appContext, libraryEntry, makeLibraryDirAndSetPermission());
    }

    // Invoke either Linker.loadLibrary(...), System.loadLibrary(...) or System.load(...),
    // triggering JNI_OnLoad in native code.
    // TODO(crbug.com/635567): Fix this properly.
    @SuppressLint({"DefaultLocale", "UnsafeDynamicallyLoadedCode"})
    private void loadAlreadyLocked(Context appContext) throws ProcessInitException {
        try (TraceEvent te = TraceEvent.scoped("LibraryLoader.loadAlreadyLocked")) {
            if (!mLoaded) {
                assert !mInitialized;

                long startTime = SystemClock.uptimeMillis();

                if (useCrazyLinker()) {
                    // Load libraries using the Chromium linker.
                    Linker linker = Linker.getInstance();

                    String apkFilePath =
                            isInZipFile() ? appContext.getApplicationInfo().sourceDir : null;
                    linker.prepareLibraryLoad(apkFilePath);

                    for (String library : NativeLibraries.LIBRARIES) {
                        // Don't self-load the linker. This is because the build system is
                        // not clever enough to understand that all the libraries packaged
                        // in the final .apk don't need to be explicitly loaded.
                        if (linker.isChromiumLinkerLibrary(library)) {
                            if (DEBUG) Log.i(TAG, "ignoring self-linker load");
                            continue;
                        }

                        // Determine where the library should be loaded from.
                        String libFilePath = System.mapLibraryName(library);
                        if (apkFilePath != null) {
                            Log.i(TAG, " Loading " + library + " from within " + apkFilePath);
                        } else {
                            Log.i(TAG, "Loading " + library);
                        }

                        try {
                            // Load the library using this Linker. May throw UnsatisfiedLinkError.
                            loadLibraryWithCustomLinkerAlreadyLocked(
                                    linker, apkFilePath, libFilePath);
                            incrementRelinkerCountNotHitHistogram();
                        } catch (UnsatisfiedLinkError e) {
                            if (!isInZipFile()
                                    && PLATFORM_REQUIRES_NATIVE_FALLBACK_EXTRACTION) {
                                loadLibraryWithCustomLinkerAlreadyLocked(
                                        linker, null, getExtractedLibraryPath(appContext, library));
                                incrementRelinkerCountHitHistogram();
                            } else {
                                Log.e(TAG, "Unable to load library: " + library);
                                throw(e);
                            }
                        }
                    }

                    linker.finishLibraryLoad();
                } else {
                    setEnvForNative();
                    preloadAlreadyLocked(appContext);

                    // If the libraries are located in the zip file, assert that the device API
                    // level is M or higher. On devices lower than M, the libraries should
                    // always be loaded by Linker.
                    assert !isInZipFile() || Build.VERSION.SDK_INT >= VERSION_CODES.M;

                    // Load libraries using the system linker.
                    for (String library : NativeLibraries.LIBRARIES) {
                        try {
                            if (!isInZipFile()) {
                                // The extract and retry logic isn't needed because this path is
                                // used only for local development.
                                System.loadLibrary(library);
                            } else {
                                // Load directly from the APK.
                                boolean is64Bit = ApiHelperForM.isProcess64Bit();
                                String zipFilePath = appContext.getApplicationInfo().sourceDir;
                                // In API level 23 and above, it’s possible to open a .so file
                                // directly from the APK of the path form
                                // "my_zip_file.zip!/libs/libstuff.so". See:
                                // https://android.googlesource.com/platform/bionic/+/master/android-changes-for-ndk-developers.md#opening-shared-libraries-directly-from-an-apk
                                String libraryName = zipFilePath + "!/"
                                        + makeLibraryPathInZipFile(library, true, is64Bit);
                                Log.i(TAG, "libraryName: " + libraryName);
                                System.load(libraryName);
                            }
                        } catch (UnsatisfiedLinkError e) {
                            Log.e(TAG, "Unable to load library: " + library);
                            throw(e);
                        }
                    }
                }

                long stopTime = SystemClock.uptimeMillis();
                mLibraryLoadTimeMs = stopTime - startTime;
                Log.i(TAG, String.format("Time to load native libraries: %d ms (timestamps %d-%d)",
                        mLibraryLoadTimeMs,
                        startTime % 10000,
                        stopTime % 10000));

                mLoaded = true;
            }
        } catch (UnsatisfiedLinkError e) {
            throw new ProcessInitException(LoaderErrors.LOADER_ERROR_NATIVE_LIBRARY_LOAD_FAILED, e);
        }
    }

    /**
     * @param library The library name that is looking for.
     * @param crazyPrefix true iff adding crazy linker prefix to the file name.
     * @param is64Bit true if the caller think it's run on a 64 bit device.
     * @return the library path name in the zip file.
     */
    @NonNull
    public static String makeLibraryPathInZipFile(
            String library, boolean crazyPrefix, boolean is64Bit) {
        // Determine the ABI string that Android uses to find native libraries. Values are described
        // in: https://developer.android.com/ndk/guides/abis.html
        // The 'armeabi' is omitted here because it is not supported in Chrome/WebView, while Cronet
        // and Cast load the native library via other paths.
        String cpuAbi;
        switch (NativeLibraries.sCpuFamily) {
            case NativeLibraries.CPU_FAMILY_ARM:
                cpuAbi = is64Bit ? "arm64-v8a" : "armeabi-v7a";
                break;
            case NativeLibraries.CPU_FAMILY_X86:
                cpuAbi = is64Bit ? "x86_64" : "x86";
                break;
            case NativeLibraries.CPU_FAMILY_MIPS:
                cpuAbi = is64Bit ? "mips64" : "mips";
                break;
            default:
                throw new RuntimeException("Unknown CPU ABI for native libraries");
        }

        // When both the Chromium linker and zip-uncompressed native libraries are used,
        // the build system renames the native shared libraries with a 'crazy.' prefix
        // (e.g. "/lib/armeabi-v7a/libfoo.so" -> "/lib/armeabi-v7a/crazy.libfoo.so").
        //
        // This prevents the package manager from extracting them at installation/update time
        // to the /data directory. The libraries can still be accessed directly by the Chromium
        // linker from the APK.
        String crazyPart = crazyPrefix ? "crazy." : "";
        return String.format("lib/%s/%s%s", cpuAbi, crazyPart, System.mapLibraryName(library));
    }

    // The WebView requires the Command Line to be switched over before
    // initialization is done. This is okay in the WebView's case since the
    // JNI is already loaded by this point.
    public void switchCommandLineForWebView() {
        synchronized (mLock) {
            ensureCommandLineSwitchedAlreadyLocked();
        }
    }

    // Switch the CommandLine over from Java to native if it hasn't already been done.
    // This must happen after the code is loaded and after JNI is ready (since after the
    // switch the Java CommandLine will delegate all calls the native CommandLine).
    private void ensureCommandLineSwitchedAlreadyLocked() {
        assert mLoaded;
        if (mCommandLineSwitched) {
            return;
        }
        CommandLine.enableNativeProxy();
        mCommandLineSwitched = true;
    }

    // Invoke base::android::LibraryLoaded in library_loader_hooks.cc
    private void initializeAlreadyLocked(@LibraryProcessType int processType)
            throws ProcessInitException {
        if (mInitialized) {
            if (mLibraryProcessType != processType) {
                throw new ProcessInitException(
                        LoaderErrors.LOADER_ERROR_NATIVE_LIBRARY_LOAD_FAILED);
            }
            return;
        }
        mLibraryProcessType = processType;

        ensureCommandLineSwitchedAlreadyLocked();

        if (!nativeLibraryLoaded(mLibraryProcessType)) {
            Log.e(TAG, "error calling nativeLibraryLoaded");
            throw new ProcessInitException(LoaderErrors.LOADER_ERROR_FAILED_TO_REGISTER_JNI);
        }

        // Check that the version of the library we have loaded matches the version we expect
        Log.i(TAG, String.format("Expected native library version number \"%s\", "
                                   + "actual native library version number \"%s\"",
                           NativeLibraries.sVersionNumber, nativeGetVersionNumber()));
        if (!NativeLibraries.sVersionNumber.equals(nativeGetVersionNumber())) {
            throw new ProcessInitException(LoaderErrors.LOADER_ERROR_NATIVE_LIBRARY_WRONG_VERSION);
        }

        // From now on, keep tracing in sync with native.
        TraceEvent.registerNativeEnabledObserver();

        if (processType == LibraryProcessType.PROCESS_BROWSER
                && PLATFORM_REQUIRES_NATIVE_FALLBACK_EXTRACTION) {
            // Perform the detection and deletion of obsolete native libraries on a background
            // background thread.
            AsyncTask.THREAD_POOL_EXECUTOR.execute(new Runnable() {
                @Override
                public void run() {
                    final String suffix = BuildInfo.getInstance().extractedFileSuffix;
                    final File[] files = getLibraryDir().listFiles();
                    if (files == null) return;

                    for (File file : files) {
                        // NOTE: Do not simply look for <suffix> at the end of the file.
                        //
                        // Extracted library files have names like 'libfoo.so<suffix>', but
                        // extractFileIfStale() will use FileUtils.copyFileStreamAtomicWithBuffer()
                        // to create them, and this method actually uses a transient temporary file
                        // named like 'libfoo.so<suffix>.tmp' to do that. These temporary files, if
                        // detected here, should be preserved; hence the reason why contains() is
                        // used below.
                        if (!file.getName().contains(suffix)) {
                            String fileName = file.getName();
                            if (!file.delete()) {
                                Log.w(TAG, "Unable to remove %s", fileName);
                            } else {
                                Log.i(TAG, "Removed obsolete file %s", fileName);
                            }
                        }
                    }
                }
            });
        }

        // From this point on, native code is ready to use and checkIsReady()
        // shouldn't complain from now on (and in fact, it's used by the
        // following calls).
        // Note that this flag can be accessed asynchronously, so any initialization
        // must be performed before.
        mInitialized = true;
    }

    // Called after all native initializations are complete.
    public void onNativeInitializationComplete() {
        synchronized (mLock) {
            recordBrowserProcessHistogramAlreadyLocked();
        }
    }

    // Record Chromium linker histogram state for the main browser process. Called from
    // onNativeInitializationComplete().
    private void recordBrowserProcessHistogramAlreadyLocked() {
        assert Thread.holdsLock(mLock);
        if (useCrazyLinker()) {
            nativeRecordChromiumAndroidLinkerBrowserHistogram(mIsUsingBrowserSharedRelros,
                    mLoadAtFixedAddressFailed,
                    mLibraryWasLoadedFromApk ? LibraryLoadFromApkStatusCodes.SUCCESSFUL
                                             : LibraryLoadFromApkStatusCodes.UNKNOWN,
                    mLibraryLoadTimeMs);
        }
        if (mLibraryPreloader != null) {
            nativeRecordLibraryPreloaderBrowserHistogram(mLibraryPreloaderStatus);
        }
    }

    // Register pending Chromium linker histogram state for renderer processes. This cannot be
    // recorded as a histogram immediately because histograms and IPC are not ready at the
    // time it are captured. This function stores a pending value, so that a later call to
    // RecordChromiumAndroidLinkerRendererHistogram() will record it correctly.
    public void registerRendererProcessHistogram(boolean requestedSharedRelro,
                                                 boolean loadAtFixedAddressFailed) {
        synchronized (mLock) {
            if (useCrazyLinker()) {
                nativeRegisterChromiumAndroidLinkerRendererHistogram(
                        requestedSharedRelro, loadAtFixedAddressFailed, mLibraryLoadTimeMs);
            }
            if (mLibraryPreloader != null) {
                nativeRegisterLibraryPreloaderRendererHistogram(mLibraryPreloaderStatus);
            }
        }
    }

    /**
     * Override the library loader (normally with a mock) for testing.
     * @param loader the mock library loader.
     */
    @VisibleForTesting
    public static void setLibraryLoaderForTesting(LibraryLoader loader) {
        sInstance = loader;
    }

    /**
     * Configure ubsan using $UBSAN_OPTIONS. This function needs to be called before any native
     * libraries are loaded because ubsan reads its configuration from $UBSAN_OPTIONS when the
     * native library is loaded.
     */
    public static void setEnvForNative() {
        // The setenv API was added in L. On older versions of Android, we should still see ubsan
        // reports, but they will not have stack traces.
        if (BuildConfig.IS_UBSAN && Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            try {
                // This value is duplicated in build/android/pylib/constants/__init__.py.
                Os.setenv("UBSAN_OPTIONS",
                        "print_stacktrace=1 stack_trace_format='#%n pc %o %m' "
                                + "handle_segv=0 handle_sigbus=0 handle_sigfpe=0",
                        true);
            } catch (Exception e) {
                Log.w(TAG, "failed to set UBSAN_OPTIONS", e);
            }
        }
    }

    // Android system sometimes fails to extract libraries from APK (https://crbug.com/806998).
    // This function manually extract libraries as a fallback.
    @SuppressLint({"SetWorldReadable"})
    private static String extractFileIfStale(
            Context appContext, String pathWithinApk, File destDir) {
        assert PLATFORM_REQUIRES_NATIVE_FALLBACK_EXTRACTION;

        String apkPath = appContext.getApplicationInfo().sourceDir;
        String fileName =
                (new File(pathWithinApk)).getName() + BuildInfo.getInstance().extractedFileSuffix;
        File libraryFile = new File(destDir, fileName);

        if (!libraryFile.exists()) {
            ZipFile zipFile = null;
            try {
                zipFile = new ZipFile(apkPath);
                ZipEntry zipEntry = zipFile.getEntry(pathWithinApk);
                if (zipEntry == null)
                    throw new RuntimeException("Cannot find ZipEntry" + pathWithinApk);
                InputStream inputStream = zipFile.getInputStream(zipEntry);

                FileUtils.copyFileStreamAtomicWithBuffer(
                        inputStream, libraryFile, new byte[16 * 1024]);
                libraryFile.setReadable(true, false);
                libraryFile.setExecutable(true, false);
            } catch (IOException e) {
                throw new RuntimeException(e);
            } finally {
                StreamUtil.closeQuietly(zipFile);
            }
        }
        return libraryFile.getAbsolutePath();
    }

    // Ensure the extracted native libraries is created with the right permissions.
    private static File makeLibraryDirAndSetPermission() {
        if (!ContextUtils.isIsolatedProcess()) {
            File cacheDir = ContextCompat.getCodeCacheDir(ContextUtils.getApplicationContext());
            File libDir = new File(cacheDir, LIBRARY_DIR);
            cacheDir.mkdir();
            cacheDir.setExecutable(true, false);
            libDir.mkdir();
            libDir.setExecutable(true, false);
        }
        return getLibraryDir();
    }

    // Return File object for the directory containing extracted native libraries.
    private static File getLibraryDir() {
        return new File(
                ContextCompat.getCodeCacheDir(ContextUtils.getApplicationContext()), LIBRARY_DIR);
    }

    // Only methods needed before or during normal JNI registration are during System.OnLoad.
    // nativeLibraryLoaded is then called to register everything else.  This process is called
    // "initialization".  This method will be mapped (by generated code) to the LibraryLoaded
    // definition in base/android/library_loader/library_loader_hooks.cc.
    //
    // Return true on success and false on failure.
    private native boolean nativeLibraryLoaded(@LibraryProcessType int processType);

    // Method called to record statistics about the Chromium linker operation for the main
    // browser process. Indicates whether the linker attempted relro sharing for the browser,
    // and if it did, whether the library failed to load at a fixed address. Also records
    // support for loading a library directly from the APK file, and the number of milliseconds
    // it took to load the libraries.
    private native void nativeRecordChromiumAndroidLinkerBrowserHistogram(
            boolean isUsingBrowserSharedRelros,
            boolean loadAtFixedAddressFailed,
            int libraryLoadFromApkStatus,
            long libraryLoadTime);

    // Method called to record the return value of NativeLibraryPreloader.loadLibrary for the main
    // browser process.
    private native void nativeRecordLibraryPreloaderBrowserHistogram(int status);

    // Method called to register (for later recording) statistics about the Chromium linker
    // operation for a renderer process. Indicates whether the linker attempted relro sharing,
    // and if it did, whether the library failed to load at a fixed address. Also records the
    // number of milliseconds it took to load the libraries.
    private native void nativeRegisterChromiumAndroidLinkerRendererHistogram(
            boolean requestedSharedRelro,
            boolean loadAtFixedAddressFailed,
            long libraryLoadTime);

    // Method called to register (for later recording) the return value of
    // NativeLibraryPreloader.loadLibrary for a renderer process.
    private native void nativeRegisterLibraryPreloaderRendererHistogram(int status);

    // Get the version of the native library. This is needed so that we can check we
    // have the right version before initializing the (rest of the) JNI.
    private native String nativeGetVersionNumber();

    // Finds the ranges corresponding to the native library pages, forks a new
    // process to prefetch these pages and waits for it. The new process then
    // terminates. This is blocking.
    private static native void nativeForkAndPrefetchNativeLibrary();

    // Returns the percentage of the native library code page that are currently reseident in
    // memory.
    private static native int nativePercentageOfResidentNativeLibraryCode();

    // Periodically logs native library residency from this thread.
    private static native void nativePeriodicallyCollectResidency();
}
