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
 *   device. See getRandomBaseLoadAddress() for more details on how this is obtained.
 *
 * Usage:
 *
 * - The native shared library must be loaded with Linker.loadLibrary(), instead of
 *   System.loadLibrary(). The two functions should behave the same (at a high level).
 *
 * - Before loading the library, setApkFilePath() must be called when loading from the APK.
 *
 * - Early on, before the attempt to load the library, the linker needs to be initialized either as
 *   a provider or a consumer of the RELRO region. Depending on the choice either
 *   initAsRelroProducer() or initAsRelroConsumer() should be invoked. Since various Chromium
 *   projects have vastly different initialization paths, for convenience the initialization runs
 *   implicitly as part of loading the library. In this case the behaviour is of a producer.
 *
 * - When running as a RELRO consumer, the loadLibrary() may block until the RELRO section Bundle
 *   is received. This is done by calling takeSharedRelrosFromBundle() from another thread.
 *
 * - After loading the native library as a RELRO producer, the putSharedRelrosToBundle() becomes
 *   available to then send the Bundle to Linkers in other processes.
 */
@JniIgnoreNatives
abstract class Linker {
    private static final String TAG = "Linker";

    // Name of the library that contains the JNI code.
    protected static final String LINKER_JNI_LIBRARY = "chromium_android_linker";

    // Set to true to enable debug logs.
    protected static final boolean DEBUG = false;

    // Constants used to pass the shared RELRO Bundle through Binder.
    private static final String SHARED_RELROS = "org.chromium.base.android.linker.shared_relros";
    private static final String BASE_LOAD_ADDRESS =
            "org.chromium.base.android.linker.base_load_address";

    protected final Object mLock = new Object();

    @GuardedBy("mLock")
    protected LibInfo mLibInfo;

    // Whether this Linker instance should potentially create the RELRO region. Even if true, the
    // library loading can fall back to the system linker without producing the region. The default
    // value is used in tests, it is set to true so that the Linker does not have to wait for RELRO
    // to arrive from another process.
    @GuardedBy("mLock")
    protected boolean mRelroProducer = true;

    // Current common random base load address. A value of -1 indicates not yet initialized.
    @GuardedBy("mLock")
    protected long mBaseLoadAddress = -1;

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

    /**
     * Initializes the Linker and ensures that after loading the native library the RELRO region
     * will be available for sharing with other processes via
     * {@link #putSharedRelrosToBundle(Bundle)}.
     */
    final void initAsRelroProducer() {
        synchronized (mLock) {
            mRelroProducer = true;
            ensureInitializedLocked();
            if (DEBUG) Log.i(TAG, "initAsRelroProducer() chose address=0x%x", mBaseLoadAddress);
        }
    }

