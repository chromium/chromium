// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_TEST_MOCK_PRINT_PREVIEW_CROSAPI_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_TEST_MOCK_PRINT_PREVIEW_CROSAPI_H_

#include "base/functional/callback_forward.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/print_preview_cros.mojom.h"
#include "components/printing/common/print.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos::printing {

class MockPrintPreviewCrosapi
    : public crosapi::mojom::PrintPreviewCrosDelegate {
 public:
  MockPrintPreviewCrosapi();
  ~MockPrintPreviewCrosapi() override;

  MOCK_METHOD(void,
              RegisterMojoClient,
              (mojo::PendingRemote<crosapi::mojom::PrintPreviewCrosClient>,
               RegisterMojoClientCallback),
              (override));
  MOCK_METHOD(void,
              RequestPrintPreview,
              (const base::UnguessableToken&,
               ::printing::mojom::RequestPrintPreviewParamsPtr,
               RequestPrintPreviewCallback),
              (override));
  MOCK_METHOD(void,
              PrintPreviewDone,
              (const base::UnguessableToken&, PrintPreviewDoneCallback),
              (override));
};

}  // namespace chromeos::printing

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_TEST_MOCK_PRINT_PREVIEW_CROSAPI_H_
