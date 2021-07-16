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
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.base.annotations.AccessedByNative;
import org.chromium.base.annotations.JniIgnoreNatives;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

import javax.annotation.concurrent.GuardedBy;

/*
 * This class provides a way to load the native library as an alternative to System.loadLibrary().
 * It has the ability to save RAM by placing the PT_GNU_RELRO segments in a shared memory region and
 * memory-mapping this region from different processes. This approach saves a few MiB RAM compared
 * to the normal placement of the segment in private dirty memory.
 *
 * In the main library only one PT_GNU_RELRO segment is present, and it maps only one section
 * (.data.rel.ro), so here and below it will be referred as a "RELRO section".
 *
 * When two processes load the same native library at the _same_ memory address, the content of
 * their RELRO section (which includes C++ vtables or any constants that contain pointers) will be
 * largely identical. The exceptions are pointers to external, randomized, symbols, like those from
 * some system libraries, but these are very rare in practice.
 *
 * In order to establish usage of the same shared region in different processes, the Linker can
 * serialize/deserialize the relevant information to/from a Bundle. Providing the RELRO shared
 * memory region is done by loading the library normally, then replacing the virtual address mapping
 * behind the RELRO section with the one backed by the shared memory, with identical contents.
 *
 * Security considerations:
 *
 * - The shared RELRO memory region is always forced read-only after creation, which means it is
 *   impossible for a compromised process to map it read-write (e.g. by calling mmap() or
 *   mprotect()) and modify its content, altering values seen in other processes.
 *
 * - The common library load addresses are randomized for each instance of the program on the
 *   device.
 *
 * Usage:
 *
 * - The native shared library must be loaded with Linker.loadLibrary(), instead of
 *   System.loadLibrary(). The two functions should behave the same (at a high level).
 *
 * - Before loading the library, setApkFilePath() must be called when loading from the APK.
 *
 * - Early on, before the attempt to load the library, the linker needs to be initialized either as
 *   a producer or a consumer of the RELRO region by invoking ensureInitialized(). Since various
 *   Chromium projects have vastly different initialization paths, for convenience the
 *   initialization runs implicitly as part of loading the library. In this case the behaviour is of
 *   a producer.
 *
 * - When running as a RELRO consumer, the loadLibrary() may block until the RELRO section Bundle
 *   is received. This is done by calling takeSharedRelrosFromBundle() from another thread.
 *
 * - After loading the native library as a RELRO producer, the putSharedRelrosToBundle() becomes
 *   available to then send the Bundle to Linkers in other processes.
 */
abstract class Linker {
    private static final String TAG = "Linker";

    // Name of the library that contains the JNI code.
    protected static final String LINKER_JNI_LIBRARY = "chromium_android_linker";

    // Constant guarding debug logging.
    protected static final boolean DEBUG = LibraryLoader.DEBUG;

    // Constants used to pass the shared RELRO Bundle through Binder.
    private static final String SHARED_RELROS = "org.chromium.base.android.linker.shared_relros";
    private static final String BASE_LOAD_ADDRESS =
            "org.chromium.base.android.linker.base_load_address";

    protected final Object mLock = new Object();

    // Holds the address and the size of the reserved address range until the library is loaded.
    // After that its |mLoadAddress| and |mLoadSize| will reflect the state of the loaded library.
    // Further, when the RELRO region is extracted into shared memory, the |mRelroFd| is initialized
    // along with |mRelro{Start,Size}|. This object is serialized for use in other processes if the
    // process is a "RELRO producer".
    @GuardedBy("mLock")
    protected LibInfo mLocalLibInfo;

    // The library info that was transferred from another process. Only useful if it contains RELRO
    // FD.
    @GuardedBy("mLock")
    protected LibInfo mRemoteLibInfo;

    // Whether this Linker instance should potentially create the RELRO region. Even if true, the
    // library loading can fall back to the system linker without producing the region. The default
    // value is used in tests, it is set to true so that the Linker does not have to wait for RELRO
    // to arrive from another process.
    @GuardedBy("mLock")
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    boolean mRelroProducer = true;

    @GuardedBy("mLock")
    private boolean mLinkerWasWaitingSynchronously;

