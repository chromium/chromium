// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/test_support/win/fake_network_adapter.h"

#include "base/notreached.h"
#include "chrome/browser/media/router/discovery/test_support/win/fake_winrt_network_environment.h"

namespace WinrtConnectivity = ABI::Windows::Networking::Connectivity;
namespace WinrtFoundation = ABI::Windows::Foundation;

namespace media_router {

FakeNetworkAdapter::FakeNetworkAdapter(
    base::WeakPtr<FakeWinrtNetworkEnvironment> fake_network_environment,
    const GUID& network_adapter_id)
    : fake_network_environment_(fake_network_environment),
      network_adapter_id_(network_adapter_id) {}

HRESULT FakeNetworkAdapter::get_NetworkAdapterId(GUID* value) {
  if (fake_network_environment_->GetErrorStatus() ==
      FakeWinrtNetworkStatus::kErrorGetNetworkAdapterIdFailed) {
    return fake_network_environment_->MakeHresult(
        FakeWinrtNetworkStatus::kErrorGetNetworkAdapterIdFailed);
  }
  *value = network_adapter_id_;
  return S_OK;
}

HRESULT FakeNetworkAdapter::get_OutboundMaxBitsPerSecond(UINT64* value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeNetworkAdapter::get_InboundMaxBitsPerSecond(UINT64* value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeNetworkAdapter::get_IanaInterfaceType(UINT32* value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeNetworkAdapter::get_NetworkItem(
    WinrtConnectivity::INetworkItem** value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeNetworkAdapter::GetConnectedProfileAsync(
    WinrtFoundation::IAsyncOperation<WinrtConnectivity::ConnectionProfile*>**
        value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

FakeNetworkAdapter::~FakeNetworkAdapter() = default;

}  // namespace media_router
