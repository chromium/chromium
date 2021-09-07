// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_BROWSER_VERSION_OBSERVER_H_
#define CHROME_BROWSER_LACROS_BROWSER_VERSION_OBSERVER_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/browser_version.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace crosapi {

// Interface to observe Browser version changes from Crosapi clients.
class COMPONENT_EXPORT(CHROMEOS_LACROS) BrowserVersionHostObserver
    : public crosapi::mojom::BrowserVersionObserver {
 public:
  BrowserVersionObserver();
  ~BrowserVersionObserver() override;

 private:
  // crosapi::mojom::BrowserVersionObserver:
  void OnBrowserVersionInstalled(std::string version) override;

  // Receives mojo messages from ash-chrome (under Streaming mode).
  mojo::Receiver<crosapi::mojom::BrowserVersionObserver> receiver_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_LACROS_BROWSER_VERSION_OBSERVER_H_
