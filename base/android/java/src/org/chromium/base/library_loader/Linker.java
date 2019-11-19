// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

import android.annotation.SuppressLint;
import android.os.Bundle;
import android.os.Parcel;
import android.os.ParcelFileDescriptor;
import android.os.Parcelable;
import android.os.SystemClock;

import androidx.annotation.IntDef;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.base.annotations.AccessedByNative;
import org.chromium.base.annotations.JniIgnoreNatives;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

import javax.annotation.concurrent.GuardedBy;

/*
 * Technical note:
 *
 * The point of this class is to provide an alternative to System.loadLibrary()
 * to load the native shared library. One specific feature that it supports is the
 * ability to save RAM by sharing the ELF RELRO sections between renderer
 * processes.
 *
 * When two processes load the same native library at the _same_ memory address,
 * the content of their RELRO section (which includes C++ vtables or any
 * constants that contain pointers) will be largely identical [1].
 *
 * By default, the RELRO section is backed by private RAM in each process, which is still
 * significant on mobile (e.g. ~2 MB / process on Chrome 77 ARM, more on ARM64).
 *
 * However, it is possible to save RAM by creating a shared memory region,
 * copy the RELRO content into it, then have each process swap its private,
 * regular RELRO, with a shared, read-only, mapping of the shared one.
 *
 * This trick saves 98% of the RELRO section size per extra process, after the
 * first one. On the other hand, this requires careful communication between
 * the process where the shared RELRO is created and the one(s) where it is used.
 *
 * LegacyLinker only:
 * Note that swapping the regular RELRO with the shared one is not an atomic
 * operation. Care must be taken that no other thread tries to run native code
 * that accesses it during it. In practice, this means the swap must happen
 * before library native code is executed.
 *
 * [1] The exceptions are pointers to external, randomized, symbols, like
 * those from some system libraries, but these are very few in practice.
 */

/*
 * Security considerations:
 *
 * - The shared RELRO memory region is always forced read-only after creation,
 *   which means it is impossible for a compromised service process to map
 *   it read-write (e.g. by calling mmap() or mprotect()) and modify its
 *   content, altering values seen in other service processes.
 *
 * - Once the RELRO ashmem region or file is mapped into a service process's
 *   address space, the corresponding file descriptor is immediately closed. The
 *   file descriptor is kept opened in the browser process, because a copy needs
 *   to be sent to each new potential service process.
 *
 * - The common library load addresses are randomized for each instance of
 *   the program on the device. See getRandomBaseLoadAddress() for more
 *   details on how this is obtained.
 */

/**
 * Here's an explanation of how this class is supposed to be used:
 *
 *  - The native shared library must be loaded with Linker.loadLibrary(),
 *    instead of System.loadLibrary(). The two functions should behave the same
 *    (at a high level).
 *
 *  - Before loading the library, setApkFilePath() must be called when loading from the APK.
 *
 *  - A service process shall call either initServiceProcess() or
 *    disableSharedRelros() early (i.e. before loadLibrary() is called).
 *    Otherwise, the linker considers that it is running inside the browser
 *    process. This is because various Chromium projects have vastly
 *    different initialization paths.
 *
 *    disableSharedRelros() completely disables shared RELRO, and loadLibrary()
 *    will behave exactly like System.loadLibrary().
 *
 *    initServiceProcess(baseLoadAddress) indicates that shared RELRO are to be
 *    used in this process.
 *
 *  - The browser is in charge of deciding where in memory each library should
 *    be loaded. This address must be passed to each service process (see
 *    ChromiumLinkerParams.java in //content for a helper class to do so).
 *
 *  - The browser will also generate shared RELRO for the loaded library.
 *
 *  - Once the library has been loaded in the browser process, one can call getSharedRelros() which
 *    returns a Bundle instance containing the shared RELRO region.
 *
 *    This Bundle must be passed to each service process, for example through
 *    a Binder call (note that the Bundle includes file descriptors and cannot
 *    be added as an Intent extra).
 *
 *  - In a service process, loadLibrary() may block until the RELRO section Bundle is received. This
 *    is typically done by calling provideSharedRelros() from another thread.
 *
 *    This method also ensures the process uses the shared RELROs.
 */
@JniIgnoreNatives
public abstract class Linker {
    // Log tag for this class.
    private static final String TAG = "Linker";

