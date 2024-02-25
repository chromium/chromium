// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_client_connector.h"

#include "base/strings/utf_string_conversions.h"

namespace ash::input_method {

EditorClientConnector::EditorClientConnector(
    mojo::PendingAssociatedRemote<orca::mojom::EditorClientConnector> remote)
    : editor_client_connector_remote_(std::move(remote)) {}

EditorClientConnector::~EditorClientConnector() = default;

void EditorClientConnector::BindEditorClient(
    mojo::PendingReceiver<orca::mojom::EditorClient> editor_client) {
  editor_client_connector_remote_->BindEditorClient(std::move(editor_client));
}

}  // namespace ash::input_method