    /**
     * The state machine of library loading.
     *
     * The states are:
     * - UNINITIALIZED: Initial state.
     * - INITIALIZED: After linker initialization. Required for using the linker.
     *
     * When loading a library, there are two possibilities:
     *
     * - RELRO is not shared.
     *
     * - ModernLinker: RELRO is shared: the producer process loads the library, consumers load the
     *   native library without waiting, they use the RELRO bundle later when it arrives, or
     *   immediately if it arrived before load
     *
     * - LegacyLinker: loads the native library then waits synchronously for RELRO bundle
     *
     * Once the library has been loaded, in the producer process the state is DONE_PROVIDE_RELRO,
     * and in consumer processes it is DONE.
     *
     * Transitions are:
     * All processes: UNINITIALIZED -> INITIALIZED
     * Producer: INITIALIZED -> DONE_PROVIDE_RELRO
     * Consumer: INITIALIZED -> DONE
     *
     * When RELRO sharing failed for one reason or another, the state transitions remain the same,
     * despite DONE_PROVIDE_RELRO being not appropriate as a name for this case.
     */
    @IntDef({State.UNINITIALIZED, State.INITIALIZED, State.DONE_PROVIDE_RELRO, State.DONE})
    @Retention(RetentionPolicy.SOURCE)
    protected @interface State {
        int UNINITIALIZED = 0;
        int INITIALIZED = 1;
        int DONE_PROVIDE_RELRO = 2;
        int DONE = 3;
    }

    @GuardedBy("mLock")
    @State
    protected int mState = State.UNINITIALIZED;

    private static Linker sLinkerForAssert;

    protected Linker() {
        // Only one instance is allowed in a given process because effects of loading a library are
        // global, and the list of loaded libraries is not maintained at this level.
        assert sLinkerForAssert == null;
        sLinkerForAssert = this;
    }

    @IntDef({PreferAddress.FIND_RESERVED, PreferAddress.RESERVE_HINT, PreferAddress.RESERVE_RANDOM})
    @Retention(RetentionPolicy.SOURCE)
    public @interface PreferAddress {
        int FIND_RESERVED = 0;
        int RESERVE_HINT = 1;
        int RESERVE_RANDOM = 2;
    }
    private String preferAddressToString(@PreferAddress int a) {
        switch (a) {
            case PreferAddress.FIND_RESERVED:
                return "FIND_RESERVED";
            case PreferAddress.RESERVE_HINT:
                return "RESERVE_HINT";
            case PreferAddress.RESERVE_RANDOM:
                return "RESERVE_RANDOM";
            default:
                return String.valueOf(a);
        }
    }

    // Exposed to be able to mock out an assertion.
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    boolean isNonZeroLoadAddress(LibInfo libInfo) {
        return libInfo != null && libInfo.mLoadAddress != 0;
    }

