// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_SHARESHEET_INSTANCE_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_SHARESHEET_INSTANCE_H_

#include "ash/components/arc/mojom/sharesheet.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace arc {

class FakeSharesheetInstance : public mojom::SharesheetInstance {
 public:
  FakeSharesheetInstance();
  FakeSharesheetInstance(const FakeSharesheetInstance&) = delete;
  FakeSharesheetInstance& operator=(const FakeSharesheetInstance&) = delete;
  ~FakeSharesheetInstance() override;

  // mojom::SharesheetInstance overrides:
  void Init(mojo::PendingRemote<mojom::SharesheetHost> host_remote,
            InitCallback callback) override;

  size_t num_init_called() const { return num_init_called_; }

 private:
  mojo::Remote<mojom::SharesheetHost> host_remote_;
  size_t num_init_called_ = 0;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_SHARESHEET_INSTANCE_H_
