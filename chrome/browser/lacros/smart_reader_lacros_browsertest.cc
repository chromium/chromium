// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/chromeos/smart_reader/smart_reader_client_impl.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/smart_reader.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {
namespace {

class FakeSmartReaderClient : public mojom::SmartReaderClient {
 public:
  FakeSmartReaderClient() = default;
  FakeSmartReaderClient(const FakeSmartReaderClient&) = delete;
  FakeSmartReaderClient& operator=(const FakeSmartReaderClient&) = delete;
  ~FakeSmartReaderClient() override = default;

  // crosapi::mojom::SmartReaderClient overrides
  void GetPageContent(GetPageContentCallback callback) override {
    std::move(callback).Run(
        mojom::SmartReaderPageContent::New(u"title", GURL("url"), u"contents"));
  }

  mojo::Receiver<crosapi::mojom::SmartReaderClient> receiver_{this};
};

using SmartReaderLacrosBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(SmartReaderLacrosBrowserTest, Basic) {
  auto* lacros_service = chromeos::LacrosService::Get();
  ASSERT_TRUE(lacros_service);
  if (!lacros_service->IsSupported<crosapi::mojom::SmartReaderClient>()) {
    GTEST_SKIP();
  }
  FakeSmartReaderClient client;
  lacros_service->BindPendingReceiverOrRemote<
      mojo::PendingRemote<crosapi::mojom::SmartReaderClient>,
      &crosapi::mojom::Crosapi::BindSmartReaderClient>(
      client.receiver_.BindNewPipeAndPassRemote());

  client.GetPageContent(base::BindLambdaForTesting(
      [&](crosapi::mojom::SmartReaderPageContentPtr page_content) {
        EXPECT_EQ(page_content->title, u"title");
        EXPECT_EQ(page_content->url, GURL("url"));
        EXPECT_EQ(page_content->content, u"contents");
      }));
}

IN_PROC_BROWSER_TEST_F(SmartReaderLacrosBrowserTest, BasicWithClientImpl) {
  smart_reader::SmartReaderClientImpl client;

  client.GetPageContent(base::BindLambdaForTesting(
      [&](crosapi::mojom::SmartReaderPageContentPtr page_content) {
        EXPECT_EQ(page_content->title, u"");
        EXPECT_EQ(page_content->url, GURL(""));
        EXPECT_EQ(page_content->content, u"");
      }));
}

}  // namespace
}  // namespace crosapi
