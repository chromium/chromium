// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_CORAL_FAKE_CORAL_SERVICE_H_
#define ASH_WM_CORAL_FAKE_CORAL_SERVICE_H_

#include "chromeos/ash/services/coral/public/mojom/coral_service.mojom.h"

namespace ash {

// A fake coral service backend to generate coral groups for test use.
class FakeCoralService : public coral::mojom::CoralService {
 public:
  FakeCoralService() = default;
  FakeCoralService(const FakeCoralService&) = delete;
  FakeCoralService& operator=(const FakeCoralService&) = delete;
  ~FakeCoralService() override = default;

  // coral::mojom::CoralService:
  void Group(coral::mojom::GroupRequestPtr request,
             mojo::PendingRemote<coral::mojom::TitleObserver> observer,
             GroupCallback callback) override;
  void CacheEmbeddings(coral::mojom::CacheEmbeddingsRequestPtr request,
                       CacheEmbeddingsCallback callback) override;
  void PrepareResource() override;
};

}  // namespace ash

#endif  // ASH_WM_CORAL_FAKE_CORAL_SERVICE_H_
