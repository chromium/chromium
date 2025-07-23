// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import org.chromium.base.process_launcher.IChildProcessArgs;
import org.chromium.base.process_launcher.IParentProcess;
import org.chromium.base.library_loader.IRelroLibInfo;

interface IChildProcessService {
  // |clazz| identifies the ClassLoader of the caller.
  // On the first call to this method, the service will record the calling PID
  // and |clazz| and return true. Subsequent calls will only return true if the
  // calling PID and |clazz| matches the recorded values.
  boolean bindToCaller(in String clazz);

  // Returns an array of 2 strings: sourceDir and a colon-separated list of
  // sharedLibraryFiles, for validating that the parent is talking to a "matching"
  // process.
  String[] getAppInfoStrings();

  // Sets up the initial IPC channel.
  oneway void setupConnection(in IChildProcessArgs args, in IParentProcess parentProcess,
           in @nullable List<IBinder> clientInterfaces);

  // Forcefully kills the child process.
  oneway void forceKill();

  // Notifies about memory pressure. The argument is MemoryPressureLevel enum.
  oneway void onMemoryPressure(int pressure);

  // Notifies that we should freeze ourselves (as opposed to relying on  App
  // Freezer).
  oneway void onSelfFreeze();

  // Dumps the stack for the child process without crashing it.
  oneway void dumpProcessStack();

  // Takes the |libInfo| potentially containing the shared memory region and
  // uses it to replace the memory behind read only relocations in the child
  // process. On error the bundle is silently ignored, disabling the memory
  // optimization.
  oneway void consumeRelroLibInfo(in @nullable IRelroLibInfo libInfo);
}
