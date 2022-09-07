// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.chromium.base.process_launcher.FileDescriptorInfo;

/**
 * This interface is used to control child processes.
 */
interface ITestController {
  /**
   * Forces the service process to terminate and block until the process stops.
   * @param exitCode the exit code the process should terminate with.
   * @return always true, a return value is only returned to force the call to be synchronous.
   */
  boolean forceStopSynchronous(int exitCode);

  /**
   * Forces the service process to terminate.
   * @param exitCode the exit code the process should terminate with.
   */
  oneway void forceStop(int exitCode);
}