    /**
     * Initializes the Linker. This is the first method to be called on the instance.
     *
     * Linker and its subclasses abstract away from knowing process types and what the role of each
     * process is. The LibraryLoader and the layers above tell the singleton linker whether it needs
     * to produce the RELRO region, consume it, whether to use the address hint or to synthesize
     * according to a strategy.
     *
     * In many cases finding the library load address is on the critical path, and needs to be
     * transferred to other processes as soon as possible. For this purpose initialization is
     * invoked separately from loading.
     *
     * The caller should provide the |preference| for obtaining the address at which to load the
     * native library. The value is one of these:
     *
     *     FIND_RESERVED, RESERVE_HINT, RESERVE_RANDOM.
     *
     * In the vast majority of cases the chosen preference will be fulfilled and the address (with
     * the size) will be saved for use during {@link Linker#loadLibrary(String)}. In case the
     * preferred way fails (due to address fragmentation, for example), a fallback attempt will be
     * made with |preference| to the right of the current one in the list above. There is no
     * fallback for RESERVE_RANDOM.
     *
     * FIND_RESERVED: Finds the (named) reserved address range for library loading. The caller needs
     *     to make sure that this is only called on platform versions supporting this memory
     *     reservation (Android Q+).
     *
     * RESERVE_HINT: Reserves enough of address range for loading a library, starting at the
     *     |addressHint| provided. The latter is expected to arrive from another process (randomized
     *     there), hence sometimes the address range may not be available.
     *
     * RESERVE_RANDOM: Finds a free random address range and reserves it.
     *
     * With the LegacyLinker this method releases the memory reservation (or does not attempt to do
     * it). This is because the LegacyLinker reserves a region using:
     *
     *     mmap(address_hint, flags_without_MAP_FIXED )
     *
     * This behavior is harmlessly racy: in case something else grabs the address range before the
     * library loading starts, the dynamic linking will load at a different address and fall back to
     * *not* sharing RELRO. The chances of this happening in a fresh process are low.
     *
     * Unmapping and then mapping the region again is likely unnecessary. Ideally both Linkers
     * should start by reserving the memory region early in process lifetime, then load the library
     * with mmap(nullptr, ... MAP_FIXED) on top of it. Unfortunately the crazylinker feature to load
     * on top of a reserved memory region is not well tested and looks buggy.
     *
     * @param asRelroProducer whether the Linker instance will need to produce the shared memory
     *                        region as part of work in {@link Linker#loadLibrary(String)}.
     * @param preference the preference for obtaining the address, with fallback to a less memory
     *                   efficient method
     * @param addressHint the hint to be used when RESERVE_HINT is provided as |preference|
     *
     */
    final void ensureInitialized(
            boolean asRelroProducer, @PreferAddress int preference, long addressHint) {
        if (DEBUG) {
            Log.i(TAG,
                    "ensureInitialized(asRelroProducer=%b, preference=%s, "
                            + "loadAddressHint=0x%x)",
                    asRelroProducer, preferAddressToString(preference), addressHint);
        }
        assert !asRelroProducer
                || preference
                        != PreferAddress.RESERVE_HINT
            : "Producer does not accept hints from outside";
        synchronized (mLock) {
            if (mState != State.UNINITIALIZED) return;
            chooseAndReserveMemoryRange(asRelroProducer, preference, addressHint);
            if (DEBUG) {
                Log.i(TAG, "ensureInitialized: chose address=0x%x", mLocalLibInfo.mLoadAddress);
            }
            mState = State.INITIALIZED;
        }
    }

    // Initializes the |mLocalLibInfo| and reserves the address range chosen (only when
    // keepMemoryReservationUntilLoad() returns true).
    @GuardedBy("mLock")
    final void chooseAndReserveMemoryRange(
            boolean asRelroProducer, @PreferAddress int preference, long addressHint) {
        mLocalLibInfo = new LibInfo();
        mRelroProducer = asRelroProducer;
        loadLinkerJniLibraryLocked();
        boolean keepReservation = keepMemoryReservationUntilLoad();
        switch (preference) {
            case PreferAddress.FIND_RESERVED:
                if (getLinkerJni().findRegionReservedByWebViewZygote(mLocalLibInfo)) {
                    assert isNonZeroLoadAddress(mLocalLibInfo);
                    if (addressHint == 0 || addressHint == mLocalLibInfo.mLoadAddress) {
                        // Subtle: Both the producer and the consumer are expected to find the same
                        // address reservation. When |addressHint != 0| the producer was quick
                        // enough to provide the address before the consumer started initialization.
                        // Choosing the hint sounds like the right thing to do, and faster than
                        // looking up the named address range again. However, there is not enough
                        // information on how the hint was obtained by the producer. If it was found
                        // by a fallback (less likely) then the region must be reserved in this
                        // process. On systems where FIND_RESERVED is the preference, the most
                        // likely variant is that it is already reserved, hence check for it first.
                        return;
                    }
                }
                // Intentional fallthrough.
            case PreferAddress.RESERVE_HINT:
                mLocalLibInfo.mLoadAddress = addressHint;
                if (addressHint != 0) {
                    if (!keepReservation) return;
                    getLinkerJni().reserveMemoryForLibrary(mLocalLibInfo);
                    if (mLocalLibInfo.mLoadAddress != 0) return;
                }
                // Intentional fallthrough.
            case PreferAddress.RESERVE_RANDOM:
                getLinkerJni().findMemoryRegionAtRandomAddress(mLocalLibInfo, keepReservation);
        }
    }

    /**
     * Extracts the native library start address as serialized by
     * {@link #putLoadAddressToBundle(Bundle)} in a Linker instance from another process.
     */
    static long extractLoadAddressFromBundle(Bundle bundle) {
        return bundle.getLong(BASE_LOAD_ADDRESS, 0);
    }

