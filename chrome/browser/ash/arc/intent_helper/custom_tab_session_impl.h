// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INTENT_HELPER_CUSTOM_TAB_SESSION_IMPL_H_
#define CHROME_BROWSER_ASH_ARC_INTENT_HELPER_CUSTOM_TAB_SESSION_IMPL_H_

#include <memory>

#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace arc {
class CustomTab;
}  // namespace arc

class Browser;

// Implementation of CustomTabSession interface.
class CustomTabSessionImpl : public arc::mojom::CustomTabSession,
                             public TabStripModelObserver {
 public:
  static mojo::PendingRemote<arc::mojom::CustomTabSession> Create(
      std::unique_ptr<arc::CustomTab> custom_tab,
      Browser* browser);

  // arc::mojom::CustomTabSession:
  void OnOpenInChromeClicked() override;

 private:
  friend class CustomTabSessionImplTest;
  CustomTabSessionImpl(std::unique_ptr<arc::CustomTab> custom_tab,
                       Browser* browser);
  CustomTabSessionImpl(const CustomTabSessionImpl&) = delete;
  CustomTabSessionImpl& operator=(const CustomTabSessionImpl&) = delete;
  ~CustomTabSessionImpl() override;

  void Bind(mojo::PendingRemote<arc::mojom::CustomTabSession>* remote);

  void Close();

  // TabStripModelObserver overrides.
  void TabStripEmpty() override;

  // Used to bind the CustomTabSession interface implementation to a message
  // pipe.
  mojo::Receiver<arc::mojom::CustomTabSession> receiver_{this};

  // Tracks the lifetime of the ARC Custom Tab session.
  base::ElapsedTimer lifetime_timer_;

  // Set to true when the user requests to view the web contents in a normal
  // Chrome tab instead of an ARC Custom Tab.
  bool forwarded_to_normal_tab_ = false;

  // The browser object provides windowing and command controller for the
  // custom tab.
  raw_ptr<Browser> browser_;

  // The custom tab object.
  std::unique_ptr<arc::CustomTab> custom_tab_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CustomTabSessionImpl> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_ASH_ARC_INTENT_HELPER_CUSTOM_TAB_SESSION_IMPL_H_
