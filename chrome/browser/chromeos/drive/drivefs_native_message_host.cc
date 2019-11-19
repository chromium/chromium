// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drivefs_native_message_host.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/drive/file_errors.h"

namespace drive {

const char kDriveFsNativeMessageHostName[] = "com.google.drive.nativeproxy";

const char* const kDriveFsNativeMessageHostOrigins[] = {
    "chrome-extension://lmjegmlicamnimmfhcmpkclmigmmcbeh/",
};

constexpr size_t kDriveFsNativeMessageHostOriginsSize =
    base::size(kDriveFsNativeMessageHostOrigins);

class DriveFsNativeMessageHost : public extensions::NativeMessageHost {
 public:
  explicit DriveFsNativeMessageHost(Profile* profile)
      : drive_service_(DriveIntegrationServiceFactory::GetForProfile(profile)) {
  }
  ~DriveFsNativeMessageHost() override = default;

  void OnMessage(const std::string& message) override {
    if (!drive_service_ || !drive_service_->GetDriveFsInterface()) {
      OnDriveFsResponse(FILE_ERROR_SERVICE_UNAVAILABLE, "");
      return;
    }

    drive_service_->GetDriveFsInterface()->SendNativeMessageRequest(
        message, base::Bind(&DriveFsNativeMessageHost::OnDriveFsResponse,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  void Start(Client* client) override { client_ = client; }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const override {
    return task_runner_;
  }

 private:
  void OnDriveFsResponse(FileError error, const std::string& response) {
    if (error == FILE_ERROR_OK) {
      client_->PostMessageFromNativeHost(response);
    } else {
      LOG(WARNING) << "DriveFS returned error " << FileErrorToString(error);
      client_->CloseChannel(FileErrorToString(error));
    }
  }

  DriveIntegrationService* drive_service_;

  Client* client_ = nullptr;

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_ =
      base::CreateSingleThreadTaskRunner({base::CurrentThread()});

  base::WeakPtrFactory<DriveFsNativeMessageHost> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DriveFsNativeMessageHost);
};

std::unique_ptr<extensions::NativeMessageHost> CreateDriveFsNativeMessageHost(
    content::BrowserContext* browser_context) {
  return std::make_unique<DriveFsNativeMessageHost>(
      Profile::FromBrowserContext(browser_context));
}

}  // namespace drive
