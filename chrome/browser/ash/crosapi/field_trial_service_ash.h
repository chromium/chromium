// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_FIELD_TRIAL_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_FIELD_TRIAL_SERVICE_ASH_H_

#include "base/metrics/field_trial.h"
#include "chromeos/crosapi/mojom/field_trial.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace crosapi {

// The ash-chrome implementation of the FieldTrial crosapi interface.
class FieldTrialServiceAsh : public mojom::FieldTrialService,
                             base::FieldTrialList::Observer {
 public:
  FieldTrialServiceAsh();
  FieldTrialServiceAsh(const FieldTrialServiceAsh&) = delete;
  FieldTrialServiceAsh& operator=(const FieldTrialServiceAsh&) = delete;
  ~FieldTrialServiceAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::FieldTrialService> receiver);

  // crosapi::mojom::FieldTrialService:
  void AddFieldTrialObserver(
      mojo::PendingRemote<mojom::FieldTrialObserver> observer) override;

  // FieldTrialList::Observer:
  void OnFieldTrialGroupFinalized(const base::FieldTrial& trial,
                                  const std::string& group_name) override;

 private:
  void FieldTrialActivated(const std::string& trial_name,
                           const std::string& group_name,
                           bool is_overridden);

  // Support any number of connections.
  mojo::ReceiverSet<mojom::FieldTrialService> receivers_;

  // Support any number of observers.
  mojo::RemoteSet<mojom::FieldTrialObserver> observers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_FIELD_TRIAL_SERVICE_ASH_H_
