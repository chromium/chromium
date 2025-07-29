// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_event_proxy.h"

#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"

namespace ash::input_method {

EditorEventProxy::EditorEventProxy(
    mojo::PendingAssociatedRemote<orca::mojom::EditorEventSink> remote)
    : editor_event_sink_remote_(std::move(remote)) {}

EditorEventProxy::~EditorEventProxy() = default;

void EditorEventProxy::OnSurroundingTextChanged(
    orca::mojom::ContextPtr context) {
  editor_event_sink_remote_->OnContextUpdated(std::move(context));
}

}  // namespace ash::input_method
