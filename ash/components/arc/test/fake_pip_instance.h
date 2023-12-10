// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_PIP_INSTANCE_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_PIP_INSTANCE_H_

#include "ash/components/arc/mojom/pip.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace arc {

class FakePipInstance : public mojom::PipInstance {
 public:
  FakePipInstance();

  FakePipInstance(const FakePipInstance&) = delete;
  FakePipInstance& operator=(const FakePipInstance&) = delete;

  ~FakePipInstance() override;

  int num_closed() { return num_closed_; }
  std::optional<bool> suppressed() const { return suppressed_; }

  // mojom::PipInstance overrides:
  void Init(mojo::PendingRemote<mojom::PipHost> host_remote,
            InitCallback callback) override;
  void ClosePip() override;
  void SetPipSuppressionStatus(bool suppressed) override;

 private:
  mojo::Remote<mojom::PipHost> host_remote_;
  int num_closed_ = 0;
  std::optional<bool> suppressed_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_PIP_INSTANCE_H_
