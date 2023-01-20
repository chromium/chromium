// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_FAKE_SYNC_GCM_DRIVER_FOR_INSTANCE_ID_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_FAKE_SYNC_GCM_DRIVER_FOR_INSTANCE_ID_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "components/gcm_driver/instance_id/fake_gcm_driver_for_instance_id.h"

class Profile;

namespace base {
class SequencedTaskRunner;
}  // namespace base

class FakeSyncGCMDriver : public instance_id::FakeGCMDriverForInstanceID {
 public:
  FakeSyncGCMDriver(
      Profile* profile,
      const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner);

 protected:
  // FakeGCMDriverForInstanceID overrides:
  void EncryptMessage(const std::string& app_id,
                      const std::string& authorized_entity,
                      const std::string& p256dh,
                      const std::string& auth_secret,
                      const std::string& message,
                      EncryptMessageCallback callback) override;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_FAKE_SYNC_GCM_DRIVER_FOR_INSTANCE_ID_H_
