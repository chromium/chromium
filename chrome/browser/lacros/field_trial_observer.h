// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_FIELD_TRIAL_OBSERVER_H_
#define CHROME_BROWSER_LACROS_FIELD_TRIAL_OBSERVER_H_

#include "chromeos/crosapi/mojom/field_trial.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

// This class connects to ash-chrome, listens to field trial changes
// and register synthetic field trials accordingly.
class COMPONENT_EXPORT(CHROMEOS_LACROS) FieldTrialObserver
    : public crosapi::mojom::FieldTrialObserver {
 public:
  FieldTrialObserver();

  FieldTrialObserver(const FieldTrialObserver&) = delete;
  FieldTrialObserver& operator=(const FieldTrialObserver&) = delete;
  ~FieldTrialObserver() override;

  // Start observing field trial info changes in ash-chrome.
  void Start();

 protected:
  // crosapi::mojom::FieldTrialObserver:
  void OnFieldTrialGroupActivated(
      std::vector<crosapi::mojom::FieldTrialGroupInfoPtr> info) override;

 private:
  // Receives mojo messages from ash-chrome (under Streaming mode).
  mojo::Receiver<crosapi::mojom::FieldTrialObserver> receiver_{this};
};

#endif  // CHROME_BROWSER_LACROS_FIELD_TRIAL_OBSERVER_H_
