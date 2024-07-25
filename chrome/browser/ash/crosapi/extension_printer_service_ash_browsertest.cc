// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/extension_printer_service_ash.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/test/repeating_test_future.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/extension_printer.mojom.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {

namespace {

ExtensionPrinterServiceAsh* ExtensionPrinterService() {
  return CrosapiManager::Get()->crosapi_ash()->extension_printer_service_ash();
}

base::Value::List CreateTestPrintersSet1() {
  return base::test::ParseJsonList(R"(
    [ {
      "description": "A virtual printer for testing",
      "extensionId": "jbljdigmdjodgkcllikhggoepmmffba1",
      "extensionName": "Test Printer Provider",
      "id": "jbljdigmdjodgkcllikhggoepmmffba1:test-printer-01",
      "name": "Test Printer 01"
    }, {
      "description": "A virtual printer for testing",
      "extensionId": "jbljdigmdjodgkcllikhggoepmmffba1",
      "extensionName": "Test Printer Provider",
      "id": "jbljdigmdjodgkcllikhggoepmmffba1:test-printer-02",
      "name": "Test Printer 02"
    } ]
  )");
}

base::Value::List CreateTestPrintersSet2() {
  return base::test::ParseJsonList(R"(
    [ {
      "description": "A virtual printer for testing",
      "extensionId": "jbljdigmdjodgkcllikhggoepmmffba2",
      "extensionName": "Test Printer Provider",
      "id": "jbljdigmdjodgkcllikhggoepmmffba2:test-printer-03",
      "name": "Test Printer 03"
    }]
  )");
}

class FakeExtensionPrinterServiceProvider
    : public mojom::ExtensionPrinterServiceProvider {
 public:
  void DispatchGetPrintersRequest(
      const ::base::UnguessableToken& request_id) override {
    ExtensionPrinterServiceAsh* service = ExtensionPrinterService();

    service->PrintersAdded(request_id, CreateTestPrintersSet1(),
                           /*is_done=*/false);
    service->PrintersAdded(request_id, CreateTestPrintersSet2(),
                           /*is_done=*/false);

    // Signals that no more printers will be reported.
    service->PrintersAdded(request_id, base::Value::List(),
                           /*is_done=*/true);
  }

  mojo::Receiver<mojom::ExtensionPrinterServiceProvider> receiver_{this};
};

}  // namespace

class ExtensionPrinterServiceAshBrowserTest : public InProcessBrowserTest {
 public:
  ExtensionPrinterServiceAshBrowserTest() = default;

  void VerifyProvider() {
    ExtensionPrinterServiceAsh* service = ExtensionPrinterService();
    EXPECT_TRUE(service->HasProvider());
  }

 private:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    ExtensionPrinterService()->BindReceiver(
        extension_printer_service_remote_.BindNewPipeAndPassReceiver());

    extension_printer_service_remote_->RegisterServiceProvider(
        fake_provider_.receiver_.BindNewPipeAndPassRemote());
    extension_printer_service_remote_.FlushForTesting();
  }

  FakeExtensionPrinterServiceProvider fake_provider_;
  mojo::Remote<mojom::ExtensionPrinterService>
      extension_printer_service_remote_;
};

// Verifies that a service provider is registered.
IN_PROC_BROWSER_TEST_F(ExtensionPrinterServiceAshBrowserTest,
                       RegisterServiceProvider) {
  VerifyProvider();
}

// Verifies that StartGetPrinters can receive printers from multiple extensions.
IN_PROC_BROWSER_TEST_F(ExtensionPrinterServiceAshBrowserTest,
                       StartGetPrinters) {
  base::test::RepeatingTestFuture<base::Value::List> printers_added_future;
  base::test::TestFuture<void> done_future;

  ExtensionPrinterService()->StartGetPrinters(
      printers_added_future.GetCallback(), done_future.GetCallback());

  // Verifies the first set of printers from extension 1.
  const base::Value::List& printers_set1 = printers_added_future.Take();
  EXPECT_EQ(printers_set1.size(), 2u);

  const base::Value::Dict& printer1 = printers_set1[0].GetDict();
  base::ExpectDictStringValue("A virtual printer for testing", printer1,
                              "description");
  base::ExpectDictStringValue("jbljdigmdjodgkcllikhggoepmmffba1", printer1,
                              "extensionId");
  base::ExpectDictStringValue("Test Printer Provider", printer1,
                              "extensionName");
  base::ExpectDictStringValue(
      "jbljdigmdjodgkcllikhggoepmmffba1:test-printer-01", printer1, "id");
  base::ExpectDictStringValue("Test Printer 01", printer1, "name");

  const base::Value::Dict& printer2 = printers_set1[1].GetDict();
  base::ExpectDictStringValue("A virtual printer for testing", printer2,
                              "description");
  base::ExpectDictStringValue("jbljdigmdjodgkcllikhggoepmmffba1", printer2,
                              "extensionId");
  base::ExpectDictStringValue("Test Printer Provider", printer2,
                              "extensionName");
  base::ExpectDictStringValue(
      "jbljdigmdjodgkcllikhggoepmmffba1:test-printer-02", printer2, "id");
  base::ExpectDictStringValue("Test Printer 02", printer2, "name");

  // Verifies the second set of printers from extension 2.
  const base::Value::List& printers_set2 = printers_added_future.Take();
  EXPECT_EQ(printers_set2.size(), 1u);

  const base::Value::Dict& printer3 = printers_set2[0].GetDict();
  base::ExpectDictStringValue("A virtual printer for testing", printer3,
                              "description");
  base::ExpectDictStringValue("jbljdigmdjodgkcllikhggoepmmffba2", printer3,
                              "extensionId");
  base::ExpectDictStringValue("Test Printer Provider", printer3,
                              "extensionName");
  base::ExpectDictStringValue(
      "jbljdigmdjodgkcllikhggoepmmffba2:test-printer-03", printer3, "id");
  base::ExpectDictStringValue("Test Printer 03", printer3, "name");

  // Verifies that the GetPrintersDoneCallback is invoked when no more printers
  // will be reported.
  EXPECT_TRUE(done_future.Wait());
}

}  // namespace crosapi
