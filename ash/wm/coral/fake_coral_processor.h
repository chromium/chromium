// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_CORAL_FAKE_CORAL_PROCESSOR_H_
#define ASH_WM_CORAL_FAKE_CORAL_PROCESSOR_H_

#include "chromeos/ash/services/coral/public/mojom/coral_service.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"

namespace ash {

// A fake coral processor to generate coral groups for test use.
class FakeCoralProcessor : public coral::mojom::CoralProcessor {
 public:
  FakeCoralProcessor() = default;
  FakeCoralProcessor(const FakeCoralProcessor&) = delete;
  FakeCoralProcessor& operator=(const FakeCoralProcessor&) = delete;
  ~FakeCoralProcessor() override = default;

  // coral::mojom::CoralProcessor:
  void Group(coral::mojom::GroupRequestPtr request,
             mojo::PendingRemote<coral::mojom::TitleObserver> observer,
             GroupCallback callback) override;
  void CacheEmbeddings(coral::mojom::CacheEmbeddingsRequestPtr request,
                       CacheEmbeddingsCallback callback) override;
};

}  // namespace ash

#endif  // ASH_WM_CORAL_FAKE_CORAL_PROCESSOR_H_
