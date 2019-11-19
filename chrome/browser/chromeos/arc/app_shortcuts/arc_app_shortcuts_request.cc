// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/app_shortcuts/arc_app_shortcuts_request.h"

#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/arc/icon_decode_request.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_bridge_service.h"

namespace arc {

ArcAppShortcutsRequest::ArcAppShortcutsRequest(
    GetAppShortcutItemsCallback callback)
    : callback_(std::move(callback)) {
  DCHECK(callback_);
}

ArcAppShortcutsRequest::~ArcAppShortcutsRequest() = default;

void ArcAppShortcutsRequest::StartForPackage(const std::string& package_name) {
  // DCHECK because it shouldn't be called more than one time for the life cycle
  // of |this|.
  DCHECK(!items_);
  DCHECK(icon_decode_requests_.empty());

  mojom::AppInstance* app_instance =
      ArcServiceManager::Get()
          ? ARC_GET_INSTANCE_FOR_METHOD(
                ArcServiceManager::Get()->arc_bridge_service()->app(),
                GetAppShortcutItems)
          : nullptr;

  if (!app_instance) {
    std::move(callback_).Run(nullptr);
    return;
  }

  app_instance->GetAppShortcutItems(
      package_name,
      base::BindOnce(&ArcAppShortcutsRequest::OnGetAppShortcutItems,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcAppShortcutsRequest::OnGetAppShortcutItems(
    std::vector<mojom::AppShortcutItemPtr> shortcut_items) {
  // In case |shortcut_items| is empty report now, otherwise
  // |OnAllIconDecodeRequestsDone| will be called immediately on creation. As
  // result of handling this the request will be deleted and assigning
  // |barrier_closure_| will happen on deleted object that will cause memory
  // corruption.
  if (shortcut_items.empty()) {
    std::move(callback_).Run(nullptr);
    return;
  }

  items_ = std::make_unique<ArcAppShortcutItems>();

  // Using base::Unretained(this) here is safe since we own barrier_closure_.
  barrier_closure_ = base::BarrierClosure(
      shortcut_items.size(),
      base::BindOnce(&ArcAppShortcutsRequest::OnAllIconDecodeRequestsDone,
                     base::Unretained(this)));

  constexpr int kAppShortcutIconSize = 32;
  for (const auto& shortcut_item_ptr : shortcut_items) {
    ArcAppShortcutItem item;
    item.shortcut_id = shortcut_item_ptr->shortcut_id;
    item.short_label = base::UTF8ToUTF16(shortcut_item_ptr->short_label);
    item.type = shortcut_item_ptr->type;
    item.rank = shortcut_item_ptr->rank;
    items_->emplace_back(std::move(item));

    icon_decode_requests_.emplace_back(std::make_unique<IconDecodeRequest>(
        base::BindOnce(&ArcAppShortcutsRequest::OnSingleIconDecodeRequestDone,
                       weak_ptr_factory_.GetWeakPtr(), items_->size() - 1),
        kAppShortcutIconSize));
    icon_decode_requests_.back()->StartWithOptions(shortcut_item_ptr->icon_png);
  }
}

void ArcAppShortcutsRequest::OnAllIconDecodeRequestsDone() {
  icon_decode_requests_.clear();
  DCHECK(callback_);
  std::move(callback_).Run(std::move(items_));
}

void ArcAppShortcutsRequest::OnSingleIconDecodeRequestDone(
    size_t index,
    const gfx::ImageSkia& icon) {
  DCHECK(items_);
  DCHECK_LT(index, items_->size());
  items_->at(index).icon = icon;
  barrier_closure_.Run();
}

}  // namespace arc