    // Name of the library that contains our JNI code.
    protected static final String LINKER_JNI_LIBRARY = "chromium_android_linker";

    // Set to true to enable debug logs.
    protected static final boolean DEBUG = false;

    // Used to pass the shared RELRO Bundle through Binder.
    public static final String EXTRA_LINKER_SHARED_RELROS =
            "org.chromium.base.android.linker.shared_relros";

    // The name of a class that implements TestRunner.
    private String mTestRunnerClassName;

    // Constants used to indicate a given Linker implementation, for testing.
    //   LEGACY       -> Always uses the LegacyLinker implementation.
    //   MODERN       -> Always uses the ModernLinker implementation.
    // NOTE: These names are known and expected by the Linker test scripts.
    public static final int LINKER_IMPLEMENTATION_LEGACY = 1;
    public static final int LINKER_IMPLEMENTATION_MODERN = 2;

    // Singleton.
    protected static final Object sLock = new Object();

    @GuardedBy("sLock")
    private static Linker sSingleton;

    @GuardedBy("sLock")
    protected LibInfo mLibInfo;

    // Becomes true to indicate this process needs to wait for a shared RELRO in LibraryLoad().
    @GuardedBy("sLock")
    protected boolean mWaitForSharedRelros;

    // Set to true if this runs in the browser process. Disabled by initServiceProcess().
    @GuardedBy("sLock")
    protected boolean mInBrowserProcess = true;

    // Current common random base load address. A value of -1 indicates not yet initialized.
    @GuardedBy("sLock")
    protected long mBaseLoadAddress = -1;

    /**
     * States for library loading. The only transitions are forward ones.
     *
     * The states are:
     * - UNINITIALIZED: Initial state.
     * - INITIALIZED: After linker initialization. Required for using the linker.
     *
     * When loading a library, there are several cases:
     * - relro will be shared: the browser process loads the library and provides them, other
     *   processes wait for them to load the library (ModernLinker), or load then wait
     *   (LegacyLinker).
     * - relro will not be shared.
     *
     * Once the library has been loaded, in the browser process the state is DONE_PROVIDE_RELRO,
     * and in other processes DONE.
     *
     * Transitions are then:
     * All processes: UNINITIALIZED -> INITIALIZED
     * Browser: INITIALIZED -> DONE_PROVIDE_RELRO
     * Other:   INITIALIZED -> DONE
     *
     * When relro sharing is not happening:
     * INITIALIZED -> DONE_PROVIDE_RELRO (Browser in case of relro sharing error)
     * INITIALIZED -> DONE (Other processes, or browser when relro sharing was disabled from the
     * start, possibly due to a retry after a load failure.)
     */
    @IntDef({State.UNINITIALIZED, State.INITIALIZED, State.DONE_PROVIDE_RELRO, State.DONE})
    @Retention(RetentionPolicy.SOURCE)
    protected @interface State {
        int UNINITIALIZED = 0;
        int INITIALIZED = 1;
        int DONE_PROVIDE_RELRO = 2;
        int DONE = 3;
    }

    @GuardedBy("sLock")
    @State
    protected int mState = State.UNINITIALIZED;

    // Protected singleton constructor.
    protected Linker() {}

    /**
     * Get singleton instance. Returns either a LegacyLinker or a ModernLinker.
     *
     * ModernLinker requires OS features from Android M and later: a system linker
     * that handles packed relocations and load from APK, and |android_dlopen_ext()|
     * for shared RELRO support. It cannot run on Android releases earlier than M.
     *
     * LegacyLinker runs on all Android releases but it is slower and more complex
     * than ModernLinker. We still use it on M as it avoids writing the relocation to disk.
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
     * @return the Linker implementation instance.
     */
    public static Linker getInstance() {
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
        // either Chrome{,Modern}, the linker tests or Trichrome.
        synchronized (sLock) {
            if (sSingleton == null) {
                // With incremental install, it's important to fall back to the "normal"
                // library loading path in order for the libraries to be found.
                String appClass =
                        ContextUtils.getApplicationContext().getApplicationInfo().className;
                boolean isIncrementalInstall =
                        appClass != null && appClass.contains("incrementalinstall");
                if (LibraryLoader.getInstance().useModernLinker() && !isIncrementalInstall) {
                    sSingleton = new ModernLinker();
                } else {
                    sSingleton = new LegacyLinker();
                }
                Log.i(TAG, "Using linker: %s", sSingleton.getClass().getName());
            }
            return sSingleton;
        }
    }

