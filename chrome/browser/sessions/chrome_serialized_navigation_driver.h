// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_CHROME_SERIALIZED_NAVIGATION_DRIVER_H_
#define CHROME_BROWSER_SESSIONS_CHROME_SERIALIZED_NAVIGATION_DRIVER_H_

#include "components/sessions/content/content_serialized_navigation_driver.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

// Provides an implementation of ContentSerializedNavigationDriver that knows
// about chrome/ specifics.
class ChromeSerializedNavigationDriver
    : public sessions::ContentSerializedNavigationDriver {
 public:
  ChromeSerializedNavigationDriver(const ChromeSerializedNavigationDriver&) =
      delete;
  ChromeSerializedNavigationDriver& operator=(
      const ChromeSerializedNavigationDriver&) = delete;

  ~ChromeSerializedNavigationDriver() override;

  // Returns the singleton ChromeSerializedNavigationDriver.  Almost all
  // callers should use SerializedNavigationDriver::Get() instead.
  static ChromeSerializedNavigationDriver* GetInstance();

  // sessions::ContentSerializedNavigationDriver implementation.
  void Sanitize(sessions::SerializedNavigationEntry* navigation) const override;

 private:
  friend struct base::DefaultSingletonTraits<ChromeSerializedNavigationDriver>;

  ChromeSerializedNavigationDriver();
};

#endif  // CHROME_BROWSER_SESSIONS_CHROME_SERIALIZED_NAVIGATION_DRIVER_H_
