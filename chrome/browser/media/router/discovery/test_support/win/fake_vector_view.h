// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_TEST_SUPPORT_WIN_FAKE_VECTOR_VIEW_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_TEST_SUPPORT_WIN_FAKE_VECTOR_VIEW_H_

#include <windows.foundation.h>
#include <wrl.h>

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "chrome/browser/media/router/discovery/test_support/win/fake_winrt_network_environment.h"

namespace media_router {

template <typename TInterface, typename TClass>
class FakeVectorView final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::WinRt>,
          ABI::Windows::Foundation::Collections::IVectorView<TClass*>> {
 public:
  FakeVectorView(
      base::WeakPtr<FakeWinrtNetworkEnvironment> fake_network_environment)
      : fake_network_environment_(fake_network_environment),
        view_(fake_network_environment->GetConnectionProfiles()) {}

  // Implement the IVectorView interface.
  HRESULT __stdcall GetAt(unsigned index, TInterface** item) final {
    if (fake_network_environment_->GetErrorStatus() ==
        FakeWinrtNetworkStatus::kErrorVectorViewGetAtFailed) {
      return fake_network_environment_->MakeHresult(
          FakeWinrtNetworkStatus::kErrorVectorViewGetAtFailed);
    }

    if (index >= view_.size()) {
      return E_BOUNDS;
    }

    view_[index].CopyTo(item);
    return S_OK;
  }

  HRESULT __stdcall get_Size(unsigned* size) final {
    if (fake_network_environment_->GetErrorStatus() ==
        FakeWinrtNetworkStatus::kErrorVectorViewGetSizeFailed) {
      return fake_network_environment_->MakeHresult(
          FakeWinrtNetworkStatus::kErrorVectorViewGetSizeFailed);
    }

    *size = view_.size();
    return S_OK;
  }

  // The following are not implemented because they are not used.
  HRESULT __stdcall IndexOf(TInterface* value,
                            unsigned* index,
                            boolean* found) final {
    NOTIMPLEMENTED();
    return E_NOTIMPL;
  }

  HRESULT __stdcall GetMany(unsigned start_index,
                            unsigned capacity,
                            TInterface** value,
                            unsigned* actual) final {
    NOTIMPLEMENTED();
    return E_NOTIMPL;
  }

  FakeVectorView(const FakeVectorView&) = delete;
  FakeVectorView& operator=(const FakeVectorView&) = delete;

 private:
  ~FakeVectorView() final = default;

  base::WeakPtr<FakeWinrtNetworkEnvironment> fake_network_environment_;

  std::vector<Microsoft::WRL::ComPtr<TInterface>> view_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_TEST_SUPPORT_WIN_FAKE_VECTOR_VIEW_H_
