// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_EVENT_PROXY_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_EVENT_PROXY_H_

#include <string>

#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "ui/gfx/range/range.h"

namespace ash::input_method {

class EditorEventProxy {
 public:
  explicit EditorEventProxy(
      mojo::PendingAssociatedRemote<orca::mojom::EditorEventSink> remote);
  ~EditorEventProxy();

  void OnSurroundingTextChanged(const std::u16string& text, gfx::Range range);

 private:
  mojo::AssociatedRemote<orca::mojom::EditorEventSink>
      editor_event_sink_remote_;
};
}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_EVENT_PROXY_H_