    /**
     * Serializes the native library start address. If not asked to be initialized previously,
     * initializes the Linker as a RELRO producer.
     * @param bundle Bundle to put the address to.
     */
    void putLoadAddressToBundle(Bundle bundle) {
        if (DEBUG) Log.i(TAG, "putLoadAddressToBundle");
        synchronized (mLock) {
            assert mState != State.UNINITIALIZED;
            if (mLocalLibInfo != null && mLocalLibInfo.mLoadAddress != 0) {
                bundle.putLong(BASE_LOAD_ADDRESS, mLocalLibInfo.mLoadAddress);
            }
        }
    }

    /**
     * Tells whether the Linker expects the 'reserved' memory region
     * [mLocalLibInfo.{mLoadAddress,mLoadSize}] to be actually reserved with mmap(PROT_NONE). Not
     * expecting the actual reservation will make the LegacyLinker try to grab it again.
     */
    protected abstract boolean keepMemoryReservationUntilLoad();

    /** Tell the linker about the APK path, if the library is loaded from the APK. */
    void setApkFilePath(String path) {}

    /**
     * Loads the native library using a given mode.
     *
     * @param library The library name to load.
     * @param relroMode Tells whether and how to share relocations.
     */
    @GuardedBy("mLock")
    private void attemptLoadLibraryLocked(String library, @RelroSharingMode int relroMode) {
        if (DEBUG) Log.i(TAG, "attemptLoadLibraryLocked: %s", library);
        assert !library.equals(LINKER_JNI_LIBRARY);
        try {
            loadLibraryImplLocked(library, relroMode);
            if (!mLinkerWasWaitingSynchronously && mRemoteLibInfo != null && mState == State.DONE) {
                atomicReplaceRelroLocked(true /* relroAvailableImmediately */);
            }
        } finally {
            // Reset the state to serve the retry in loadLibrary().
            mLinkerWasWaitingSynchronously = false;
        }
    }

    /**
     * Loads the native shared library.
     *
     * The library must not be the Chromium linker library. The LegacyLinker only allows loading one
     * library per file, including zip/APK files.
     *
     * @param library The library name to load.
     */
    final void loadLibrary(String library) {
        synchronized (mLock) {
            try {
                // Normally Chrome/Webview/Weblayer processes initialize when they choose whether to
                // produce or consume the shared relocations. Initialization here is the last resort
                // to choose the load address in tests that forget to decide whether they are a
                // producer or a consumer.
                ensureInitializedImplicitlyAsLastResort();

                // Load the library. During initialization Linker subclass reserves the address
                // range where the library will be loaded and keeps it in |mLocalLibInfo|.
                attemptLoadLibraryLocked(library,
                        mRelroProducer ? RelroSharingMode.PRODUCE : RelroSharingMode.CONSUME);
            } catch (UnsatisfiedLinkError e) {
                Log.w(TAG, "Failed to load native library with shared RELRO, retrying without");
                try {
                    // Retry without relocation sharing.
                    mLocalLibInfo.mLoadAddress = 0;
                    attemptLoadLibraryLocked(library, RelroSharingMode.NO_SHARING);
                } catch (UnsatisfiedLinkError e2) {
                    Log.w(TAG, "Failed to load native library without RELRO sharing");
                    throw e2;
                }
            }
        }
    }

    /**
     * Serializes information about the RELRO region to be passed to a Linker in another process.
     * @param bundle The Bundle to serialize to.
     */
    void putSharedRelrosToBundle(Bundle bundle) {
        Bundle relros = null;
        synchronized (mLock) {
            if (mState == State.DONE_PROVIDE_RELRO) {
                assert mRelroProducer;
                relros = mLocalLibInfo.toBundle();
            }
            bundle.putBundle(SHARED_RELROS, relros);
            if (DEBUG && relros != null) {
                Log.i(TAG,
                        "putSharedRelrosToBundle() puts mLoadAddress=0x%x, mLoadSize=%d, "
                                + "mRelroFd=%d",
                        mLocalLibInfo.mLoadAddress, mLocalLibInfo.mLoadSize,
                        mLocalLibInfo.mRelroFd);
            }
        }
    }

