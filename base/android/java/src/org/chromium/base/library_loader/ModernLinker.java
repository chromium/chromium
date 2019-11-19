// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.annotations.JniIgnoreNatives;

import javax.annotation.concurrent.GuardedBy;

/**
 * Provides a concrete implementation of the Chromium Linker.
 *
 * This Linker implementation uses the Android M and later system linker to map Chrome and call
 * |JNI_OnLoad()|.
 *
 * For more on the operations performed by the Linker, see {@link Linker}.
 */
@JniIgnoreNatives
class ModernLinker extends Linker {
    // Log tag for this class.
    private static final String TAG = "ModernLinker";

    ModernLinker() {}

    @Override
    @GuardedBy("sLock")
    void loadLibraryImplLocked(String library, boolean isFixedAddressPermitted) {
        // We expect to load monochrome, if it's not the case, log.
        if (!"monochrome".equals(library) || DEBUG) {
            Log.i(TAG, "loadLibraryImpl: %s, %b", library, isFixedAddressPermitted);
        }

        ensureInitializedLocked();
        assert mState == State.INITIALIZED; // Only one successful call.

        String libFilePath = System.mapLibraryName(library);
        boolean loadNoRelro = !isFixedAddressPermitted;
        boolean provideRelro = isFixedAddressPermitted && mInBrowserProcess;
        long loadAddress = isFixedAddressPermitted ? mBaseLoadAddress : 0;

        if (loadNoRelro) {
            // Cannot use System.loadLibrary(), as the library name is transformed (adding the "lib"
            // prefix and ".so" suffix), making the name incorrect.
            boolean ok = nativeLoadLibraryNoRelros(libFilePath);
            if (!ok) resetAndThrow("Cannot load without relro sharing");
            mState = State.DONE;
        } else if (provideRelro) {
            // We are in the browser, and with a current load address that indicates that there
            // is enough address space for shared RELRO to operate. Create the shared RELRO, and
            // store it in the map.
            String relroPath = PathUtils.getDataDirectory() + "/RELRO:" + libFilePath;
            LibInfo libInfo = new LibInfo();
            libInfo.mLibFilePath = libFilePath;
            if (!nativeLoadLibraryCreateRelros(libFilePath, loadAddress, relroPath, libInfo)) {
                Log.e(TAG, "Unable to create relro, retrying without");
                nativeLoadLibraryNoRelros(libFilePath);
                libInfo.mRelroFd = -1;
            }
            mLibInfo = libInfo;
            // Next state is still to provide relro (even if we don't have any), as child processes
            // would wait for them.
            mState = State.DONE_PROVIDE_RELRO;
        } else {
            // We are in a service process, again with a current load address that is suitable for
            // shared RELRO, and we are to wait for shared RELROs. So do that, then use the LibInfo
            // we received.
            waitForSharedRelrosLocked();
            assert libFilePath.equals(mLibInfo.mLibFilePath);
            if (!nativeLoadLibraryUseRelros(libFilePath, loadAddress, mLibInfo.mRelroFd)) {
                resetAndThrow(String.format("Unable to load library: %s", libFilePath));
            }

            mLibInfo.close();
            mLibInfo = null;
            mState = State.DONE;
        }

        // Load the library a second time, in order to keep using lazy JNI registration.  When
        // loading the library with the Chromium linker, ART doesn't know about our library, so
        // cannot resolve JNI methods lazily. Loading the library a second time makes sure it
        // knows about us.
        //
        // This is not wasteful though, as libraries are reference-counted, and as a consequence the
        // library is not really loaded a second time, and we keep relocation sharing.
        try {
            System.loadLibrary(library);
        } catch (UnsatisfiedLinkError e) {
            throw new UnsatisfiedLinkError(
                    "Unable to load the library a second time with the system linker");
        }
    }

    @GuardedBy("sLock")
    private void resetAndThrow(String message) {
        mState = State.INITIALIZED;
        Log.e(TAG, message);
        throw new UnsatisfiedLinkError(message);
    }

    private static native boolean nativeLoadLibraryCreateRelros(
            String dlopenExtPath, long loadAddress, String relroPath, LibInfo libInfo);
    private static native boolean nativeLoadLibraryUseRelros(
            String dlopenExtPath, long loadAddress, int fd);
    private static native boolean nativeLoadLibraryNoRelros(String dlopenExtPath);
}
