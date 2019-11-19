// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGING_LAUNCH_FROM_NATIVE_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGING_LAUNCH_FROM_NATIVE_H_

#include <string>
#include "base/macros.h"
#include "base/time/time.h"

class Profile;

namespace extensions {

// Returns whether |extension_id| running in |profile| is allowed to accept
// connections from native host named |host_id|.
bool ExtensionSupportsConnectionFromNativeApp(const std::string& extension_id,
                                              const std::string& host_id,
                                              Profile* profile,
                                              bool log_errors);

// Creates a native messaging connection between the extension with ID
// |extension_id| with |profile| and the native messaging host with name
// |host_id|.
void LaunchNativeMessageHostFromNativeApp(const std::string& extension_id,
                                          const std::string& host_id,
                                          const std::string& connection_id,
                                          Profile* profile);

class ScopedAllowNativeAppConnectionForTest {
 public:
  explicit ScopedAllowNativeAppConnectionForTest(bool allow);
  ~ScopedAllowNativeAppConnectionForTest();

  bool allow() const { return allow_; }

 private:
  const bool allow_;

  DISALLOW_COPY_AND_ASSIGN(ScopedAllowNativeAppConnectionForTest);
};

class ScopedNativeMessagingErrorTimeoutOverrideForTest {
 public:
  explicit ScopedNativeMessagingErrorTimeoutOverrideForTest(
      base::TimeDelta timeout);
  ~ScopedNativeMessagingErrorTimeoutOverrideForTest();

  base::TimeDelta timeout() const { return timeout_; }

 private:
  const base::TimeDelta timeout_;

  DISALLOW_COPY_AND_ASSIGN(ScopedNativeMessagingErrorTimeoutOverrideForTest);
};
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGING_LAUNCH_FROM_NATIVE_H_
