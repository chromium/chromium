// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SWITCHER_BHO_LOGGING_H_
#define CHROME_BROWSER_BROWSER_SWITCHER_BHO_LOGGING_H_

#include <cassert>
#include <fstream>

enum LogLevels { OFF = 0, ERR = 1, WARNING = 2, INFO = 3 };

extern std::wostream* gLogStream;
extern LogLevels gLogLevel;

void InitLog(const std::wstring& file);
void CloseLog();
void SetLogLevel(LogLevels level);

#define INFO_MSG "[info] : "
#define WARNING_MSG "[WARN] : "
#define ERR_MSG "[*ERROR!*] : "

#define LOG(a)        \
  if (gLogLevel >= a) \
  *gLogStream << (a##_MSG) << __FILE__ << ":" << __LINE__ << " : "

#define DCHECK(a) assert(a)
#define NOTREACHED() assert(false)

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_BHO_LOGGING_H_
