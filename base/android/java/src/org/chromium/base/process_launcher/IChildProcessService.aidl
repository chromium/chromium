// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import android.content.pm.ApplicationInfo;
import android.os.Bundle;

import org.chromium.base.process_launcher.IParentProcess;

interface IChildProcessService {
  // |clazz| identifies the ClassLoader of the caller.
  // On the first call to this method, the service will record the calling PID
  // and |clazz| and return true. Subsequent calls will only return true if the
  // calling PID and |clazz| matches the recorded values.
  boolean bindToCaller(in String clazz);

  // Get the ApplicationInfo object used to load the code and resources of the
  // child process, for validating that the parent is talking to a "matching"
  // process.
  ApplicationInfo getAppInfo();

  // Sets up the initial IPC channel.
  oneway void setupConnection(in Bundle args, IParentProcess parentProcess,
          in List<IBinder> clientInterfaces, in IBinder binderBox);

  // Forcefully kills the child process.
  oneway void forceKill();

  // Notifies about memory pressure. The argument is MemoryPressureLevel enum.
  oneway void onMemoryPressure(int pressure);

  // Dumps the stack for the child process without crashing it.
  oneway void dumpProcessStack();

  // Takes the |bundle| potentially containing the shared memory region and
  // uses it to replace the memory behind read only relocations in the child
  // process. On error the bundle is silently ignored, disabling the memory
  // optimization.
  oneway void consumeRelroBundle(in Bundle bundle);
}
