// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_MULTIDEVICE_SETUP_FAKE_FEATURE_STATE_OBSERVER_H_
#define ASH_SERVICES_MULTIDEVICE_SETUP_FAKE_FEATURE_STATE_OBSERVER_H_

#include "ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "base/containers/flat_map.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {

namespace multidevice_setup {

// Fake mojom::FeatureStateObserver implementation for tests.
class FakeFeatureStateObserver
    : public ash::multidevice_setup::mojom::FeatureStateObserver {
 public:
  FakeFeatureStateObserver();

  FakeFeatureStateObserver(const FakeFeatureStateObserver&) = delete;
  FakeFeatureStateObserver& operator=(const FakeFeatureStateObserver&) = delete;

  ~FakeFeatureStateObserver() override;

  mojo::PendingRemote<ash::multidevice_setup::mojom::FeatureStateObserver>
  GenerateRemote();

  const std::vector<
      base::flat_map<ash::multidevice_setup::mojom::Feature,
                     ash::multidevice_setup::mojom::FeatureState>>&
  feature_state_updates() {
    return feature_state_updates_;
  }

 private:
  // mojom::FeatureStateObserver:
  void OnFeatureStatesChanged(
      const base::flat_map<ash::multidevice_setup::mojom::Feature,
                           ash::multidevice_setup::mojom::FeatureState>&
          feature_states_map) override;

  std::vector<base::flat_map<ash::multidevice_setup::mojom::Feature,
                             ash::multidevice_setup::mojom::FeatureState>>
      feature_state_updates_;

  mojo::ReceiverSet<ash::multidevice_setup::mojom::FeatureStateObserver>
      receivers_;
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // ASH_SERVICES_MULTIDEVICE_SETUP_FAKE_FEATURE_STATE_OBSERVER_H_
