// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_BROWSER_CONTEXT_LIFETIME_TRACKER_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_BROWSER_CONTEXT_LIFETIME_TRACKER_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "extensions/browser/browser_context_lifetime_tracker.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace extensions {

// Singleton that lets any //chrome/browser/extensions code (an API
// delegate, a KeyedService, etc.) watch a Profile's off-the-record sibling
// lifecycle via BrowserContextLifetimeObserver, without each site
// reimplementing the underlying ProfileObserver plumbing itself. Reached via
// ExtensionsBrowserClient::GetBrowserContextLifetimeTracker().
class ChromeBrowserContextLifetimeTracker
    : public BrowserContextLifetimeTracker {
 public:
  // Returns the instance of ChromeBrowserContextLifetimeTracker.
  static ChromeBrowserContextLifetimeTracker* GetInstance();

  ChromeBrowserContextLifetimeTracker(
      const ChromeBrowserContextLifetimeTracker&) = delete;
  ChromeBrowserContextLifetimeTracker& operator=(
      const ChromeBrowserContextLifetimeTracker&) = delete;

  // BrowserContextLifetimeTracker:
  void StartObserving(content::BrowserContext& context,
                      BrowserContextLifetimeObserver& observer) override;
  void StopObserving(BrowserContextLifetimeObserver& observer) override;

 private:
  friend class base::NoDestructor<ChromeBrowserContextLifetimeTracker>;

  class ProfileWatcher;

  ChromeBrowserContextLifetimeTracker();
  ~ChromeBrowserContextLifetimeTracker() override;

  std::map<raw_ptr<BrowserContextLifetimeObserver>,
           std::unique_ptr<ProfileWatcher>>
      watchers_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_BROWSER_CONTEXT_LIFETIME_TRACKER_H_
