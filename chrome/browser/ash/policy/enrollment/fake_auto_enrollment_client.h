// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_FAKE_AUTO_ENROLLMENT_CLIENT_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_FAKE_AUTO_ENROLLMENT_CLIENT_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_client.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_state.h"

class PrefService;

namespace policy::psm {
class RlweDmserverClient;
}

namespace ash {
class OobeConfiguration;
}  // namespace ash

namespace policy {

class DeviceManagementService;

// A fake AutoEnrollmentClient. The test code can control its state.
class FakeAutoEnrollmentClient : public AutoEnrollmentClient {
 public:
  // A factory that creates |FakeAutoEnrollmentClient|s.
  class FactoryImpl : public Factory {
   public:
    // The factory will notify |fake_client_created_callback| when a
    // |AutoEnrollmentClient| has been created through one of its methods.
    explicit FactoryImpl(
        const base::RepeatingCallback<void(FakeAutoEnrollmentClient*)>&
            fake_client_created_callback);

    FactoryImpl(const FactoryImpl&) = delete;
    FactoryImpl& operator=(const FactoryImpl&) = delete;

    ~FactoryImpl() override;

    std::unique_ptr<AutoEnrollmentClient> CreateForFRE(
        const ProgressCallback& progress_callback,
        DeviceManagementService* device_management_service,
        PrefService* local_state,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        const std::string& server_backed_state_key,
        int power_initial,
        int power_limit) override;

    std::unique_ptr<AutoEnrollmentClient> CreateForInitialEnrollment(
        const ProgressCallback& progress_callback,
        DeviceManagementService* device_management_service,
        PrefService* local_state,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        const std::string& device_serial_number,
        const std::string& device_brand_code,
        std::unique_ptr<psm::RlweDmserverClient> psm_rlwe_dmserver_client,
        ash::OobeConfiguration* oobe_config) override;

   private:
    base::RepeatingCallback<void(FakeAutoEnrollmentClient*)>
        fake_client_created_callback_;
  };

  explicit FakeAutoEnrollmentClient(const ProgressCallback& progress_callback);

  FakeAutoEnrollmentClient(const FakeAutoEnrollmentClient&) = delete;
  FakeAutoEnrollmentClient& operator=(const FakeAutoEnrollmentClient&) = delete;

  ~FakeAutoEnrollmentClient() override;

  // The methods do not fire state change until `SetState` is called.
  void Start() override;
  void Retry() override;

  // Sets the state and notifies the |ProgressCallback| passed to the
  // constructor.
  void SetState(AutoEnrollmentState target_state);

 private:
  ProgressCallback progress_callback_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_FAKE_AUTO_ENROLLMENT_CLIENT_H_
