// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_TAB_FAVICON_OBSERVER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_TAB_FAVICON_OBSERVER_H_

#include <map>
#include <memory>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

class Profile;

namespace glic {

// Observers tabs for favicon changes. Plumbs these changes to the provided
// mojo receiver.
class GlicTabFaviconObserver {
 public:
  explicit GlicTabFaviconObserver(Profile* profile);
  ~GlicTabFaviconObserver();

  GlicTabFaviconObserver(const GlicTabFaviconObserver&) = delete;
  GlicTabFaviconObserver& operator=(const GlicTabFaviconObserver&) = delete;

  // Request receiving updates for a tab's favicon. The remote receives updates
  // as long as the mojo pipe is open. The pipe is automatically closed when the
  // subscribed tab no longer exists.
  void SubscribeToTabFavicon(
      int32_t tab_id,
      mojo::PendingRemote<mojom::TabFaviconHandler> remote);

  // For internal use.
  void ScheduleCleanupForTab(tabs::TabHandle tab_handle);

 private:
  class TabObserver;

  void DoCleanup();
  void OnTabWillClose(tabs::TabHandle tab_handle);

  absl::flat_hash_map<tabs::TabHandle, std::unique_ptr<TabObserver>> observers_;
  absl::flat_hash_set<tabs::TabHandle> pending_cleanup_;
  base::OneShotTimer cleanup_timer_;
  const raw_ptr<Profile> profile_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_TAB_FAVICON_OBSERVER_H_
