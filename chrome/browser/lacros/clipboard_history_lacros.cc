// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/clipboard_history_lacros.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "chromeos/lacros/lacros_service.h"

namespace crosapi {

namespace {
ClipboardHistoryLacros* g_instance = nullptr;
}  // namespace

ClipboardHistoryLacros::ClipboardHistoryLacros() : receiver_(this) {
  CHECK(chromeos::features::IsClipboardHistoryRefreshEnabled());
  CHECK(!g_instance);
  g_instance = this;

  // Register on the Ash side to receive descriptor updates.
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (service->IsAvailable<mojom::ClipboardHistory>() &&
      service->GetInterfaceVersion<mojom::ClipboardHistory>() >=
          int{crosapi::mojom::ClipboardHistory::MethodMinVersions::
                  kRegisterClientMinVersion}) {
    service->GetRemote<mojom::ClipboardHistory>()->RegisterClient(
        receiver_.BindNewPipeAndPassRemote());

    // `receiver_` is a class member so it is safe to use `this` pointer here.
    receiver_.set_disconnect_handler(base::BindOnce(
        &ClipboardHistoryLacros::OnDisconnected, base::Unretained(this)));
  }
}

ClipboardHistoryLacros::~ClipboardHistoryLacros() {
  CHECK(g_instance);
  g_instance = nullptr;
}

// static
ClipboardHistoryLacros* ClipboardHistoryLacros::Get() {
  CHECK(g_instance);
  return g_instance;
}

void ClipboardHistoryLacros::SetClipboardHistoryItemDescriptors(
    std::vector<mojom::ClipboardHistoryItemDescriptorPtr> descriptor_ptrs) {
  std::vector<mojom::ClipboardHistoryItemDescriptor> filtered_items;
  for (const auto& descriptor_ptr : descriptor_ptrs) {
    // Ignore the received descriptors of unknown types.
    if (descriptor_ptr->display_format ==
        mojom::ClipboardHistoryDisplayFormat::kUnknown) {
      continue;
    }
    filtered_items.emplace_back(
        descriptor_ptr->item_id, descriptor_ptr->display_format,
        descriptor_ptr->display_text, descriptor_ptr->file_count);
  }
  cached_descriptors_ = std::move(filtered_items);
}

void ClipboardHistoryLacros::OnDisconnected() {
  receiver_.reset();
}

}  // namespace crosapi