    /**
     * Deserializes the RELRO region information that was marshalled by
     * {@link #putLoadAddressToBundle(Bundle)} and wakes up the threads waiting for it to replace
     * the RELRO section in this process with shared memory.
     * @param bundle The Bundle to extract the information from.
     */
    void takeSharedRelrosFromBundle(Bundle bundle) {
        if (DEBUG) Log.i(TAG, "called takeSharedRelrosFromBundle(%s)", bundle);
        Bundle relros = bundle.getBundle(SHARED_RELROS);
        if (relros == null) return;
        LibInfo newRemote = LibInfo.fromBundle(relros);
        if (newRemote == null) return;
        synchronized (mLock) {
            if (mRemoteLibInfo != null && mRemoteLibInfo.mRelroFd != -1) {
                if (DEBUG) {
                    Log.i(TAG,
                            "Attempt to replace RELRO a second time "
                                    + "library addr=0x%x, with new library addr=0x%x",
                            mRemoteLibInfo.mLoadAddress, newRemote.mLoadAddress);
                }
                return;
            }
            mRemoteLibInfo = newRemote;
            if (mState == State.DONE) {
                atomicReplaceRelroLocked(false /* relroAvailableImmediately */);
            } else if (mState != State.DONE_PROVIDE_RELRO) {
                // Wake up blocked callers of waitForSharedRelrosLocked().
                mLock.notifyAll();
            }
        }
    }

    @IntDef({Linker.RelroSharingMode.NO_SHARING, Linker.RelroSharingMode.PRODUCE,
            Linker.RelroSharingMode.CONSUME})
    @Retention(RetentionPolicy.SOURCE)
    protected @interface RelroSharingMode {
        // Do not attempt to create or use a RELRO region.
        int NO_SHARING = 0;

        // Produce a shared memory region with relocations.
        int PRODUCE = 1;

        // Load the library and (potentially later) use the externally provided region.
        int CONSUME = 2;
    }

    /**
     * Linker-specific entry point for library loading. Loads the library into the address range
     * provided by mLocalLibInfo. The ModernLinker assumes that the range is reserved with
     * mmap(2), the LegacyLinker will attempt to reserve it.
     *
     * If the library is within a zip file, it must be uncompressed and page aligned in this file.
     *
     * This method may block by calling {@link #waitForSharedRelrosLocked()}. This would
     * synchronously wait until {@link #takeSharedRelrosFromBundle(Bundle)} is called on another
     * thread. Used only in LegacyLinker,
     *
     * If blocking is avoided in a subclass (for performance reasons) then
     * {@link #atomicReplaceRelroLocked(boolean)} must be implemented to *atomically* replace the
     * RELRO region. Atomicity is required because the library code can be running concurrently on
     * another thread. Used only in ModernLinker.
     *
     * @param libraryName The name of the library to load.
     * @param relroMode Tells whether to use RELRO sharing and whether to produce or consume the
     *          RELRO region.
     */
    protected abstract void loadLibraryImplLocked(
            String libraryName, @RelroSharingMode int relroMode);

    /**
     * Atomically replaces the RELRO with the shared memory region described in the
     * |mRemoteLibInfo|. In order to perform the replacement verifies that the replacement is safe
     * by inspecting |mLocalLibInfo| for equality of the library address range and the contents of
     * the RELRO region.
     *
     * By *not* calling {@link #waitForSharedRelrosLocked()} when loading the library subclasses opt
     * into supporting the atomic replacement of RELRO and override this method.
     * @param relroAvailableImmediately Whether the RELRO bundle arrived before
     * {@link #loadLibraryImplLocked(String, int)} was called.
     */
    protected void atomicReplaceRelroLocked(boolean relroAvailableImmediately) {
        assert false;
    }

    /** Loads the Linker JNI library. Throws UnsatisfiedLinkError on error. */
    @SuppressLint({"UnsafeDynamicallyLoadedCode"})
    @GuardedBy("mLock")
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void loadLinkerJniLibraryLocked() {
        assert mState == State.UNINITIALIZED;

        LibraryLoader.setEnvForNative();
        if (DEBUG) Log.i(TAG, "Loading lib%s.so", LINKER_JNI_LIBRARY);

        // May throw UnsatisfiedLinkError, we do not catch it as we cannot continue if we cannot
        // load the linker. Technically we could try to load the library with the system linker on
        // Android M+, but this should never happen, better to catch it in crash reports.
        System.loadLibrary(LINKER_JNI_LIBRARY);
    }

