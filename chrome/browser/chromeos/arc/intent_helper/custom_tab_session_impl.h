// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_INTENT_HELPER_CUSTOM_TAB_SESSION_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_INTENT_HELPER_CUSTOM_TAB_SESSION_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ui/ash/arc_custom_tab_modal_dialog_host.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace ash {
class ArcCustomTab;
}  // namespace ash

namespace content {
class WebContents;
}  // namespace content

// Implementation of CustomTabSession interface.
class CustomTabSessionImpl : public arc::mojom::CustomTabSession,
                             public ArcCustomTabModalDialogHost {
 public:
  static arc::mojom::CustomTabSessionPtr Create(
      std::unique_ptr<content::WebContents> web_contents,
      std::unique_ptr<ash::ArcCustomTab> custom_tab);

  // arc::mojom::CustomTabSession:
  void OnOpenInChromeClicked() override;

 private:
  CustomTabSessionImpl(std::unique_ptr<content::WebContents> web_contents,
                       std::unique_ptr<ash::ArcCustomTab> custom_tab);
  ~CustomTabSessionImpl() override;

  void Bind(arc::mojom::CustomTabSessionPtr* ptr);

  void Close();

  // Used to bind the CustomTabSession interface implementation to a message
  // pipe.
  mojo::Binding<arc::mojom::CustomTabSession> binding_;

  // Tracks the lifetime of the ARC Custom Tab session.
  base::ElapsedTimer lifetime_timer_;

  // Set to true when the user requests to view the web contents in a normal
  // Chrome tab instead of an ARC Custom Tab.
  bool forwarded_to_normal_tab_ = false;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CustomTabSessionImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CustomTabSessionImpl);
};

#endif  // CHROME_BROWSER_CHROMEOS_ARC_INTENT_HELPER_CUSTOM_TAB_SESSION_IMPL_H_
