// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_CLOUD_POLICY_CLIENT_FACTORY_ASH_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_CLOUD_POLICY_CLIENT_FACTORY_ASH_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"

namespace ash::system {
class StatisticsProvider;
}

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace policy {

class DeviceManagementService;

// Creates a CloudPolicyClient specific to the device level policies in Ash.
// Returned client can be used to connect |DeviceCloudPolicyManagerAsh|.
// |statistics_provider| is used to retrieve machine identity (machine id,
// model, brand code etc.). |service|, |url_loader_factory| and
// |device_dm_token_callback| are passed to the client as are.
std::unique_ptr<CloudPolicyClient> CreateDeviceCloudPolicyClientAsh(
    ash::system::StatisticsProvider* statistics_provider,
    DeviceManagementService* service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    CloudPolicyClient::DeviceDMTokenCallback device_dm_token_callback);

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_CLOUD_POLICY_CLIENT_FACTORY_ASH_H_