    /**
     * Initializes the auxiliary native library unless it was initialized before.
     *
     * Initializes as a RELRO producer without knowledge about preferred placement of the RELRO
     * region. Should only be used as the last resort: when the simplicity of avoiding the explicit
     * initialization is preferred over memory savings, such as in tests.
     */
    protected final void ensureInitializedImplicitlyAsLastResort() {
        ensureInitialized(
                /* asRelroProducer= */ true, PreferAddress.RESERVE_RANDOM, /* addressHint= */ 0);
    }

    // Used by the LegacyLinker to wait for shared RELROs. Returns once takeSharedRelrosFromBundle()
    // has been called to supply a valid shared RELROs bundle.
    @GuardedBy("mLock")
    protected final void waitForSharedRelrosLocked() {
        if (DEBUG) Log.i(TAG, "waitForSharedRelros() called");
        mLinkerWasWaitingSynchronously = true;

        // Most likely the relocations already have been provided at this point. If not, wait until
        // takeSharedRelrosFromBundle() notifies about RELROs arrival.
        long startTime = DEBUG ? SystemClock.uptimeMillis() : 0;
        while (mRemoteLibInfo == null) {
            try {
                mLock.wait();
            } catch (InterruptedException e) {
                // Continue waiting even if just interrupted.
            }
        }

        if (DEBUG) {
            Log.i(TAG, "Time to wait for shared RELRO: %d ms",
                    SystemClock.uptimeMillis() - startTime);
        }
    }

    /**
     * Holds the information for a given native library or the address range for the future library
     * load. Owns the shared RELRO file descriptor.
     *
     * Native code accesses the fields of this class by name. Renaming should be done on C++ size as
     * well.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    @JniIgnoreNatives
    static class LibInfo implements Parcelable {
        private static final String EXTRA_LINKER_LIB_INFO = "libinfo";

        LibInfo() {}

        // from Parcelable
        LibInfo(Parcel in) {
            // See below in writeToParcel() for the serialization protocol.
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

    // Intentionally omitting @NativeMethods because generation of the stubs it requires (as
    // GEN_JNI.java) is disabled by the @JniIgnoreNatives.
    interface Natives {
        /**
         * Reserves a memory region (=mapping) of sufficient size to hold the loaded library before
         * the real size is known. The mmap(2) being used here provides built in randomization.
         *
         * On failure |libInfo.mLoadAddress| should be set to 0. Observing it a subclass can:
         * 1. Fail early and let LibraryLoader fall back to loading using the system linker
         *    (ModernLinker)
         * 2. Try again (LegacyLinker)
         *
         * @param libInfo holds the output values: |mLoadAddress| and |mLoadSize|. On failure sets
         *                the |libInfo.mLoadAddress| to 0.
         * @param keepReserved should normally be |true|. Setting |keepReserved=false| is intended
         *                     for the legacy behavior within the LegacyLinker. This way the address
         *                     range is freed up (unmapped) immediately after being reserved.
         */
        void findMemoryRegionAtRandomAddress(@NonNull LibInfo libInfo, boolean keepReserved);

        /**
         * Reserves the fixed address range starting at |libInfo.mLoadAddress| big enough to load
         * the main native library. The size of the range is an internal detail of the native
         * implementation.
         *
         * @param libInfo holds the output values: |mLoadAddress| and |mLoadSize|. On success
         *                returns the size in |libInfo.mLoadSize|. On failure sets the
         *                |libInfo.mLoadAddress| to 0.
         */
        void reserveMemoryForLibrary(@NonNull LibInfo libInfo);

        /**
         * Finds the (named) address range reservation made by the system zygote and dedicated for
         * loading the native library. Reads /proc/self/maps, which is a slow operation (up to a few
         * ms). TODO(pasko): collect measurements of the duration.
         *
         * @param libInfo holds the output values: |mLoadAddress| and |mLoadSize|. On success saves
         *                the start address and the size of the webview memory reservation to them.
         * @return whether the region was found.
         */
        boolean findRegionReservedByWebViewZygote(@NonNull LibInfo libInfo);
    }

    private static Linker.Natives sNativesInstance;

    static void setNativesForTesting(Natives instance) {
        sNativesInstance = instance;
        sLinkerForAssert = null; // Also allow to create Linker multiple times in tests.
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static Linker.Natives getLinkerJni() {
        if (sNativesInstance != null) return sNativesInstance;
        return new LinkerJni(); // R8 optimizes away all construction except the initial one.
    }
}
