// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/test_support/win/fake_network_information_statics.h"

#include "base/notreached.h"
#include "chrome/browser/media/router/discovery/test_support/win/fake_vector_view.h"
#include "chrome/browser/media/router/discovery/test_support/win/fake_winrt_network_environment.h"

namespace WinrtConnectivity = ABI::Windows::Networking::Connectivity;
namespace WinrtCollections = ABI::Windows::Foundation::Collections;
namespace WinrtNetworking = ABI::Windows::Networking;
namespace WinrtFoundation = ABI::Windows::Foundation;

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

namespace media_router {

FakeNetworkInformationStatics::FakeNetworkInformationStatics(
    base::WeakPtr<FakeWinrtNetworkEnvironment> fake_network_environment)
    : fake_network_environment_(fake_network_environment) {}

HRESULT FakeNetworkInformationStatics::GetConnectionProfiles(
    WinrtCollections::IVectorView<WinrtConnectivity::ConnectionProfile*>**
        value) {
  if (fake_network_environment_->GetErrorStatus() ==
      FakeWinrtNetworkStatus::
          kErrorNetworkInformationStaticsGetConnectionProfilesFailed) {
    return fake_network_environment_->MakeHresult(
        FakeWinrtNetworkStatus::
            kErrorNetworkInformationStaticsGetConnectionProfilesFailed);
  }

  ComPtr<WinrtCollections::IVectorView<WinrtConnectivity::ConnectionProfile*>>
      new_vector_view =
          Make<FakeVectorView<WinrtConnectivity::IConnectionProfile,
                              WinrtConnectivity::ConnectionProfile>>(
              fake_network_environment_);

  *value = new_vector_view.Detach();
  return S_OK;
}

HRESULT FakeNetworkInformationStatics::GetInternetConnectionProfile(
    WinrtConnectivity::IConnectionProfile** value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeNetworkInformationStatics::GetLanIdentifiers(
    WinrtCollections::IVectorView<WinrtConnectivity::LanIdentifier*>** value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeNetworkInformationStatics::GetHostNames(
    WinrtCollections::IVectorView<WinrtNetworking::HostName*>** value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeNetworkInformationStatics::GetProxyConfigurationAsync(
    WinrtFoundation::IUriRuntimeClass* uri,
    WinrtFoundation::IAsyncOperation<WinrtConnectivity::ProxyConfiguration*>**
        value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeNetworkInformationStatics::GetSortedEndpointPairs(
    WinrtCollections::IIterable<WinrtNetworking::EndpointPair*>*
        destination_list,
    WinrtNetworking::HostNameSortOptions sort_options,
    WinrtCollections::IVectorView<WinrtNetworking::EndpointPair*>** value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeNetworkInformationStatics::add_NetworkStatusChanged(
    WinrtConnectivity::INetworkStatusChangedEventHandler*
        network_status_handler,
    EventRegistrationToken* event_cookie) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeNetworkInformationStatics::remove_NetworkStatusChanged(
    EventRegistrationToken event_cookie) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

FakeNetworkInformationStatics::~FakeNetworkInformationStatics() = default;

}  // namespace media_router