    /**
     * Call this method before loading any libraries to indicate that this
     * process shall neither create or reuse shared RELRO sections.
     */
    public final void disableSharedRelros() {
        if (DEBUG) Log.i(TAG, "disableSharedRelros() called");
        synchronized (sLock) {
            ensureInitializedLocked();
            assert mState == State.INITIALIZED; // Not after the library has been loaded.
            mInBrowserProcess = false;
            mWaitForSharedRelros = false;
        }
    }

    /**
     * Obtain a random base load address at which to place loaded libraries.
     *
     * @return new base load address
     */
    protected static long getRandomBaseLoadAddress() {
        // nativeGetRandomBaseLoadAddress() returns an address at which it has previously
        // successfully mapped an area larger than the largest library we expect to load,
        // on the basis that we will be able, with high probability, to map our library
        // into it.
        //
        // One issue with this is that we do not yet know the size of the library that
        // we will load is. If it is smaller than the size we used to obtain a random
        // address the library mapping may still succeed. The other issue is that
        // although highly unlikely, there is no guarantee that something else does not
        // map into the area we are going to use between here and when we try to map into it.
        //
        // The above notes mean that all of this is probabilistic. It is however okay to do
        // because if, worst case and unlikely, we get unlucky in our choice of address,
        // the back-out and retry without the shared RELRO in the ChildProcessService will
        // keep things running.
        final long address = nativeGetRandomBaseLoadAddress();
        if (DEBUG) Log.i(TAG, "Random native base load address: 0x%x", address);
        return address;
    }

    /** Tell the linker about the APK path, if the library is loaded from the APK. */
    void setApkFilePath(String path) {}

    /**
     * Load a native shared library with the Chromium linker. Note the crazy linker treats
     * libraries and files as equivalent, so you can only open one library in a given zip
     * file. The library must not be the Chromium linker library.
     *
     * @param library The library name to load.
     * @param isFixesAddressPermitted Whether the library can be loaded at a fixed address for RELRO
     * sharing.
     */
    final void loadLibrary(String library, boolean isFixedAddressPermitted) {
        if (DEBUG) Log.i(TAG, "loadLibrary: %s", library);
        assert !library.equals(LINKER_JNI_LIBRARY);
        synchronized (sLock) {
            loadLibraryImplLocked(library, isFixedAddressPermitted);
        }
    }

    /**
     * Call this to send a Bundle containing the shared RELRO section to be used in this process. If
     * initServiceProcess() was previously called, loadLibrary() will not exit until this method is
     * called in another thread with a non-null value.
     *
     * @param bundle The Bundle instance containing the shared RELRO section to use in this process.
     */
    public final void provideSharedRelros(Bundle bundle) {
        if (DEBUG) Log.i(TAG, "provideSharedRelros() called with %s", bundle);
        synchronized (sLock) {
            assert mState != State.DONE && mState != State.DONE_PROVIDE_RELRO;
            // Note that in certain cases, this can be called before
            // initServiceProcess() in service processes.
            mLibInfo = LibInfo.fromBundle(bundle);
            // Tell any listener blocked in loadLibraryImplLocked() about it.
            sLock.notifyAll();
        }
    }

    /**
     * Call this to retrieve the shared RELRO sections created in this process,
     * after loading the library.
     *
     * @return a new Bundle instance, or null if RELRO sharing is disabled on
     * this system, or if initServiceProcess() was called previously.
     */
    public final Bundle getSharedRelros() {
        synchronized (sLock) {
            if (mState == State.DONE_PROVIDE_RELRO) {
                assert mInBrowserProcess;
                Bundle result = mLibInfo.toBundle();
                if (DEBUG) Log.i(TAG, "getSharedRelros() = %s", result.toString());
                return result;
            }

            if (DEBUG) Log.i(TAG, "getSharedRelros() = null");
            return null;
        }
    }

