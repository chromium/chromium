// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_service_connector.h"

#include <vector>

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "chrome/browser/ash/input_method/editor_config_factory.h"
#include "chrome/browser/ash/input_method/editor_context.h"
#include "chrome/browser/ash/input_method/editor_helpers.h"
#include "chrome/browser/ash/input_method/input_methods_by_language.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"
#include "content/public/browser/service_process_host.h"

namespace ash::input_method {

EditorServiceConnector::EditorServiceConnector(EditorContext* context)
    : context_(context) {
  content::ServiceProcessHost::Launch(
      remote_orca_service_connector_.BindNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          // replace with IDS strings
          .WithDisplayName("EditorService")
          .Pass());
  remote_orca_service_connector_.reset_on_disconnect();
}

EditorServiceConnector::~EditorServiceConnector() = default;

void EditorServiceConnector::BindEditor(
    mojo::PendingAssociatedReceiver<orca::mojom::EditorClientConnector>
        editor_client_connector,
    mojo::PendingAssociatedReceiver<orca::mojom::EditorEventSink>
        editor_event_sink,
    mojo::PendingAssociatedRemote<orca::mojom::SystemActuator> system_actuator,
    mojo::PendingAssociatedRemote<orca::mojom::TextQueryProvider>
        text_query_provider) {
  remote_orca_service_connector_->BindEditor(
      std::move(system_actuator), std::move(text_query_provider),
      std::move(editor_client_connector), std::move(editor_event_sink),
      BuildConfigFor(
          InputMethodToLanguageCategory(context_->active_engine_id())));
}

bool EditorServiceConnector::IsBound() {
  return remote_orca_service_connector_ &&
         remote_orca_service_connector_.is_bound();
}

}  // namespace ash::input_method
