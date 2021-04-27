// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.ApplicationInfo;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.os.Bundle;
import android.os.SystemClock;
import android.system.Os;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.JNIUtils;
import org.chromium.base.Log;
import org.chromium.base.NativeLibraryLoadedStatus;
import org.chromium.base.NativeLibraryLoadedStatus.NativeLibraryLoadedStatusProvider;
import org.chromium.base.StrictModeContext;
import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.CheckDiscard;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.compat.ApiHelperForM;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.build.BuildConfig;
import org.chromium.build.NativeLibraries;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Locale;

import javax.annotation.concurrent.GuardedBy;
import javax.annotation.concurrent.ThreadSafe;

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

    // Shared preferences key for the reached code profiler.
    private static final String DEPRECATED_REACHED_CODE_PROFILER_KEY =
            "reached_code_profiler_enabled";
    private static final String REACHED_CODE_SAMPLING_INTERVAL_KEY =
            "reached_code_sampling_interval";

    // Default sampling interval for reached code profiler in microseconds.
    private static final int DEFAULT_REACHED_CODE_SAMPLING_INTERVAL_US = 10000;

    // Shared preferences key for the background thread pool setting.
    private static final String BACKGROUND_THREAD_POOL_KEY = "background_thread_pool_enabled";

    // The singleton instance of LibraryLoader. Never null (not final for tests).
    private static LibraryLoader sInstance = new LibraryLoader();

    // One-way switch becomes true when the libraries are initialized (by calling
    // LibraryLoaderJni.get().libraryLoaded, which forwards to LibraryLoaded(...) in
    // library_loader_hooks.cc). Note that this member should remain a one-way switch, since it
    // accessed from multiple threads without a lock.
    private volatile boolean mInitialized;

    // State that only transitions one-way from 0->1->2. Volatile for the same reasons as
    // mInitialized.
    @IntDef({LoadState.NOT_LOADED, LoadState.MAIN_DEX_LOADED, LoadState.LOADED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface LoadState {
        int NOT_LOADED = 0;
        int MAIN_DEX_LOADED = 1;
        int LOADED = 2;
    }
    private volatile @LoadState int mLoadState;

    // Whether to use the Chromium linker vs. the system linker.
    // Avoids locking: should be initialized very early.
    private boolean mUseChromiumLinker;

    // Whether to use ModernLinker vs. LegacyLinker.
    // Avoids locking: should be initialized very early.
    private boolean mUseModernLinker;

    // Whether the |mUseChromiumLinker| and |mUseModernLinker| configuration has been set.
    // Avoids locking: should be initialized very early.
    private boolean mConfigurationSet;

    // The type of process the shared library is loaded in. Gets passed to native after loading.
    // Avoids locking: should be initialized very early.
    private @LibraryProcessType int mLibraryProcessType;

    // Makes sure non-Main Dex initialization happens only once. Does not use any class members
    // except the volatile |mLoadState|.
    private final Object mNonMainDexLock = new Object();

    // Mediates all communication between Linker instances in different processes.
    private final MultiProcessMediator mMediator = new MultiProcessMediator();

    // Guards all the fields below.
    private final Object mLock = new Object();

    // When a Chromium linker is used, this field represents the concrete class serving as a Linker.
    // Always accessed via getLinker() because the choice of the class can be influenced by
    // public setLinkerImplementation() below.
    @GuardedBy("mLock")
    private Linker mLinker;

    @GuardedBy("mLock")
    private NativeLibraryPreloader mLibraryPreloader;

    @GuardedBy("mLock")
    private boolean mLibraryPreloaderCalled;

    // Similar to |mLoadState| but is limited case of being loaded in app zygote.
    // This is exposed to clients.
    @GuardedBy("mLock")
    private boolean mLoadedByZygote;

    // One-way switch becomes true when the Java command line is switched to
    // native.
    @GuardedBy("mLock")
    private boolean mCommandLineSwitched;

    // The number of milliseconds it took to load all the native libraries, which will be reported
    // via UMA. Set once when the libraries are done loading.
    @GuardedBy("mLock")
    private long mLibraryLoadTimeMs;

    /**
     * Inner class encapsulating points of communication between instances of LibraryLoader in
     * different processes.
     *
     * Usage:
     *
     * - For a {@link LibraryLoader} requiring the knowledge of the load address before
     *   initialization, {@link #takeLoadAddressFromBundle(Bundle)} should be called first. It is
     *   done very early after establishing a Binder connection.
     *
     * - To initialize the object, one of {@link #ensureInitializedInMainProcess()} and
     *   {@link #initInChildProcess()} must be called. Subsequent calls to initialization are
     *   ignored.
     *
     * - Later  {@link #putLoadAddressToBundle(Bundle)} and
     *   {@link #takeLoadAddressFromBundle(Bundle)} should be called for passing the RELRO
     *   information between library loaders.
     *
     * Internally the {@LibraryLoader} may ignore these messages because it can fall back to not
     * sharing RELRO.
     */
    @ThreadSafe
    public class MultiProcessMediator {
        @GuardedBy("mLock")
        private long mLoadAddress;

        // Used only for asserts, and only ever switched from false to true.
        private volatile boolean mInitDone;

        /**
         * Extracts the load address as provided by another process.
         * @param bundle The Bundle to extract from.
         */
        public void takeLoadAddressFromBundle(Bundle bundle) {
            // Currently clients call this method strictly before any other method can get executed
            // on a different thread. Hence, synchronization is not required, but verification of
            // correctness is still non-trivial, and over-synchronization is cheap compared to
            // library loading.
            synchronized (mLock) {
                mLoadAddress = Linker.extractLoadAddressFromBundle(bundle);
            }
        }

        /**
         * Initializes the Browser process side of communication, the one that coordinates creation
         * of other processes. Can be called more than once, subsequent calls are ignored.
         */
        public void ensureInitializedInMainProcess() {
            if (mInitDone) return;
            if (useChromiumLinker()) {
                getLinker().initAsRelroProducer();
            }
            mInitDone = true;
        }

        /**
         * Serializes the load address for communication, if any was determined during
         * initialization. Must be called after the library has been loaded in this process.
         * @param bundle Bundle to put the address to.
         */
        public void putLoadAddressToBundle(Bundle bundle) {
            assert mInitDone;
            if (useChromiumLinker()) {
                getLinker().putLoadAddressToBundle(bundle);
            }
        }

        /**
         * Initializes in processes other than "Main".
         */
        public void initInChildProcess() {
            if (useChromiumLinker()) {
                synchronized (mLock) {
                    getLinker().initAsRelroConsumer(mLoadAddress);
                }
            }
            mInitDone = true;
        }

        /**
         * Optionally extracts RELRO and saves it for replacing the RELRO section in this process.
         * Can be invoked before initialization.
         * @param bundle Where to deserialize from.
         */
        public void takeSharedRelrosFromBundle(Bundle bundle) {
            if (useChromiumLinker() && !isLoadedByZygote()) {
                getLinker().takeSharedRelrosFromBundle(bundle);
            }
        }

        /**
         * Optionally puts the RELRO section information so that it can be memory-mapped in another
         * process reading the bundle.
         * @param bundle Where to serialize.
         */
        public void putSharedRelrosToBundle(Bundle bundle) {
            assert mInitDone;
            if (useChromiumLinker()) {
                getLinker().putSharedRelrosToBundle(bundle);
            }
        }
    }

    public final MultiProcessMediator getMediator() {
        return mMediator;
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

    public static LibraryLoader getInstance() {
        return sInstance;
    }

    @VisibleForTesting
    protected LibraryLoader() {}

    /**
     * Set the {@Link LibraryProcessType} for this process.
     *
     * Since this function is called extremely early on in startup, locking is not required.
     *
     * @param type the process type.
     */
    public void setLibraryProcessType(@LibraryProcessType int type) {
        assert type != LibraryProcessType.PROCESS_UNINITIALIZED;
        if (type == mLibraryProcessType) return;
        if (mLibraryProcessType != LibraryProcessType.PROCESS_UNINITIALIZED) {
            throw new IllegalStateException(
                    String.format("Trying to change the LibraryProcessType from %d to %d",
                            mLibraryProcessType, type));
        }
        mLibraryProcessType = type;
    }

    /**
     * Set native library preloader, if set, the NativeLibraryPreloader.loadLibrary will be invoked
     * before calling System.loadLibrary, this only applies when not using the chromium linker.
     *
     * @param loader the NativeLibraryPreloader, it shall only be set once and before the
     *               native library is loaded.
     */
    public void setNativeLibraryPreloader(NativeLibraryPreloader loader) {
        synchronized (mLock) {
            assert mLibraryPreloader == null;
            assert mLoadState == LoadState.NOT_LOADED;
            mLibraryPreloader = loader;
        }
    }

    /**
     * Sets the configuration for library loading.
     *
     * Must be called before loading the library. Since this function is called extremely early on
     * in startup, locking is not required.
     *
     * @param useChromiumLinker Whether to use a chromium linker.
     * @param useModernLinker Given that one of the Chromium linkers is used, whether to use
     *                        ModernLinker instead of the LegacyLinker.
     */
    public void setLinkerImplementation(boolean useChromiumLinker, boolean useModernLinker) {
        assert !mInitialized;

        mUseChromiumLinker = useChromiumLinker;
        mUseModernLinker = useModernLinker;

        Log.d(TAG, "Configuration: useChromiumLinker() = %b, mUseModernLinker = %b",
                useChromiumLinker(), mUseModernLinker);
        mConfigurationSet = true;
    }

    @GuardedBy("mLock")
    private void setLinkerImplementationIfNeededAlreadyLocked() {
        if (mConfigurationSet) return;

        // Cannot use initial values for the fields below, as this makes robolectric tests fail,
        // since they don't have a NativeLibraries class.
        mUseChromiumLinker = NativeLibraries.sUseLinker;
        mUseModernLinker = NativeLibraries.sUseModernLinker;
        mConfigurationSet = true;
    }

    // LegacyLinker is buggy on Android 10, causing crashes (see crbug.com/980304).
    //
    // Rather than preventing people from running chrome_public_apk on Android 10, fallback to the
    // system linker on this platform. We lose relocation sharing as a side-effect, but this
    // configuration does not ship to users (since we only use LegacyLinker for APKs targeted at
    // pre-N users).
    //
    // Note: This cannot be done in the build configuration, as otherwise chrome_public_apk cannot
    // both be used as the basis to ship on L, and the default APK used by developers on 10+.
    private boolean forceSystemLinker() {
        boolean result =
                mUseChromiumLinker && !mUseModernLinker && Build.VERSION.SDK_INT >= VERSION_CODES.Q;
        if (result) {
            Log.d(TAG,
                    "Forcing system linker, relocations will not be shared. "
                            + "This negatively impacts memory usage.");
        }
        return result;
    }

    private boolean useChromiumLinker() {
        return mUseChromiumLinker && !forceSystemLinker();
    }

    /**
     * Returns either a LegacyLinker or a ModernLinker.
     *
     * ModernLinker requires OS features from Android M and later: a system linker that handles
     * packed relocations and load from APK, and |android_dlopen_ext()| for shared RELRO support. It
     * cannot run on Android releases earlier than M.
     *
     * LegacyLinker runs on all Android releases but it is slower and more complex than
     * ModernLinker. The LegacyLinker is used on M as it avoids writing the relocation to disk.
     *
     * On N, O and P Monochrome is selected by Play Store. With Monochrome this code is not used,
     * instead Chrome asks the WebView to provide the library (and the shared RELRO). If the WebView
     * fails to provide the library, the system linker is used as a fallback.
     *
     * LegacyLinker can run on all Android releases, but is unused on P+ as it may cause issues.
     * LegacyLinker is preferred on M- because it does not write the shared RELRO to disk at
     * almost every cold startup.
     *
     * Finally, ModernLinker is used on Android Q+ with Trichrome.
     *
     * More: docs/android_native_libraries.md
     *
     * @return the Linker implementation instance.
     */
    private Linker getLinker() {
        // A non-monochrome APK (such as ChromePublic.apk) can be installed on N+ in these
        // circumstances:
        // * installing APK manually
        // * after OTA from M to N
        // * side-installing Chrome (possibly from another release channel)
        // * Play Store bugs leading to incorrect APK flavor being installed
        // * installing other Chromium-based browsers
        //
        // For Chrome builds regularly shipped to users on N+, the system linker (or the Android
        // Framework) provides the necessary functionality to load without crazylinker. The
        // LegacyLinker is risky to auto-enable on newer Android releases, as it may interfere with
        // regular library loading. See http://crbug.com/980304 as example.
        //
        // This is only called if LibraryLoader.useChromiumLinker() returns true, meaning this is
        // either Chrome{,Modern} or Trichrome.
        synchronized (mLock) {
            if (mLinker == null) {
                mLinker = mUseModernLinker ? new ModernLinker() : new LegacyLinker();
                Log.i(TAG, "Using linker: %s", mLinker.getClass().getName());
            }
            return mLinker;
        }
    }

    @CheckDiscard(
            "Can't use @RemovableInRelease because Release build with ENABLE_ASSERTS needs it")
    public void enableJniChecks() {
        if (!BuildConfig.ENABLE_ASSERTS) return;

        NativeLibraryLoadedStatus.setProvider(new NativeLibraryLoadedStatusProvider() {
            @Override
            public boolean areMainDexNativeMethodsReady() {
                return mLoadState >= LoadState.MAIN_DEX_LOADED;
            }

            @Override
            public boolean areNativeMethodsReady() {
                return isInitialized();
            }
        });
    }

    /**
     * Return if library is already loaded successfully by the zygote.
     */
    public boolean isLoadedByZygote() {
        synchronized (mLock) {
            return mLoadedByZygote;
        }
    }

    /**
     *  This method blocks until the library is fully loaded and initialized.
     */
    public void ensureInitialized() {
        if (isInitialized()) return;
        ensureMainDexInitialized();
        loadNonMainDex();
    }

    /**
     * This method blocks until the native library is initialized, and the Main Dex is loaded
     * (MainDex JNI is registered).
     *
     * You should use this if you would like to use isolated parts of the native library that don't
     * depend on content initialization, and only use MainDex classes with JNI.
     *
     * However, you should be careful not to call this too early in startup on the UI thread, or you
     * may significantly increase the time to first draw.
     */
    public void ensureMainDexInitialized() {
        synchronized (mLock) {
            loadMainDexAlreadyLocked(
                    ContextUtils.getApplicationContext().getApplicationInfo(), false);
            initializeAlreadyLocked();
        }
    }

    /**
     * Calls native library preloader (see {@link #setNativeLibraryPreloader}) with the app
     * context. If there is no preloader set, this function does nothing.
     * Preloader is called only once, so calling it explicitly via this method means
     * that it won't be (implicitly) called during library loading.
     */
    public void preloadNow() {
        preloadNowOverridePackageName(
                ContextUtils.getApplicationContext().getApplicationInfo().packageName);
    }

    /**
     * Similar to {@link #preloadNow}, but allows specifying app context to use.
     */
    public void preloadNowOverridePackageName(String packageName) {
        synchronized (mLock) {
            setLinkerImplementationIfNeededAlreadyLocked();
            if (useChromiumLinker()) return;
            preloadAlreadyLocked(packageName, false /* inZygote */);
        }
    }

    @GuardedBy("mLock")
    private void preloadAlreadyLocked(String packageName, boolean inZygote) {
        try (TraceEvent te = TraceEvent.scoped("LibraryLoader.preloadAlreadyLocked")) {
            // Preloader uses system linker, we shouldn't preload if Chromium linker is used.
            assert !useChromiumLinker() || inZygote;
            if (mLibraryPreloader != null && !mLibraryPreloaderCalled) {
                mLibraryPreloader.loadLibrary(packageName);
                mLibraryPreloaderCalled = true;
            }
        }
    }

    /**
     * Checks whether the native library is fully loaded and initialized.
     */
    public boolean isInitialized() {
        return mInitialized && mLoadState == LoadState.LOADED;
    }

    /**
     * Loads the library and blocks until the load completes. The caller is responsible for
     * subsequently calling ensureInitialized(). May be called on any thread, but should only be
     * called once.
     */
    public void loadNow() {
        loadNowOverrideApplicationContext(ContextUtils.getApplicationContext());
    }

    /**
     * Override kept for callers that need to load from a different app context. Do not use unless
     * specifically required to load from another context that is not the current process's app
     * context.
     *
     * @param appContext The overriding app context to be used to load libraries.
     */
    public void loadNowOverrideApplicationContext(Context appContext) {
        synchronized (mLock) {
            if (mLoadState != LoadState.NOT_LOADED
                    && appContext != ContextUtils.getApplicationContext()) {
                throw new IllegalStateException("Attempt to load again from alternate context.");
            }
            loadMainDexAlreadyLocked(appContext.getApplicationInfo(), false /* inZygote */);
        }
        loadNonMainDex();
    }

    public void loadNowInZygote(ApplicationInfo appInfo) {
        synchronized (mLock) {
            assert mLoadState == LoadState.NOT_LOADED;
            loadMainDexAlreadyLocked(appInfo, true /* inZygote */);
            loadNonMainDex();
            mLoadedByZygote = true;
        }
    }

    /**
     * Initializes the native library: must be called on the thread that the
     * native will call its "main" thread. The library must have previously been
     * loaded with one of the loadNow*() variants.
     */
    public void initialize() {
        synchronized (mLock) {
            initializeAlreadyLocked();
        }
    }

    /**
     * Enables the reached code profiler. The value comes from "ReachedCodeProfiler"
     * finch experiment, and is pushed on every run. I.e. the effect of the finch experiment
     * lags by one run, which is the best we can do considering that the profiler has to be enabled
     * before finch is initialized. Note that since LibraryLoader is in //base, it can't depend
     * on ChromeFeatureList, and has to rely on external code pushing the value.
     *
     * @param enabled whether to enable the reached code profiler.
     * @param samplingIntervalUs the sampling interval for reached code profiler.
     */
    public static void setReachedCodeProfilerEnabledOnNextRuns(
            boolean enabled, int samplingIntervalUs) {
        // Store 0 if the profiler is not enabled, otherwise store the sampling interval in
        // microseconds.
        if (enabled && samplingIntervalUs == 0) {
            samplingIntervalUs = DEFAULT_REACHED_CODE_SAMPLING_INTERVAL_US;
        } else if (!enabled) {
            samplingIntervalUs = 0;
        }
        SharedPreferences.Editor editor = ContextUtils.getAppSharedPreferences().edit();
        editor.remove(DEPRECATED_REACHED_CODE_PROFILER_KEY);
        editor.putInt(REACHED_CODE_SAMPLING_INTERVAL_KEY, samplingIntervalUs).apply();
    }

    /**
     * @return sampling interval for reached code profiler, or 0 when the profiler is disabled. (see
     *         setReachedCodeProfilerEnabledOnNextRuns()).
     */
    @VisibleForTesting
    public static int getReachedCodeSamplingIntervalUs() {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            if (ContextUtils.getAppSharedPreferences().getBoolean(
                        DEPRECATED_REACHED_CODE_PROFILER_KEY, false)) {
                return DEFAULT_REACHED_CODE_SAMPLING_INTERVAL_US;
            }
            return ContextUtils.getAppSharedPreferences().getInt(
                    REACHED_CODE_SAMPLING_INTERVAL_KEY, 0);
        }
    }

    /**
     * Enables the background priority thread pool group. The value comes from the
     * "BackgroundThreadPool" finch experiment, and is pushed on every run, to take effect on the
     * subsequent run. I.e. the effect of the finch experiment lags by one run, which is the best we
     * can do considering that the thread pool has to be configured before finch is initialized.
     * Note that since LibraryLoader is in //base, it can't depend on ChromeFeatureList, and has to
     * rely on external code pushing the value.
     *
     * @param enabled whether to enable the background priority thread pool group.
     */
    public static void setBackgroundThreadPoolEnabledOnNextRuns(boolean enabled) {
        SharedPreferences.Editor editor = ContextUtils.getAppSharedPreferences().edit();
        editor.putBoolean(BACKGROUND_THREAD_POOL_KEY, enabled).apply();
    }

    /**
     * @return whether the background priority thread pool group should be enabled. (see
     *         setBackgroundThreadPoolEnabledOnNextRuns()).
     */
    @VisibleForTesting
    public static boolean isBackgroundThreadPoolEnabled() {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            return ContextUtils.getAppSharedPreferences().getBoolean(
                    BACKGROUND_THREAD_POOL_KEY, false);
        }
    }

    private void loadWithChromiumLinker(ApplicationInfo appInfo, String library) {
        Linker linker = getLinker();

        if (isInZipFile()) {
            String sourceDir = appInfo.sourceDir;
            linker.setApkFilePath(sourceDir);
            Log.i(TAG, "Loading %s from within %s", library, sourceDir);
        } else {
            Log.i(TAG, "Loading %s", library);
        }

        linker.loadLibrary(library); // May throw UnsatisfiedLinkError.
    }

    @GuardedBy("mLock")
    @SuppressLint("UnsafeDynamicallyLoadedCode")
    private void loadWithSystemLinkerAlreadyLocked(ApplicationInfo appInfo, boolean inZygote) {
        setEnvForNative();
        preloadAlreadyLocked(appInfo.packageName, inZygote);

        // If the libraries are located in the zip file, assert that the device API level is M or
        // higher. On devices <=M, the libraries should always be loaded by LegacyLinker.
        assert !isInZipFile() || Build.VERSION.SDK_INT >= VERSION_CODES.M;

        // Load libraries using the system linker.
        for (String library : NativeLibraries.LIBRARIES) {
            if (!isInZipFile()) {
                System.loadLibrary(library);
            } else {
                // Load directly from the APK.
                boolean is64Bit = ApiHelperForM.isProcess64Bit();
                String zipFilePath = appInfo.sourceDir;
                boolean crazyPrefix = forceSystemLinker(); // See comment in this function.
                String fullPath = zipFilePath + "!/"
                        + makeLibraryPathInZipFile(library, crazyPrefix, is64Bit);
                Log.i(TAG, "libraryName: %s", fullPath);
                System.load(fullPath);
            }
        }
    }

    // Invoke either Linker.loadLibrary(...), System.loadLibrary(...) or System.load(...),
    // triggering JNI_OnLoad in native code.
    @GuardedBy("mLock")
    @VisibleForTesting
    protected void loadMainDexAlreadyLocked(ApplicationInfo appInfo, boolean inZygote) {
        if (mLoadState >= LoadState.MAIN_DEX_LOADED) return;
        try (TraceEvent te = TraceEvent.scoped("LibraryLoader.loadMainDexAlreadyLocked")) {
            assert !mInitialized;
            assert mLibraryProcessType != LibraryProcessType.PROCESS_UNINITIALIZED || inZygote;
            setLinkerImplementationIfNeededAlreadyLocked();

            long startTime = SystemClock.uptimeMillis();

            if (useChromiumLinker() && !inZygote) {
                Log.d(TAG, "Loading with the Chromium linker.");
                // See base/android/linker/config.gni, the chromium linker is only enabled when
                // we have a single library.
                assert NativeLibraries.LIBRARIES.length == 1;
                String library = NativeLibraries.LIBRARIES[0];
                loadWithChromiumLinker(appInfo, library);
            } else {
                Log.d(TAG, "Loading with the System linker.");
                loadWithSystemLinkerAlreadyLocked(appInfo, inZygote);
            }

            long stopTime = SystemClock.uptimeMillis();
            mLibraryLoadTimeMs = stopTime - startTime;
            Log.d(TAG, "Time to load native libraries: %d ms", mLibraryLoadTimeMs);

            mLoadState = LoadState.MAIN_DEX_LOADED;
        } catch (UnsatisfiedLinkError e) {
            throw new ProcessInitException(LoaderErrors.NATIVE_LIBRARY_LOAD_FAILED, e);
        }
    }

    @VisibleForTesting
    // After Android M, this function is likely a no-op.
    protected void loadNonMainDex() {
        if (mLoadState == LoadState.LOADED) return;
        synchronized (mNonMainDexLock) {
            assert mLoadState != LoadState.NOT_LOADED;
            if (mLoadState == LoadState.LOADED) return;
            try (TraceEvent te = TraceEvent.scoped("LibraryLoader.loadNonMainDex")) {
                if (!JNIUtils.isSelectiveJniRegistrationEnabled()) {
                    LibraryLoaderJni.get().registerNonMainDexJni();
                }
                mLoadState = LoadState.LOADED;
            }
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
        return String.format(
                Locale.US, "lib/%s/%s%s", cpuAbi, crazyPart, System.mapLibraryName(library));
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
    @GuardedBy("mLock")
    private void ensureCommandLineSwitchedAlreadyLocked() {
        assert mLoadState >= LoadState.MAIN_DEX_LOADED;
        if (mCommandLineSwitched) {
            return;
        }
        CommandLine.enableNativeProxy();
        mCommandLineSwitched = true;
    }

    /**
     * Assert that library process type in the LibraryLoader is compatible with provided type.
     *
     * @param libraryProcessType a library process type to assert.
     */
    public void assertCompatibleProcessType(@LibraryProcessType int libraryProcessType) {
        assert libraryProcessType == mLibraryProcessType;
    }

    // Invoke base::android::LibraryLoaded in library_loader_hooks.cc
    @GuardedBy("mLock")
    private void initializeAlreadyLocked() {
        if (mInitialized) return;
        assert mLibraryProcessType != LibraryProcessType.PROCESS_UNINITIALIZED;

        if (mLibraryProcessType == LibraryProcessType.PROCESS_BROWSER) {
            // Add a switch for the reached code profiler as late as possible since it requires a
            // read from the shared preferences. At this point the shared preferences are usually
            // warmed up.
            int reachedCodeSamplingIntervalUs = getReachedCodeSamplingIntervalUs();
            if (reachedCodeSamplingIntervalUs > 0) {
                CommandLine.getInstance().appendSwitch(BaseSwitches.ENABLE_REACHED_CODE_PROFILER);
                CommandLine.getInstance().appendSwitchWithValue(
                        BaseSwitches.REACHED_CODE_SAMPLING_INTERVAL_US,
                        Integer.toString(reachedCodeSamplingIntervalUs));
            }

            // Similarly, append a switch to enable the background thread pool group if the cached
            // preference indicates it should be enabled.
            if (isBackgroundThreadPoolEnabled()) {
                CommandLine.getInstance().appendSwitch(BaseSwitches.ENABLE_BACKGROUND_THREAD_POOL);
            }
        }

        ensureCommandLineSwitchedAlreadyLocked();

        if (!LibraryLoaderJni.get().libraryLoaded(mLibraryProcessType)) {
            Log.e(TAG, "error calling LibraryLoaderJni.get().libraryLoaded");
            throw new ProcessInitException(LoaderErrors.FAILED_TO_REGISTER_JNI);
        }

        Log.i(TAG, "Successfully loaded native library");
        UmaRecorderHolder.onLibraryLoaded();

        // From now on, keep tracing in sync with native.
        TraceEvent.onNativeTracingReady();

        // From this point on, native code is ready to use, but non-MainDex JNI may not yet have
        // been registered. Check isInitialized() to be sure that initialization is fully complete.
        // Note that this flag can be accessed asynchronously, so any initialization
        // must be performed before.
        mInitialized = true;
    }

    // Called after all native initializations are complete.
    public void onBrowserNativeInitializationComplete() {
        synchronized (mLock) {
            if (useChromiumLinker()) {
                RecordHistogram.recordTimesHistogram(
                        "ChromiumAndroidLinker.BrowserLoadTime", mLibraryLoadTimeMs);
            }
        }
    }

    // Records pending Chromium linker histogram state for renderer process. This cannot be
    // recorded as a histogram immediately because histograms and IPCs are not ready at the
    // time they are captured. This function stores a pending value, so that a later call to
    // RecordChromiumAndroidLinkerRendererHistogram() will record it correctly.
    public void registerRendererProcessHistogram() {
        if (!useChromiumLinker()) return;
        synchronized (mLock) {
            LibraryLoaderJni.get().recordRendererLibraryLoadTime(mLibraryLoadTimeMs);
        }
    }

    /**
     * Overrides the library loader (normally with a mock) for testing.
     *
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

    /**
     * This sets the LibraryLoader internal state to its fully initialized state and should *only*
     * be used by clients like NativeTests which manually load their native libraries without using
     * the LibraryLoader.
     */
    public void setLibrariesLoadedForNativeTests() {
        mLoadState = LoadState.LOADED;
        mInitialized = true;
    }

    @NativeMethods
    interface Natives {
        // Only methods needed before or during normal JNI registration are during System.OnLoad.
        // nativeLibraryLoaded is then called to register everything else.  This process is called
        // "initialization".  This method will be mapped (by generated code) to the LibraryLoaded
        // definition in base/android/library_loader/library_loader_hooks.cc.
        //
        // Return true on success and false on failure.
        boolean libraryLoaded(@LibraryProcessType int processType);

        void registerNonMainDexJni();

        // Records the number of milliseconds it took to load the libraries in the renderer.
        void recordRendererLibraryLoadTime(long libraryLoadTime);
    }
}
