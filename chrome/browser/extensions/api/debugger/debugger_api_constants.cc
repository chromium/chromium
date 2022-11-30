// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/debugger/debugger_api_constants.h"

namespace debugger_api_constants {

const char kAlreadyAttachedError[] =
    "Another debugger is already attached to the * with id: *.";
const char kNoTargetError[] = "No * with given id *.";
const char kInvalidTargetError[] =
    "Either tab id or extension id must be specified.";
const char kNotAttachedError[] =
    "Debugger is not attached to the * with id: *.";
const char kProtocolVersionNotSupportedError[] =
    "Requested protocol version is not supported: *.";
const char kSilentDebuggingRequired[] =
    "Cannot attach to this target unless '*' flag is enabled.";
const char kRestrictedError[] = "Cannot attach to this target.";
const char kDetachedWhileHandlingError[] = "Detached while handling command.";

const char kTabTargetType[] = "tab";
const char kBackgroundPageTargetType[] = "background page";
const char kOpaqueTargetType[] = "target";

}  // namespace debugger_api_constants