    /**
     * Call this method before loading the library to indicate that this process is ready to reuse
     * shared RELRO sections from another one. Typically used when starting service processes.
     *
     * @param baseLoadAddress the base library load address to use.
     */
    public final void initServiceProcess(long baseLoadAddress) {
        if (DEBUG) Log.i(TAG, "initServiceProcess(0x%x) called", baseLoadAddress);
        synchronized (sLock) {
            ensureInitializedLocked();
            assert mState == State.INITIALIZED;

            mInBrowserProcess = false;
            mWaitForSharedRelros = true;
            mBaseLoadAddress = baseLoadAddress;
        }
    }

    /**
     * Retrieve the base load address of the shared RELRO section.  This also enforces the creation
     * of the shared RELRO section, which can later be retrieved with getSharedRelros().
     *
     * @return a common, random base load address, or 0 if RELRO sharing is
     * disabled.
     */
    public final long getBaseLoadAddress() {
        synchronized (sLock) {
            ensureInitializedLocked();
            setupBaseLoadAddressLocked();
            if (DEBUG) Log.i(TAG, "getBaseLoadAddress() returns 0x%x", mBaseLoadAddress);

            return mBaseLoadAddress;
        }
    }

    /**
     * Implements loading the native shared library with the Chromium linker.
     *
     * Load a native shared library with the Chromium linker. If the library is within a zip file
     * it must be uncompressed and page aligned.
     *
     * If asked to wait for shared RELROs, this function will block until the shared RELRO bundle
     * is received by provideSharedRelros().
     *
     * @param libFilePath The path of the library (possibly in the zip file).
     * @param isFixedAddressPermitted If true, uses a fixed load address if one was
     * supplied, otherwise ignores the fixed address and loads wherever available.
     */
    abstract void loadLibraryImplLocked(String libFilePath, boolean isFixedAddressPermitted);

    /** Load the Linker JNI library. Throws UnsatisfiedLinkError on error. */
    @SuppressLint({"UnsafeDynamicallyLoadedCode"})
    @GuardedBy("sLock")
    private void loadLinkerJniLibraryLocked() {
        assert mState == State.UNINITIALIZED;

        LibraryLoader.setEnvForNative();
        if (DEBUG) Log.i(TAG, "Loading lib%s.so", LINKER_JNI_LIBRARY);

        try {
            System.loadLibrary(LINKER_JNI_LIBRARY);
        } catch (UnsatisfiedLinkError e) {
            if (LibraryLoader.PLATFORM_REQUIRES_NATIVE_FALLBACK_EXTRACTION) {
                System.load(LibraryLoader.getExtractedLibraryPath(
                        ContextUtils.getApplicationContext().getApplicationInfo(),
                        LINKER_JNI_LIBRARY));
            } else {
                // Cannot continue if we cannot load the linker. Technically we could try to
                // load the library with the system linker on Android M+, but this should never
                // happen, better to catch it in crash reports.
                throw e;
            }
        }
    }

    // Used internally to initialize the linker's data. Loads JNI.
    @GuardedBy("sLock")
    protected final void ensureInitializedLocked() {
        if (mState != State.UNINITIALIZED) return;

        loadLinkerJniLibraryLocked();
        // Force generation of random base load address, as well as creation of shared RELRO
        // sections in this process.
        if (mInBrowserProcess) setupBaseLoadAddressLocked();

        mState = State.INITIALIZED;
    }

    // Used internally to wait for shared RELROs. Returns once useSharedRelros() has been
    // called to supply a valid shared RELROs bundle.
    @GuardedBy("sLock")
    protected final void waitForSharedRelrosLocked() {
        if (DEBUG) Log.i(TAG, "waitForSharedRelros() called");

        // Wait until notified by useSharedRelros() that shared RELROs have arrived.
        //
        // Note that the relocations may already have been provided by the time we arrive here, so
        // this may return immediately.
        long startTime = DEBUG ? SystemClock.uptimeMillis() : 0;
        while (mLibInfo == null) {
            try {
                sLock.wait();
            } catch (InterruptedException e) {
                // Continue waiting even if we were just interrupted.
            }
        }

        if (DEBUG) {
            Log.i(TAG, "Time to wait for shared RELRO: %d ms",
                    SystemClock.uptimeMillis() - startTime);
        }
    }

    // Used internally to lazily setup the common random base load address.
    @GuardedBy("sLock")
    private void setupBaseLoadAddressLocked() {
        if (mBaseLoadAddress == -1) {
            mBaseLoadAddress = getRandomBaseLoadAddress();
            if (mBaseLoadAddress == 0) {
                // If the random address is 0 there are issues with finding enough
                // free address space, so disable RELRO shared / fixed load addresses.
                Log.w(TAG, "Disabling shared RELROs due address space pressure");
                mWaitForSharedRelros = false;
            }
        }
    }

