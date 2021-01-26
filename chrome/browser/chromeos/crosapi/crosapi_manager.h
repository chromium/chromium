// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSAPI_CROSAPI_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_CROSAPI_CROSAPI_MANAGER_H_

#include <memory>

#include "base/callback.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace crosapi {
namespace mojom {
class Crosapi;
}  // namespace mojom

class CrosapiAsh;

// Maintains the crosapi connection provided by ash-chrome.
class CrosapiManager {
 public:
  // Returns the instance of CrosapiManager. It is effectively a singleton.
  static CrosapiManager* Get();

  CrosapiManager();
  CrosapiManager(const CrosapiManager&) = delete;
  CrosapiManager& operator=(const CrosapiManager&) = delete;
  ~CrosapiManager();

  // Binds the given receiver to the CrosapiAsh implementation.
  // |disconnect_handler| is called, when the connection is lost.
  void BindCrosapi(mojo::PendingReceiver<mojom::Crosapi> pending_receiver,
                   base::OnceClosure disconnect_handler);

  // TODO(crbug.com/1148448): Move invitation sending flow to here.

 private:
  std::unique_ptr<CrosapiAsh> crosapi_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_CHROMEOS_CROSAPI_CROSAPI_MANAGER_H_
