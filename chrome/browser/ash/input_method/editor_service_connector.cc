// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_service_connector.h"

#include "content/public/browser/service_process_host.h"

namespace ash::input_method {

EditorServiceConnector::EditorServiceConnector() {}

EditorServiceConnector::~EditorServiceConnector() = default;

bool EditorServiceConnector::SetUpNewEditorService() {
  if (IsBound()) {
    return false;
  }

  content::ServiceProcessHost::Launch(
      remote_orca_service_connector_.BindNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          // replace with IDS strings
          .WithDisplayName("EditorService")
          .Pass());

  remote_orca_service_connector_.reset_on_disconnect();

  return true;
}

void EditorServiceConnector::BindEditor(
    mojo::PendingAssociatedReceiver<orca::mojom::EditorClientConnector>
        editor_client_connector,
    mojo::PendingAssociatedReceiver<orca::mojom::EditorEventSink>
        editor_event_sink,
    mojo::PendingAssociatedRemote<orca::mojom::TextActuator> text_actuator,
    mojo::PendingAssociatedRemote<orca::mojom::TextQueryProvider>
        text_query_provider) {
  remote_orca_service_connector_->BindEditor(
      std::move(text_actuator), std::move(text_query_provider),
      std::move(editor_client_connector), std::move(editor_event_sink));
}

bool EditorServiceConnector::IsBound() {
  return remote_orca_service_connector_ &&
         remote_orca_service_connector_.is_bound();
}

}  // namespace ash::input_method
