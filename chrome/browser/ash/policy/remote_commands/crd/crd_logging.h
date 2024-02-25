// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_CRD_LOGGING_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_CRD_LOGGING_H_

#include "base/logging.h"

// Add a common prefix to all our logs, to make them easy to find.
#define CRD_VLOG(level) VLOG(level) << "CRD: "
#define CRD_LOG(level) LOG(level) << "CRD: "

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_CRD_LOGGING_H_
