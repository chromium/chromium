// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_CAST_RECEIVER_INSTANCE_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_CAST_RECEIVER_INSTANCE_H_

#include <string>

#include "ash/components/arc/mojom/cast_receiver.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace arc {

class FakeCastReceiverInstance : public mojom::CastReceiverInstance {
 public:
  FakeCastReceiverInstance();
  FakeCastReceiverInstance(const FakeCastReceiverInstance&) = delete;
  FakeCastReceiverInstance& operator=(const FakeCastReceiverInstance&) = delete;
  ~FakeCastReceiverInstance() override;

  // mojom::CastReceiverInstance overrides:
  using GetNameCallback =
      base::OnceCallback<void(mojom::CastReceiverInstance::Result,
                              const std::string&)>;
  void GetName(GetNameCallback callback) override;

  using SetEnabledCallback =
      base::OnceCallback<void(mojom::CastReceiverInstance::Result)>;
  void SetEnabled(bool enabled, SetEnabledCallback callback) override;

  using SetNameCallback =
      base::OnceCallback<void(mojom::CastReceiverInstance::Result)>;
  void SetName(const std::string& name, SetNameCallback callback) override;

  const absl::optional<bool>& last_enabled() const { return last_enabled_; }
  const absl::optional<std::string>& last_name() const { return last_name_; }

 private:
  absl::optional<bool> last_enabled_;
  absl::optional<std::string> last_name_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_CAST_RECEIVER_INSTANCE_H_
