// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.IBinder;
import android.util.SparseArray;

import java.util.List;

/** The interface that embedders should implement to specialize child service creation. */
public interface ChildProcessServiceDelegate {
    /** Invoked when the service was created. This is the first method invoked on the delegate. */
    void onServiceCreated();

    /**
     * Called when the service is bound. Invoked on a background thread.
     * @param intent the intent that started the service.
     */
    void onServiceBound(Intent intent);

    /**
     * Called once the connection has been setup. Invoked on a background thread.
     *
     * @param connectionBundle the bundle pass to the setupConnection call
     * @param clientInterfaces the IBinders interfaces provided by the client
     * @param binderBox an optional binder box which may contain other binders to be unpacked
     */
    void onConnectionSetup(
            Bundle connectionBundle, List<IBinder> clientInterfaces, IBinder binderBox);

    /**
     * Called when the delegate should load the native library.
     *
     * @param hostContext The host context the library should be loaded with (i.e. Chrome).
     */
    void loadNativeLibrary(Context hostContext);

    /**
     * Called when the delegate should preload the native library.
     * Preloading is automatically done during library loading, but can also be called explicitly
     * to speed up the loading. See {@link LibraryLoader#preloadNow()}.
     * @param packageName The package name the library should be preloaded with (i.e.
     * org.chromium.chrome).
     */
    void preloadNativeLibrary(String packageName);

    /**
     * Takes the shared memory region containing read-only relocations, to save memory after loading
     * the native library in the child process.
     * @param bundle potentially holds the description of the shared memory region transferred
     * between processes from one {@link org.chromium.base.library_loader.Linker} to another.
     */
    void consumeRelroBundle(Bundle bundle);

    /**
     * Should return a map that associatesfile descriptors' IDs to keys.
     * This is needed as at the moment we use 2 different stores for the FDs in native code:
     * base::FileDescriptorStore which associates FDs with string identifiers (the key), and
     * base::GlobalDescriptors which associates FDs with int ids.
     * FDs for which the returned map contains a mapping are added to base::FileDescriptorStore with
     * the associated key, all others are added to base::GlobalDescriptors.
     */
    SparseArray<String> getFileDescriptorsIdsToKeys();

    /** Called before the main method is invoked. */
    void onBeforeMain();

    /**
     * The main entry point for the service. This method should block as long as the service should
     * be running.
     */
    void runMain();
}
