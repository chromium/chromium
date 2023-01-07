// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_CUPS_PRINTERS_MANAGER_PROXY_H_
#define CHROME_BROWSER_ASH_PRINTING_CUPS_PRINTERS_MANAGER_PROXY_H_

#include <memory>
#include <vector>

#include "chrome/browser/ash/printing/cups_printers_manager.h"

namespace ash {

// A proxy for observers of CupsPrintersManager who do not have access to a
// profile and always wish to observe the printers for the primary user profile.
class CupsPrintersManagerProxy {
 public:
  static std::unique_ptr<CupsPrintersManagerProxy> Create();

  CupsPrintersManagerProxy(const CupsPrintersManagerProxy&) = delete;
  CupsPrintersManagerProxy& operator=(const CupsPrintersManagerProxy&) = delete;

  virtual ~CupsPrintersManagerProxy() = default;

  virtual void AddObserver(CupsPrintersManager::Observer*) = 0;
  virtual void RemoveObserver(CupsPrintersManager::Observer*) = 0;

  // Set the manager which supplies events.  As a sideeffect, this triggers
  // observer calls to guarantee notification to observers.
  //
  // It is an error to set a manager when an active manager is present.
  virtual void SetManager(CupsPrintersManager* manager) = 0;

  // Resets the active manager if |manager| matches the current active manager.
  virtual void RemoveManager(CupsPrintersManager* manager) = 0;

 protected:
  CupsPrintersManagerProxy() = default;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_CUPS_PRINTERS_MANAGER_PROXY_H_
