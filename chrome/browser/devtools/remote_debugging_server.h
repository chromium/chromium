// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_REMOTE_DEBUGGING_SERVER_H_
#define CHROME_BROWSER_DEVTOOLS_REMOTE_DEBUGGING_SERVER_H_

#include <stdint.h>

#include "base/macros.h"

class RemoteDebuggingServer {
 public:
  static void EnableTetheringForDebug();

  RemoteDebuggingServer();
  virtual ~RemoteDebuggingServer();

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteDebuggingServer);
};

#endif  // CHROME_BROWSER_DEVTOOLS_REMOTE_DEBUGGING_SERVER_H_
