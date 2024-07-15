// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/test_sea_pen_observer.h"

#include <optional>

#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "base/functional/callback.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::personalization_app {

TestSeaPenObserver::TestSeaPenObserver() = default;

TestSeaPenObserver::~TestSeaPenObserver() = default;

void TestSeaPenObserver::SetCallback(
    base::OnceCallback<void(std::optional<uint32_t>)> callback) {
  callback_ = std::move(callback);
}

mojo::PendingRemote<personalization_app::mojom::SeaPenObserver>
TestSeaPenObserver::GetPendingRemote() {
  if (sea_pen_observer_receiver_.is_bound()) {
    sea_pen_observer_receiver_.reset();
  }
  return sea_pen_observer_receiver_.BindNewPipeAndPassRemote();
}

std::optional<uint32_t> TestSeaPenObserver::GetCurrentId() {
  if (sea_pen_observer_receiver_.is_bound()) {
    sea_pen_observer_receiver_.FlushForTesting();
  }
  return id_;
}

std::optional<std::vector<mojom::TextQueryHistoryEntryPtr>>
TestSeaPenObserver::GetHistoryEntries() {
  if (sea_pen_observer_receiver_.is_bound()) {
    sea_pen_observer_receiver_.FlushForTesting();
  }
  return std::move(entries_);
}

void TestSeaPenObserver::OnSelectedSeaPenImageChanged(
    std::optional<uint32_t> id) {
  id_updated_count_++;
  id_ = id;
  if (callback_) {
    std::move(callback_).Run(id);
  }
}

void TestSeaPenObserver::OnTextQueryHistoryChanged(
    std::optional<std::vector<mojom::TextQueryHistoryEntryPtr>> entries) {
  entries_ = std::move(entries);
}

}  // namespace ash::personalization_app