    /**
     * Record information for a given library.
     *
     * IMPORTANT: Native code knows about this class's fields, so
     * don't change them without modifying the corresponding C++ sources.
     * Also, the LibInfo instance owns the shared RELRO file descriptor.
     */
    @JniIgnoreNatives
    protected static class LibInfo implements Parcelable {
        private static final String EXTRA_LINKER_LIB_INFO = "libinfo";

        LibInfo() {}

        // from Parcelable
        LibInfo(Parcel in) {
            // See below in writeToParcel() for the serializatiom protocol.
            mLibFilePath = in.readString();
            mLoadAddress = in.readLong();
            mLoadSize = in.readLong();
            mRelroStart = in.readLong();
            mRelroSize = in.readLong();
            boolean hasRelroFd = in.readInt() != 0;
            if (hasRelroFd) {
                ParcelFileDescriptor fd = ParcelFileDescriptor.CREATOR.createFromParcel(in);
                // If CreateSharedRelro fails, the OS file descriptor will be -1 and |fd| will be
                // null.
                if (fd != null) {
                    mRelroFd = fd.detachFd();
                }
            } else {
                mRelroFd = -1;
            }
        }

        public void close() {
            if (mRelroFd >= 0) {
                StreamUtil.closeQuietly(ParcelFileDescriptor.adoptFd(mRelroFd));
                mRelroFd = -1;
            }
        }

        public static LibInfo fromBundle(Bundle bundle) {
            return bundle.getParcelable(EXTRA_LINKER_LIB_INFO);
        }

        public Bundle toBundle() {
            Bundle bundle = new Bundle();
            bundle.putParcelable(EXTRA_LINKER_LIB_INFO, this);
            return bundle;
        }

        @Override
        public void writeToParcel(Parcel out, int flags) {
            out.writeString(mLibFilePath);
            out.writeLong(mLoadAddress);
            out.writeLong(mLoadSize);
            out.writeLong(mRelroStart);
            out.writeLong(mRelroSize);
            // Parcel#writeBoolean() is API level 29, so use an int instead.
            // We use this as a flag as we cannot serialize an invalid fd.
            out.writeInt(mRelroFd >= 0 ? 1 : 0);
            if (mRelroFd >= 0) {
                try {
                    ParcelFileDescriptor fd = ParcelFileDescriptor.fromFd(mRelroFd);
                    fd.writeToParcel(out, 0);
                    fd.close();
                } catch (java.io.IOException e) {
                    Log.e(TAG, "Can't write LibInfo file descriptor to parcel", e);
                }
            }
        }

        @Override
        public int describeContents() {
            return Parcelable.CONTENTS_FILE_DESCRIPTOR;
        }

        // From Parcelable
        public static final Parcelable.Creator<LibInfo> CREATOR =
                new Parcelable.Creator<LibInfo>() {
                    @Override
                    public LibInfo createFromParcel(Parcel in) {
                        return new LibInfo(in);
                    }

                    @Override
                    public LibInfo[] newArray(int size) {
                        return new LibInfo[size];
                    }
                };

        public String mLibFilePath;

        // IMPORTANT: Don't change these fields without modifying the
        // native code that accesses them directly!
        @AccessedByNative
        public long mLoadAddress; // page-aligned library load address.
        @AccessedByNative
        public long mLoadSize;    // page-aligned library load size.
        @AccessedByNative
        public long mRelroStart;  // page-aligned address in memory, or 0 if none.
        @AccessedByNative
        public long mRelroSize;   // page-aligned size in memory, or 0.
        @AccessedByNative
        public int mRelroFd = -1; // shared RELRO file descriptor, or -1
    }

    /* ---------------------- Testing support methods. ---------------------- */

