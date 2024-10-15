// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCANNER_SCANNER_KEYED_SERVICE_H_
#define CHROME_BROWSER_ASH_SCANNER_SCANNER_KEYED_SERVICE_H_

#include <memory>

#include "ash/public/cpp/scanner/scanner_action.h"
#include "ash/public/cpp/scanner/scanner_profile_scoped_delegate.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ash/scanner/scanner_action_provider.h"
#include "chrome/browser/ash/scanner/scanner_system_state_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"

namespace ash {
struct ScannerSystemState;
}  // namespace ash

namespace drive {
class DriveAPIService;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace signin {
class IdentityManager;
}

// A ProfileKeyedService for the Scanner feature. This is a top level class
// that is scoped to a particular profile, and provides access to that profile
// instance to all sub classes that require a valid profile instance to
// function.
class ScannerKeyedService : public ash::ScannerProfileScopedDelegate,
                            public KeyedService {
 public:
  explicit ScannerKeyedService(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ScannerKeyedService(const ScannerKeyedService&) = delete;
  ScannerKeyedService& operator=(const ScannerKeyedService&) = delete;
  ~ScannerKeyedService() override;

  // ash::ScannerProfileScopedDelegate:
  ash::ScannerSystemState GetSystemState() const override;
  void FetchActionsForImage(
      scoped_refptr<base::RefCountedMemory> jpeg_bytes,
      base::OnceCallback<void(ash::ScannerActionsResponse)> callback) override;
  drive::DriveServiceInterface* GetDriveService() override;

  // KeyedService:
  void Shutdown() override;

 private:
  ScannerActionProvider action_provider_;
  ScannerSystemStateProvider system_state_provider_;

  std::unique_ptr<drive::DriveAPIService> drive_service_;
};

#endif  // CHROME_BROWSER_ASH_SCANNER_SCANNER_KEYED_SERVICE_H_
