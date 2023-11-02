// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

interface IParentProcess {
    // Sends the child pid and information from the app zygote (if any) to the
    // parent process. This will be called before any third-party code is
    // loaded, and will be a no-op after the first call.
    //
    // The |zygotePid| must be 0 if the process does not inherit from an app zygote
    // or its app zygote did not produce a usable shared region containing
    // linker relocations (RELRO FD).
    oneway void finishSetupConnection(int pid, int zygotePid,
        long zygoteStartupTimeMillis, in Bundle relroBundle);

    // Reports exception before calling into native main method. This is before
    // crash reporting is initialized, which means this exception would
    // otherwise not be reported.
    // Not oneway to ensure the browser receives the message before child exits.
    void reportExceptionInInit(in String exception);

    // Tells the parent proces the child exited cleanly. Not oneway to ensure
    // the browser receives the message before child exits.
    void reportCleanExit();
}