    /**
     * Get Linker implementation type.
     * For testing.
     *
     * @return LINKER_IMPLEMENTATION_LEGACY or LINKER_IMPLEMENTATION_MODERN
     */
    public final int getImplementationForTesting() {
        // Sanity check. This method may only be called during tests.
        assert NativeLibraries.sEnableLinkerTests;

        synchronized (sLock) {
            assert sSingleton == this;

            if (sSingleton instanceof ModernLinker) {
                return LINKER_IMPLEMENTATION_MODERN;
            } else if (sSingleton instanceof LegacyLinker) {
                return LINKER_IMPLEMENTATION_LEGACY;
            }
            throw new AssertionError("Invalid linker: " + sSingleton.getClass().getName());
        }
    }

    /**
     * A public interface used to run runtime linker tests after loading
     * libraries. Should only be used to implement the linker unit tests,
     * which is controlled by the value of NativeLibraries.sEnableLinkerTests
     * configured at build time.
     */
    public interface TestRunner {
        /**
         * Run runtime checks and return true if they all pass.
         *
         * @param inBrowserProcess true iff this is the browser process.
         * @return true if all checks pass.
         */
        public boolean runChecks(boolean inBrowserProcess);
    }

    /**
     * Call this to retrieve the name of the current TestRunner class name
     * if any. This can be useful to pass it from the browser process to
     * child ones.
     *
     * @return null or a String holding the name of the class implementing
     * the TestRunner set by calling setTestRunnerClassNameForTesting() previously.
     */
    public final String getTestRunnerClassNameForTesting() {
        assert NativeLibraries.sEnableLinkerTests;

        synchronized (sLock) {
            return mTestRunnerClassName;
        }
    }

    /**
     * Set up the Linker for a test.
     * Convenience function that calls setImplementationForTesting() to force an
     * implementation, and then setTestRunnerClassNameForTesting() to set the test
     * class name.
     *
     * On first call, instantiates a Linker of the requested type and sets its test
     * runner class name. On subsequent calls, checks that the singleton produced by
     * the first call matches the requested type and test runner class name.
     */
    public static final void setupForTesting(int type, String testRunnerClassName) {
        assert NativeLibraries.sEnableLinkerTests;
        assert type == LINKER_IMPLEMENTATION_LEGACY || type == LINKER_IMPLEMENTATION_MODERN;

        if (DEBUG) Log.i(TAG, "setupForTesting(%d, %s) called", type, testRunnerClassName);

        synchronized (sLock) {
            assert sSingleton == null;
            if (type == LINKER_IMPLEMENTATION_MODERN) {
                sSingleton = new ModernLinker();
            } else if (type == LINKER_IMPLEMENTATION_LEGACY) {
                sSingleton = new LegacyLinker();
            }
            Log.i(TAG, "Forced linker: %s", sSingleton.getClass().getName());
            Linker.getInstance().mTestRunnerClassName = testRunnerClassName;
        }
    }

    /**
     * Instantiate and run the current TestRunner, if any. The TestRunner implementation
     * must be instantiated _after_ all libraries are loaded to ensure that its
     * native methods are properly registered.
     *
     * @param inBrowserProcess true if in the browser process
     */
    protected final void runTestRunnerClassForTesting(boolean inBrowserProcess) {
        assert NativeLibraries.sEnableLinkerTests;
        if (DEBUG) Log.i(TAG, "runTestRunnerClassForTesting called");

        synchronized (sLock) {
            if (mTestRunnerClassName == null) {
                Log.wtf(TAG, "Linker runtime tests not set up for this process");
                assert false;
            }
            if (DEBUG) {
                Log.i(TAG, "Instantiating " + mTestRunnerClassName);
            }
            TestRunner testRunner = null;
            try {
                testRunner = (TestRunner) Class.forName(mTestRunnerClassName)
                                     .getDeclaredConstructor()
                                     .newInstance();
            } catch (Exception e) {
                Log.wtf(TAG, "Could not instantiate test runner class by name", e);
                assert false;
            }

            if (!testRunner.runChecks(inBrowserProcess)) {
                Log.wtf(TAG, "Linker runtime tests failed in this process");
                assert false;
            }

            Log.i(TAG, "All linker tests passed");
        }
    }

    /**
     * Return a random address that should be free to be mapped with the given size.
     * Maps an area large enough for the largest library we might attempt to load,
     * and if successful then unmaps it and returns the address of the area allocated
     * by the system (with ASLR). The idea is that this area should remain free of
     * other mappings until we map our library into it.
     *
     * @return address to pass to future mmap, or 0 on error.
     */
    private static native long nativeGetRandomBaseLoadAddress();
}
