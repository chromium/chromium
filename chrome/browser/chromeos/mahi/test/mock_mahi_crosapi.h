// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MAHI_TEST_MOCK_MAHI_CROSAPI_H_
#define CHROME_BROWSER_CHROMEOS_MAHI_TEST_MOCK_MAHI_CROSAPI_H_

#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/mahi.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace mahi {

class MockMahiCrosapi : public crosapi::mojom::MahiBrowserDelegate {
 public:
  MockMahiCrosapi();
  ~MockMahiCrosapi() override;

  MOCK_METHOD(void,
              RegisterMojoClient,
              (mojo::PendingRemote<crosapi::mojom::MahiBrowserClient>,
               const base::UnguessableToken&,
               RegisterMojoClientCallback),
              (override));
  MOCK_METHOD(void,
              OnFocusedPageChanged,
              (crosapi::mojom::MahiPageInfoPtr, OnFocusedPageChangedCallback),
              (override));
  MOCK_METHOD(void,
              OnContextMenuClicked,
              (crosapi::mojom::MahiContextMenuRequestPtr,
               OnContextMenuClickedCallback),
              (override));
};

}  // namespace mahi

#endif  // CHROME_BROWSER_CHROMEOS_MAHI_TEST_MOCK_MAHI_CROSAPI_H_
