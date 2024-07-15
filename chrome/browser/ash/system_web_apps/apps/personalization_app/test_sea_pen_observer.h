// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_TEST_SEA_PEN_OBSERVER_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_TEST_SEA_PEN_OBSERVER_H_

#include <optional>

#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "base/functional/callback.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::personalization_app {

class TestSeaPenObserver : public personalization_app::mojom::SeaPenObserver {
 public:
  TestSeaPenObserver();

  TestSeaPenObserver(const TestSeaPenObserver&) = delete;
  TestSeaPenObserver& operator=(const TestSeaPenObserver&) = delete;

  ~TestSeaPenObserver() override;

  void SetCallback(base::OnceCallback<void(std::optional<uint32_t>)> callback);

  mojo::PendingRemote<personalization_app::mojom::SeaPenObserver>
  GetPendingRemote();

  std::optional<uint32_t> GetCurrentId();

  std::optional<std::vector<mojom::TextQueryHistoryEntryPtr>>
  GetHistoryEntries();

  // ash::personalization_app::mojom::SeaPenObserver:
  void OnSelectedSeaPenImageChanged(std::optional<uint32_t> id) override;

  void OnTextQueryHistoryChanged(
      std::optional<std::vector<mojom::TextQueryHistoryEntryPtr>> entries)
      override;

  unsigned int id_updated_count() const { return id_updated_count_; }

 private:
  std::optional<std::vector<mojom::TextQueryHistoryEntryPtr>> entries_ =
      std::nullopt;
  unsigned int id_updated_count_ = 0;
  std::optional<uint32_t> id_ = std::nullopt;
  base::OnceCallback<void(std::optional<uint32_t>)> callback_;
  mojo::Receiver<ash::personalization_app::mojom::SeaPenObserver>
      sea_pen_observer_receiver_{this};
};

}  // namespace ash::personalization_app

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_TEST_SEA_PEN_OBSERVER_H_
