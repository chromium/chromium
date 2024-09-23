// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/webauthn_request_registrar_lacros.h"

#include <string>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"
#include "ui/platform_window/platform_window.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_lacros.h"

WebAuthnRequestRegistrarLacros::WebAuthnRequestRegistrarLacros() = default;

WebAuthnRequestRegistrarLacros::~WebAuthnRequestRegistrarLacros() = default;

WebAuthnRequestRegistrarLacros::GenerateRequestIdCallback
WebAuthnRequestRegistrarLacros::GetRegisterCallback(aura::Window* window) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!window) {
    return base::BindRepeating([] { return std::string(); });
  }

  auto* host = views::DesktopWindowTreeHostLacros::From(window->GetHost());
  if (!host) {
    return base::BindRepeating([] { return std::string(); });
  }
  auto* platform_window = host->platform_window();
  if (!platform_window) {
    return base::BindRepeating([] { return std::string(); });
  }
  std::string unique_id = platform_window->GetWindowUniqueId();

  return base::BindRepeating(
      [](const std::string& unique_id) { return unique_id; }, unique_id);
}

// GetWindowForRequestId is not used in Lacros's WebAuthnRequestRegistrar:
// WebAuthnRequestRegistrar is now in charge of two things: assigning ids to
// windows and getting windows by assigned ids. In lacros we only need the first
// because the logic of getting windows by assigned ids is only needed in ash
// where we summon the dialog.
aura::Window* WebAuthnRequestRegistrarLacros::GetWindowForRequestId(
    std::string request_id) {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}
