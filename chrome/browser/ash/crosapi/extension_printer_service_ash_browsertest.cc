// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/extension_printer_service_ash.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/test/metrics/histogram_tester.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {

namespace {

using ::testing::_;
using ::testing::Mock;

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

base::Value::Dict CreateTestCapability() {
  return base::test::ParseJsonDict(R"(
    {
      "version": "1.0",
      "printer": {
        "supported_content_type": [
          {"content_type": "application/pdf"}
        ]
      }
    })");
}

class MockExtensionPrinterServiceProvider
    : public crosapi::mojom::ExtensionPrinterServiceProvider {
 public:
  MOCK_METHOD(void,
              DispatchGetPrintersRequest,
              (const ::base::UnguessableToken& request_id),
              (override));
  MOCK_METHOD(void, DispatchResetRequest, (), (override));
  MOCK_METHOD(void,
              DispatchStartGetCapability,
              (const std::string& destination_id,
               DispatchStartGetCapabilityCallback callback),
              (override));
  MOCK_METHOD(void,
              DispatchStartPrint,
              (const ::std::u16string& job_title,
               ::base::Value::Dict settings,
               ::scoped_refptr<::base::RefCountedMemory> print_data,
               DispatchStartPrintCallback callback),
              (override));
  MOCK_METHOD(void,
              DispatchStartGrantPrinterAccess,
              (const std::string& printer_id,
               DispatchStartGrantPrinterAccessCallback callback),
              (override));

  mojo::Receiver<mojom::ExtensionPrinterServiceProvider> receiver_{this};
};

}  // namespace

class ExtensionPrinterServiceAshBrowserTest : public InProcessBrowserTest {
 public:
  ExtensionPrinterServiceAshBrowserTest() = default;

  void VerifyProvider() {
    ExtensionPrinterServiceAsh* service = ExtensionPrinterService();
    EXPECT_TRUE(service->HasProviderForTesting());
  }

 protected:
  MockExtensionPrinterServiceProvider& mock_provider() {
    return mock_provider_;
  }
  void FlushForTesting() {
    extension_printer_service_remote_.FlushForTesting();
  }

 private:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    ExtensionPrinterService()->BindReceiver(
        extension_printer_service_remote_.BindNewPipeAndPassReceiver());

    extension_printer_service_remote_->RegisterServiceProvider(
        mock_provider_.receiver_.BindNewPipeAndPassRemote());
    extension_printer_service_remote_.FlushForTesting();
  }

  MockExtensionPrinterServiceProvider mock_provider_;
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
  base::HistogramTester histogram_tester;

  constexpr char kNumberOfPrintersMetricName[] =
      "Printing.LacrosExtensions.FromAsh.NumberOfPrinters";

  EXPECT_CALL(mock_provider(), DispatchGetPrintersRequest(_))
      .WillOnce([](const ::base::UnguessableToken& requestId) {
        ExtensionPrinterServiceAsh* service = ExtensionPrinterService();
        // Simulates reporting printers from extension 1.
        service->PrintersAdded(requestId, CreateTestPrintersSet1(), false);
        // Simulates reporting printers from extension 2.
        service->PrintersAdded(requestId, CreateTestPrintersSet2(), false);
        // Simulates reporting printers is done.
        service->PrintersAdded(requestId, base::Value::List(), true);
      });

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
  // The histogram is recorded once with a value of 3 (number of printers).
  histogram_tester.ExpectUniqueSample(kNumberOfPrintersMetricName, 3, 1);
}

// Verifies that StartGetCapability can receive capability.
IN_PROC_BROWSER_TEST_F(ExtensionPrinterServiceAshBrowserTest, Reset) {
  // Captures the request_id.
  base::UnguessableToken captured_request_id;
  // Simulates that a get printers request has been created but reporting
  // printers is not done yet, i.e., the service provider has not called
  // PrintersAdded.
  EXPECT_CALL(mock_provider(), DispatchGetPrintersRequest(_))
      .WillOnce(
          [&captured_request_id](const ::base::UnguessableToken& request_id) {
            captured_request_id = request_id;
          });
  // Verifies that the downstream's Reset has been called.
  EXPECT_CALL(mock_provider(), DispatchResetRequest()).Times(1);

  EXPECT_FALSE(ExtensionPrinterService()->HasAnyPendingGetPrintersRequests());

  // Starts a Get printers request.
  base::test::RepeatingTestFuture<base::Value::List> printers_added_future;
  base::test::TestFuture<void> done_future;
  ExtensionPrinterService()->StartGetPrinters(
      printers_added_future.GetCallback(), done_future.GetCallback());
  FlushForTesting();

  // A pending request with |captured_request_id| has been created.
  EXPECT_TRUE(ExtensionPrinterService()->HasPendingGetPrintersRequestForTesting(
      captured_request_id));

  ExtensionPrinterService()->Reset();
  // The pending request with |captured_request_id| has been cleared.
  EXPECT_FALSE(
      ExtensionPrinterService()->HasPendingGetPrintersRequestForTesting(
          captured_request_id));
  // And there are none pending requests with other ids.
  EXPECT_FALSE(ExtensionPrinterService()->HasAnyPendingGetPrintersRequests());
}