    /**
     * Initializes the Linker in the mode prepared to receive a RELRO region information from
     * another process. Arrival of the RELRO region may block loading the native library in this
     * process.
     *
     * @param baseLoadAddress the base library load address to use.
     */
    final void initAsRelroConsumer(long baseLoadAddress) {
        if (DEBUG) Log.i(TAG, "initAsRelroConsumer(0x%x) called", baseLoadAddress);
        synchronized (mLock) {
            mRelroProducer = false;
            ensureInitializedLocked();
            mBaseLoadAddress = baseLoadAddress;
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
        synchronized (mLock) {
            ensureInitializedLocked();
            if (mBaseLoadAddress != 0) {
                bundle.putLong(BASE_LOAD_ADDRESS, mBaseLoadAddress);
            }
        }
    }

    /**
     * Obtains a random base load address at which to place loaded libraries.
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
        // the fallback to no RELRO sharing guarantees correctness.
        final long address = nativeGetRandomBaseLoadAddress();
        if (DEBUG) Log.i(TAG, "Random native base load address: 0x%x", address);
        return address;
    }

    /** Tell the linker about the APK path, if the library is loaded from the APK. */
    void setApkFilePath(String path) {}

    /**
     * Loads a native shared library with the Chromium linker. Note the crazy linker treats
     * libraries and files as equivalent, so you can only open one library in a given zip
     * file. The library must not be the Chromium linker library.
     *
     * @param library The library name to load.
     * @param isFixedAddressPermitted Whether the library can be loaded at a fixed address for RELRO
     * sharing.
     */
    final void loadLibrary(String library, boolean isFixedAddressPermitted) {
        if (DEBUG) Log.i(TAG, "loadLibrary: %s", library);
        assert !library.equals(LINKER_JNI_LIBRARY);
        synchronized (mLock) {
            ensureInitializedLocked();
            try {
                loadLibraryImplLocked(library, isFixedAddressPermitted);
                if (!mLinkerWasWaitingSynchronously && mLibInfo != null && mState == State.DONE) {
                    atomicReplaceRelroLocked(true /* relroAvailableImmediately */);
                }
            } finally {
                // Reset the state to serve the retry with |isFixedAddressPermitted=false|.
                mLinkerWasWaitingSynchronously = false;
            }
        }
    }

    /**
     * Serializes information and about the RELRO region to be passed to a Linker in another
     * process.
     * @param bundle The Bundle to serialize to.
     */
    void putSharedRelrosToBundle(Bundle bundle) {
        Bundle relros = null;
        synchronized (mLock) {
            if (mState == State.DONE_PROVIDE_RELRO) {
                assert mRelroProducer;
                relros = mLibInfo.toBundle();
            }
        }
        bundle.putBundle(SHARED_RELROS, relros);
        if (DEBUG) Log.i(TAG, "putSharedRelrosToBundle() puts %s", relros);
    }

    /**
     * Deserializes the RELRO region information that was marshalled by
     * {@link #putLoadAddressToBundle(Bundle)} and wakes up the threads waiting for it to use (mmap)
     * replace the RELRO section in this process with shared memory.
     * @param bundle The Bundle to extract the information from.
     */
    void takeSharedRelrosFromBundle(Bundle bundle) {
        if (DEBUG) Log.i(TAG, "called takeSharedRelrosFromBundle(%s)", bundle);
        Bundle relros = bundle.getBundle(SHARED_RELROS);
        if (relros != null) {
            synchronized (mLock) {
                assert mLibInfo == null;
                mLibInfo = LibInfo.fromBundle(relros);
                if (mState == State.DONE) {
                    atomicReplaceRelroLocked(false /* relroAvailableImmediately */);
                } else {
                    assert mState != State.DONE_PROVIDE_RELRO;
                    // Wake up blocked callers of waitForSharedRelrosLocked().
                    mLock.notifyAll();
                }
            }
        }
    }

    /**
     * Loads the native library.
     *
     * If the library is within a zip file, it must be uncompressed and page aligned in this file.
     *
     * This method may block by calling {@link #waitForSharedRelrosLocked()}. This would
     * synchronously wait until {@link #takeSharedRelrosFromBundle(Bundle)} is called on another
     * thread.
     *
     * If blocking is avoided in a subclass (for performance reasons) then
     * {@link #atomicReplaceRelroLocked(boolean)} must be implemented to *atomically* replace the
     * RELRO region. Atomicity is required because the library code can be running concurrently on
     *    another thread.
     *
     * @param libFilePath The path of the library (possibly in the zip file).
     * @param isFixedAddressPermitted If true, uses a fixed load address if one was
     * supplied, otherwise ignores the fixed address and loads wherever available.
     */
    abstract void loadLibraryImplLocked(String libFilePath, boolean isFixedAddressPermitted);

    /**
     * Atomically replaces the RELRO with the shared memory region described in the |mLibInfo|.
     *
     * By *not* calling {@link #waitForSharedRelrosLocked()} when loading the library subclasses opt
     * into supporting the atomic replacement of RELRO and override this method.
     * @param relroAvailableImmediately Whether the RELRO bundle arrived before
     * {@link #loadLibraryImplLocked(String, boolean)} was called.
     */
    protected void atomicReplaceRelroLocked(boolean relroAvailableImmediately) {
        assert false;
    }

    /** Loads the Linker JNI library. Throws UnsatisfiedLinkError on error. */
    @SuppressLint({"UnsafeDynamicallyLoadedCode"})
    @GuardedBy("mLock")
    private void loadLinkerJniLibraryLocked() {
        assert mState == State.UNINITIALIZED;

        LibraryLoader.setEnvForNative();
        if (DEBUG) Log.i(TAG, "Loading lib%s.so", LINKER_JNI_LIBRARY);

        // May throw UnsatisfiedLinkError, we do not catch it as we cannot continue if we cannot
        // load the linker. Technically we could try to load the library with the system linker on
        // Android M+, but this should never happen, better to catch it in crash reports.
        System.loadLibrary(LINKER_JNI_LIBRARY);
    }

    // Used internally to initialize the linker's data. Loads JNI.
    @GuardedBy("mLock")
    protected final void ensureInitializedLocked() {
        if (mState != State.UNINITIALIZED) return;

        loadLinkerJniLibraryLocked();

        if (mRelroProducer) {
            assert mBaseLoadAddress == -1;
            mBaseLoadAddress = getRandomBaseLoadAddress();
        }

        mState = State.INITIALIZED;
    }

    // Used internally to wait for shared RELROs. Returns once provideSharedRelros() has been
    // called to supply a valid shared RELROs bundle.
    @GuardedBy("mLock")
    protected final void waitForSharedRelrosLocked() {
        if (DEBUG) Log.i(TAG, "waitForSharedRelros() called");
        mLinkerWasWaitingSynchronously = true;

        // Wait until notified by provideSharedRelros() that shared RELROs have arrived.
        //
        // Note that the relocations may already have been provided by the time we arrive here, so
        // this may return immediately.
        long startTime = DEBUG ? SystemClock.uptimeMillis() : 0;
        while (mLibInfo == null) {
            try {
                mLock.wait();
            } catch (InterruptedException e) {
                // Continue waiting even if we were just interrupted.
            }
        }

        if (DEBUG) {
            Log.i(TAG, "Time to wait for shared RELRO: %d ms",
                    SystemClock.uptimeMillis() - startTime);
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

    /**
     * Returns a random address that should be free to be mapped with the given size.
     * Maps an area large enough for the largest library we might attempt to load,
     * and if successful then unmaps it and returns the address of the area allocated
     * by the system (with ASLR). The idea is that this area should remain free of
     * other mappings until we map our library into it.
     *
     * @return address to pass to future mmap, or 0 on error.
     */
    private static native long nativeGetRandomBaseLoadAddress();
}
