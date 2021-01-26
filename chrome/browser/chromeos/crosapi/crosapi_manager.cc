// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/crosapi_manager.h"

#include <utility>

#include "chrome/browser/chromeos/crosapi/crosapi_ash.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"

namespace crosapi {
namespace {

CrosapiManager* g_instance = nullptr;

}  // namespace

CrosapiManager* CrosapiManager::Get() {
  DCHECK(g_instance);
  return g_instance;
}

CrosapiManager::CrosapiManager() : crosapi_(std::make_unique<CrosapiAsh>()) {
  DCHECK(!g_instance);
  g_instance = this;
}

CrosapiManager::~CrosapiManager() {
  DCHECK(g_instance == this);
  g_instance = nullptr;
}

void CrosapiManager::BindCrosapi(
    mojo::PendingReceiver<mojom::Crosapi> pending_receiver,
    base::OnceClosure disconnect_handler) {
  crosapi_->BindReceiver(std::move(pending_receiver),
                         std::move(disconnect_handler));
}

}  // namespace crosapi
