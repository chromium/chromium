// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/test_support/win/fake_wlan_connection_profile_details.h"

#include "base/win/scoped_hstring.h"
#include "chrome/browser/media/router/discovery/test_support/win/fake_winrt_network_environment.h"

namespace media_router {

FakeWlanConnectionProfileDetails::FakeWlanConnectionProfileDetails(
    base::WeakPtr<FakeWinrtNetworkEnvironment> fake_network_environment,
    const std::string& ssid)
    : fake_network_environment_(fake_network_environment), ssid_(ssid) {}

HRESULT FakeWlanConnectionProfileDetails::GetConnectedSsid(HSTRING* value) {
  if (fake_network_environment_->GetErrorStatus() ==
      FakeWinrtNetworkStatus::
          kErrorWlanConnectionProfileDetailsGetConnectedSsidFailed) {
    return fake_network_environment_->MakeHresult(
        FakeWinrtNetworkStatus::
            kErrorWlanConnectionProfileDetailsGetConnectedSsidFailed);
  }

  base::win::ScopedHString ssid_copy(base::win::ScopedHString::Create(ssid_));
  *value = ssid_copy.release();
  return S_OK;
}

FakeWlanConnectionProfileDetails::~FakeWlanConnectionProfileDetails() = default;

}  // namespace media_router
