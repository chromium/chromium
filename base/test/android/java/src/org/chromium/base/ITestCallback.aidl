// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.chromium.base.ITestController;
import org.chromium.base.process_launcher.FileDescriptorInfo;

/**
 * This interface is called by the child process to pass its controller to its parent.
 */
interface ITestCallback {
  oneway void childConnected(ITestController controller);

  /**
    * Invoked by the service to notify that the main method returned.
    * IMPORTANT! Should not be marked oneway as the caller will terminate the running process after
    * this call. Marking it oneway would make the call asynchronous and the process could terminate
    * before the call was actually sent.
    */
  void mainReturned(int returnCode);
}
