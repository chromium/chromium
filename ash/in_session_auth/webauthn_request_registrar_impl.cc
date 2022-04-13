// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/webauthn_request_registrar_impl.h"

#include <string>

#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"

namespace ash {

namespace {

constexpr uint32_t kInvalidRequestId = 0u;
uint32_t g_current_request_id = kInvalidRequestId;

// A property key to tie the WebAuthn request id to a window.
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::string, kWebAuthnRequestId, nullptr)

}  // namespace

WebAuthnRequestRegistrarImpl::WebAuthnRequestRegistrarImpl() = default;

WebAuthnRequestRegistrarImpl::~WebAuthnRequestRegistrarImpl() = default;

WebAuthnRequestRegistrarImpl::GenerateRequestIdCallback
WebAuthnRequestRegistrarImpl::GetRegisterCallback(aura::Window* window) {
  // If the window is nullptr, e.g. IsUVPAA() is called shortly before or after
  // navigation, the render_frame_host may not be initialized properly, we
  // return a dumb callback. In the worst case, if it's MakeCredential or
  // GetAssertion, the Chrome OS auth dialog will not show up and the operation
  // fails gracefully.
  if (!window) {
    return base::BindRepeating([] { return std::string(); });
  }

  window_tracker_.Add(window);
  // base::Unretained() is safe here because WebAuthnRequestRegistrarImpl
  // has the same lifetime as Shell and is released at
  // PostMainMessageLoopRun stage. The callback should not be invoked
  // after main message loop tearing down.
  return base::BindRepeating(&WebAuthnRequestRegistrarImpl::DoRegister,
                             base::Unretained(this), window);
}

std::string WebAuthnRequestRegistrarImpl::DoRegister(aura::Window* window) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Avoid uint32_t overflow.
  do {
    g_current_request_id++;
  } while (g_current_request_id == kInvalidRequestId);

  std::string request_id = base::NumberToString(g_current_request_id);

  // If |window| is still valid, associate it with the new request id.
  // If |window| is gone, the incremented id will fail the request later,
  // which is ok.
  if (window_tracker_.Contains(window)) {
    window->SetProperty(kWebAuthnRequestId, new std::string(request_id));
  }
  return request_id;
}

aura::Window* WebAuthnRequestRegistrarImpl::GetWindowForRequestId(
    std::string request_id) {
  if (request_id.empty()) {
    return nullptr;
  }

  MruWindowTracker::WindowList windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kAllDesks);
  for (aura::Window* window : windows) {
    std::string* window_request_id = window->GetProperty(kWebAuthnRequestId);
    if (window_request_id && *window_request_id == request_id) {
      return window;
    }
  }
  return nullptr;
}

}  // namespace ash
