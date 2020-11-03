// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/webauthn_request_registrar_impl.h"

#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"

namespace ash {

namespace {

// A property key to tie the WebAuthn request id to a window.
DEFINE_UI_CLASS_PROPERTY_KEY(uint32_t, kWebAuthnRequestId, 0u)

uint32_t g_current_request_id = 0u;

}  // namespace

WebAuthnRequestRegistrarImpl::WebAuthnRequestRegistrarImpl() = default;

WebAuthnRequestRegistrarImpl::~WebAuthnRequestRegistrarImpl() = default;

WebAuthnRequestRegistrarImpl::GenerateRequestIdCallback
WebAuthnRequestRegistrarImpl::GetRegisterCallback(aura::Window* window) {
  window_tracker_.Add(window);
  // base::Unretained() is safe here because WebAuthnRequestRegistrarImpl
  // has the same lifetime as Shell and is released at
  // PostMainMessageLoopRun stage. The callback should not be invoked
  // after main message loop tearing down.
  return base::BindRepeating(&WebAuthnRequestRegistrarImpl::DoRegister,
                             base::Unretained(this), window);
}

uint32_t WebAuthnRequestRegistrarImpl::DoRegister(aura::Window* window) {
  g_current_request_id++;
  // If |window| is still valid, associate it with the new request id.
  // If |window| is gone, the incremented id will fail the request later,
  // which is ok.
  if (window_tracker_.Contains(window))
    window->SetProperty(kWebAuthnRequestId, g_current_request_id);
  return g_current_request_id;
}

aura::Window* WebAuthnRequestRegistrarImpl::GetWindowForRequestId(
    uint32_t request_id) {
  MruWindowTracker::WindowList windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kAllDesks);
  for (aura::Window* window : windows) {
    uint32_t window_request_id = window->GetProperty(kWebAuthnRequestId);
    if (window_request_id == request_id) {
      return window;
    }
  }
  return nullptr;
}

}  // namespace ash
