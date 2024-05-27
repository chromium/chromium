// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SWITCHER_ALTERNATIVE_BROWSER_DRIVER_H_
#define CHROME_BROWSER_BROWSER_SWITCHER_ALTERNATIVE_BROWSER_DRIVER_H_

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"

class GURL;

namespace browser_switcher {

class BrowserSwitcherPrefs;

// Used for UMA metrics, so do NOT change the order and the numbers. If you want
// to add a new type, only append/increment.
enum class BrowserType {
  kUnknown = 0,
  kIE = 1,
  kFirefox = 2,
  kOpera = 3,
  kSafari = 4,
  kChrome = 5,
  kEdge = 6,
  kMaxValue = kEdge,
};

// Interface for a helper class that does I/O operations for an
// |AlternativeBrowserLauncher|.
//
// - Reading from the Windows Registry
// - Communicating with an external process using the DDE protocol
// - Launching an external process
class AlternativeBrowserDriver {
 public:
  using LaunchCallback = base::OnceCallback<void(bool success)>;

  virtual ~AlternativeBrowserDriver();

  // Tries to launch |browser| at the specified URL, using whatever
  // method is most appropriate.
  virtual void TryLaunch(const GURL& url, LaunchCallback cb) = 0;

  // Returns the localized string for the name of the alternative browser, if it
  // was auto-detected. If the name couldn't be auto-detected, returns an empty
  // string.
  virtual std::string GetBrowserName() const = 0;

  // Returns the type of browser as an enum, if it was auto-detected. Otherwise,
  // returns |BrowserType::kUnknown|.
  virtual BrowserType GetBrowserType() const = 0;
};

// Default concrete implementation for |AlternativeBrowserDriver|. Uses a
// platform-specific method to locate and launch the appropriate browser.
class AlternativeBrowserDriverImpl : public AlternativeBrowserDriver {
 public:
  explicit AlternativeBrowserDriverImpl(const BrowserSwitcherPrefs* prefs);
  ~AlternativeBrowserDriverImpl() override;

  AlternativeBrowserDriverImpl(const AlternativeBrowserDriverImpl&) = delete;
  AlternativeBrowserDriverImpl& operator=(const AlternativeBrowserDriverImpl&) =
      delete;

  // AlternativeBrowserDriver
  void TryLaunch(const GURL& url, LaunchCallback cb) override;
  std::string GetBrowserName() const override;
  BrowserType GetBrowserType() const override;

  // Create the CommandLine object that would be used to launch an external
  // process.
  base::CommandLine CreateCommandLine(const GURL& url);

 private:
  using StringType = base::FilePath::StringType;

  const raw_ptr<const BrowserSwitcherPrefs> prefs_;
};

}  // namespace browser_switcher

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_ALTERNATIVE_BROWSER_DRIVER_H_
