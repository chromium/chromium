// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_CLIENT_CONNECTOR_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_CLIENT_CONNECTOR_H_

#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "ui/gfx/range/range.h"

namespace ash::input_method {

class EditorClientConnector {
 public:
  explicit EditorClientConnector(
      mojo::PendingAssociatedRemote<orca::mojom::EditorClientConnector> remote);
  ~EditorClientConnector();

  void BindEditorClient(
      mojo::PendingReceiver<orca::mojom::EditorClient> editor_client_receiver);

 private:
  mojo::AssociatedRemote<orca::mojom::EditorClientConnector>
      editor_client_connector_remote_;
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_CLIENT_CONNECTOR_H_