// Verifies that StartGetCapability can receive capability.
IN_PROC_BROWSER_TEST_F(ExtensionPrinterServiceAshBrowserTest,
                       StartGetCapability) {
  EXPECT_CALL(mock_provider(), DispatchStartGetCapability(_, _))
      .WillOnce([](const std::string& destination_id,
                   base::OnceCallback<void(::base::Value::Dict)> callback) {
        std::move(callback).Run(CreateTestCapability());
      });

  base::test::TestFuture<base::Value::Dict> get_capability_future;

  ExtensionPrinterService()->StartGetCapability(
      "jbljdigmdjodgkcllikhggoepmmffba1:test-printer-02",
      get_capability_future.GetCallback());

  const base::Value::Dict& capability = get_capability_future.Take();
  base::ExpectDictStringValue("1.0", capability, "version");

  const base::Value::List* supportedContentTypes =
      capability.FindListByDottedPath("printer.supported_content_type");
  ASSERT_TRUE(supportedContentTypes);
  EXPECT_EQ(supportedContentTypes->size(), 1u);

  const base::Value& contentType1 = (*supportedContentTypes)[0];
  EXPECT_TRUE(contentType1.is_dict());
  base::ExpectDictStringValue("application/pdf", contentType1.GetDict(),
                              "content_type");
}

// Verifies that StartPrint is dispatched correctly.
IN_PROC_BROWSER_TEST_F(ExtensionPrinterServiceAshBrowserTest, StartPrint) {
  // Test data for the print job.
  const std::u16string job_title = u"Test Print Job";
  base::Value::Dict settings = base::test::ParseJsonDict(R"(
    {
      "copies": 2,
      "color": "color"
    }
  )");
  scoped_refptr<base::RefCountedMemory> print_data =
      base::MakeRefCounted<base::RefCountedString>("Test print data");

  // Captures the arguments passed to DispatchStartPrint.
  std::u16string captured_job_title;
  base::Value::Dict captured_settings;
  scoped_refptr<base::RefCountedMemory> captured_print_data;

  // Sets up the expectation for DispatchStartPrint.
  EXPECT_CALL(mock_provider(), DispatchStartPrint(_, _, _, _))
      .WillOnce(
          [&](const std::u16string& job_title, base::Value::Dict settings,
              scoped_refptr<base::RefCountedMemory> print_data,
              MockExtensionPrinterServiceProvider::DispatchStartPrintCallback
                  callback) {
            // Capture the arguments.
            captured_job_title = job_title;
            captured_settings = std::move(settings);
            captured_print_data = print_data;

            // Simulate a successful print job.
            std::move(callback).Run(crosapi::mojom::StartPrintStatus::KOk);
          });

  // Calls the StartPrint method.
  base::test::TestFuture<crosapi::mojom::StartPrintStatus> print_future;
  ExtensionPrinterService()->StartPrint(job_title, std::move(settings),
                                        print_data, print_future.GetCallback());
  FlushForTesting();

  // Verifies the result of the print job.
  EXPECT_EQ(print_future.Get(), crosapi::mojom::StartPrintStatus::KOk);
  // Asserts the captured data matches the input.
  EXPECT_EQ(captured_job_title, job_title);
  EXPECT_EQ(captured_settings,
            base::test::ParseJsonDict(R"({"copies": 2, "color": "color"})"));
  EXPECT_TRUE(captured_print_data->Equals(print_data));
}

// Verifies that StartGrantPrinterAccess is dispatched correctly.
IN_PROC_BROWSER_TEST_F(ExtensionPrinterServiceAshBrowserTest,
                       StartGrantPrinterAccess) {
  const std::string test_printer_id = "test_printer_id_123";
  base::Value::Dict expected_printer_info = base::test::ParseJsonDict(R"(
    {
      "printerId": "test_printer_id_123",
      "name": "Test Printer"
    }
  )");

  EXPECT_CALL(mock_provider(),
              DispatchStartGrantPrinterAccess(test_printer_id, _))
      .WillOnce([&expected_printer_info](
                    const std::string& printer_id,
                    MockExtensionPrinterServiceProvider::
                        DispatchStartGrantPrinterAccessCallback callback) {
        // Calls the callback with the simulated printer info.
        std::move(callback).Run(expected_printer_info.Clone());
      });

  base::test::TestFuture<base::Value::Dict> grant_access_future;
  ExtensionPrinterService()->StartGrantPrinterAccess(
      test_printer_id, grant_access_future.GetCallback());

  const base::Value::Dict& printer_info = grant_access_future.Get();
  EXPECT_EQ(printer_info, expected_printer_info);
}

}  // namespace crosapi
