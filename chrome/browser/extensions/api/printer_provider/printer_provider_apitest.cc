// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <functional>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/repeating_test_future.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/printer_provider/printer_provider_api.h"
#include "extensions/browser/api/printer_provider/printer_provider_api_factory.h"
#include "extensions/browser/api/printer_provider/printer_provider_print_job.h"
#include "extensions/browser/api/usb/usb_device_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

using ContextType = ExtensionBrowserTest::ContextType;

using GetPrintersRepeatingFuture =
    base::test::RepeatingTestFuture<base::Value::List, bool>;

base::Value::List FetchPrintersFromFuture(
    std::unique_ptr<GetPrintersRepeatingFuture> future) {
  base::Value::List printers_out;
  while (true) {
    auto [printers, done] = future->Take();
    for (auto& printer : printers) {
      EXPECT_TRUE(printer.is_dict());
      printers_out.Append(std::move(printer));
    }
    if (done) {
      break;
    }
  }
  return printers_out;
}

std::pair<bool, std::string> ParsePrintResult(const base::Value& status) {
  bool success = status.is_none();
  std::string status_str = success ? "OK" : status.GetString();
  return {success, status_str};
}

std::string SerializeDict(const base::Value::Dict& value) {
  std::string result;
  JSONStringValueSerializer serializer(&result);
  EXPECT_TRUE(serializer.Serialize(value));
  return result;
}

// Tests for chrome.printerProvider API.
class PrinterProviderApiTest : public ExtensionApiTest,
                               public testing::WithParamInterface<ContextType> {
 public:
  enum PrintRequestDataType {
    PRINT_REQUEST_DATA_TYPE_NOT_SET,
    PRINT_REQUEST_DATA_TYPE_BYTES
  };

  PrinterProviderApiTest() : ExtensionApiTest(GetParam()) {}
  ~PrinterProviderApiTest() override = default;
  PrinterProviderApiTest(const PrinterProviderApiTest&) = delete;
  PrinterProviderApiTest& operator=(const PrinterProviderApiTest&) = delete;

 protected:
  void StartGetPrintersRequest(
      const PrinterProviderAPI::GetPrintersCallback& callback) {
    PrinterProviderAPIFactory::GetInstance()
        ->GetForBrowserContext(profile())
        ->DispatchGetPrintersRequested(callback);
  }

  void StartPrintRequestWithNoData(const ExtensionId& extension_id,
                                   PrinterProviderAPI::PrintCallback callback) {
    PrinterProviderPrintJob job;
    job.printer_id = extension_id + ":printer_id";
    job.content_type = "application/pdf";

    PrinterProviderAPIFactory::GetInstance()
        ->GetForBrowserContext(profile())
        ->DispatchPrintRequested(std::move(job), std::move(callback));
  }

  void StartPrintRequestUsingDocumentBytes(
      const ExtensionId& extension_id,
      PrinterProviderAPI::PrintCallback callback) {
    PrinterProviderPrintJob job;
    job.printer_id = extension_id + ":printer_id";
    job.job_title = u"Print job";
    job.content_type = "application/pdf";
    const unsigned char kDocumentBytes[] = {'b', 'y', 't', 'e', 's'};
    job.document_bytes = new base::RefCountedBytes(kDocumentBytes);

    PrinterProviderAPIFactory::GetInstance()
        ->GetForBrowserContext(profile())
        ->DispatchPrintRequested(std::move(job), std::move(callback));
  }

  void StartCapabilityRequest(
      const ExtensionId& extension_id,
      PrinterProviderAPI::GetCapabilityCallback callback) {
    PrinterProviderAPIFactory::GetInstance()
        ->GetForBrowserContext(profile())
        ->DispatchGetCapabilityRequested(extension_id + ":printer_id",
                                         std::move(callback));
  }

  // Loads chrome.printerProvider test extension and initializes it for test.
  // |test_param|.
  // When the extension's background page is loaded, it will send a 'loaded'
  // message. As a response to the message it will expect a string message
  // specifying the test that should be run. When the extension initializes its
  // state (e.g. registers listener for a chrome.printerProvider event) it will
  // send the message 'ready', at which point the test may be started.
  // If the extension is successfully initialized, |*extension_id_out| will be
  // set to the loaded extension's id, otherwise it will remain unchanged.
  void InitializePrinterProviderTestExtension(const std::string& extension_path,
                                              const std::string& test_param,
                                              ExtensionId* extension_id_out) {
    ExtensionTestMessageListener loaded_listener("loaded",
                                                 ReplyBehavior::kWillReply);
    ExtensionTestMessageListener ready_listener("ready");

    const Extension* extension =
        LoadExtension(test_data_dir_.AppendASCII(extension_path));
    ASSERT_TRUE(extension);
    ASSERT_TRUE(loaded_listener.WaitUntilSatisfied());

    loaded_listener.Reply(test_param);

    ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

    *extension_id_out = extension->id();
  }

  // Runs a test for chrome.printerProvider.onPrintRequested event.
  // |test_param|: The test that should be run.
  // |expected_result|: The print result the extension is expected to report.
  void RunPrintRequestTestExtension(const std::string& test_param,
                                    PrintRequestDataType data_type,
                                    const std::string& expected_result) {
    ResultCatcher catcher;

    ExtensionId extension_id;
    InitializePrinterProviderTestExtension("printer_provider/request_print",
                                           test_param, &extension_id);
    if (extension_id.empty())
      return;

    base::RunLoop run_loop;
    bool success;
    std::string status;
    // TestFuture cannot be used with <const base::Value&>.
    auto callback = base::BindLambdaForTesting([&](const base::Value& result) {
                      std::tie(success, status) = ParsePrintResult(result);
                    }).Then(run_loop.QuitClosure());
    switch (data_type) {
      case PRINT_REQUEST_DATA_TYPE_NOT_SET:
        StartPrintRequestWithNoData(extension_id, std::move(callback));
        break;
      case PRINT_REQUEST_DATA_TYPE_BYTES:
        StartPrintRequestUsingDocumentBytes(extension_id, std::move(callback));
        break;
    }

    if (data_type != PRINT_REQUEST_DATA_TYPE_NOT_SET)
      ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

    run_loop.Run();
    EXPECT_EQ(expected_result, status);
    EXPECT_EQ(expected_result == "OK", success);
  }

  // Runs a test for chrome.printerProvider.onGetCapabilityRequested
  // event.
  // |test_param|: The test that should be run.
  // |expected_result|: The capability the extension is expected to report.
  void RunPrinterCapabilitiesRequestTest(const std::string& test_param,
                                         const std::string& expected_result) {
    ResultCatcher catcher;

    ExtensionId extension_id;
    InitializePrinterProviderTestExtension(
        "printer_provider/request_capability", test_param, &extension_id);
    if (extension_id.empty())
      return;

    base::test::TestFuture<base::Value::Dict> capability_future;
    StartCapabilityRequest(extension_id, capability_future.GetCallback());

    ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

    auto result = SerializeDict(capability_future.Get());
    EXPECT_EQ(expected_result, result);
  }

  bool SimulateExtensionUnload(const ExtensionId& extension_id) {
    ExtensionRegistry* extension_registry = ExtensionRegistry::Get(profile());

    scoped_refptr<const Extension> extension =
        extension_registry->enabled_extensions().GetByID(extension_id);
    if (!extension)
      return false;

    extension_registry->RemoveEnabled(extension_id);
    extension_registry->TriggerOnUnloaded(extension.get(),
                                          UnloadedExtensionReason::TERMINATE);
    return true;
  }

  // Validates that set of printers reported by test extensions via
  // chrome.printerProvider.onGetPritersRequested is the same as the set of
  // printers in |expected_printers|. |expected_printers| contains list of
  // printer objects formatted as a JSON string. It is assumed that the values
  // in |expected_printers| are unique.
  void ValidatePrinterListValue(
      const base::Value::List& printers,
      const std::vector<base::Value::Dict>& expected_printers) {
    ASSERT_EQ(expected_printers.size(), printers.size());
    for (const auto& printer_value : expected_printers) {
      EXPECT_TRUE(base::Contains(printers, printer_value))
          << "Unable to find " << printer_value << " in " << printers;
    }
  }
};

INSTANTIATE_TEST_SUITE_P(EventPage,
                         PrinterProviderApiTest,
                         ::testing::Values(ContextType::kEventPage));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         PrinterProviderApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(PrinterProviderApiTest, PrintJobSuccess) {
  RunPrintRequestTestExtension("OK", PRINT_REQUEST_DATA_TYPE_BYTES, "OK");
}

IN_PROC_BROWSER_TEST_P(PrinterProviderApiTest, PrintJobAsyncSuccess) {
  RunPrintRequestTestExtension("ASYNC_RESPONSE", PRINT_REQUEST_DATA_TYPE_BYTES,
                               "OK");
}

IN_PROC_BROWSER_TEST_P(PrinterProviderApiTest, PrintJobFailed) {
  RunPrintRequestTestExtension("INVALID_TICKET", PRINT_REQUEST_DATA_TYPE_BYTES,
                               "INVALID_TICKET");
}

IN_PROC_BROWSER_TEST_P(PrinterProviderApiTest, NoPrintEventListener) {
  RunPrintRequestTestExtension("NO_LISTENER", PRINT_REQUEST_DATA_TYPE_BYTES,
                               "FAILED");
}

IN_PROC_BROWSER_TEST_P(PrinterProviderApiTest,
                       PrintRequestInvalidCallbackParam) {
  RunPrintRequestTestExtension("INVALID_VALUE", PRINT_REQUEST_DATA_TYPE_BYTES,
                               "FAILED");
}

IN_PROC_BROWSER_TEST_P(PrinterProviderApiTest, PrintRequestDataNotSet) {
  RunPrintRequestTestExtension("IGNORE_CALLBACK",
                               PRINT_REQUEST_DATA_TYPE_NOT_SET, "FAILED");
}

IN_PROC_BROWSER_TEST_P(PrinterProviderApiTest, PrintRequestExtensionUnloaded) {
  ResultCatcher catcher;

  ExtensionId extension_id;
  InitializePrinterProviderTestExtension("printer_provider/request_print",
                                         "IGNORE_CALLBACK", &extension_id);
  ASSERT_FALSE(extension_id.empty());

  base::RunLoop run_loop;
  bool success;
  std::string status;
  // TestFuture cannot be used with <const base::Value&>.
  StartPrintRequestUsingDocumentBytes(
      extension_id, base::BindLambdaForTesting([&](const base::Value& result) {
                      std::tie(success, status) = ParsePrintResult(result);
                    }).Then(run_loop.QuitClosure()));

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  ASSERT_TRUE(SimulateExtensionUnload(extension_id));

  run_loop.Run();
  EXPECT_FALSE(success);
  EXPECT_EQ("FAILED", status);
}

IN_PROC_BROWSER_TEST_P(PrinterProviderApiTest, GetCapabilitySuccess) {
  RunPrinterCapabilitiesRequestTest("OK", "{\"capability\":\"value\"}");
}

IN_PROC_BROWSER_TEST_P(PrinterProviderApiTest, GetCapabilityAsyncSuccess) {
  RunPrinterCapabilitiesRequestTest("ASYNC_RESPONSE",
                                    "{\"capability\":\"value\"}");
}

IN_PROC_BROWSER_TEST_P(PrinterProviderApiTest, EmptyCapability) {
  RunPrinterCapabilitiesRequestTest("EMPTY", "{}");
}

IN_PROC_BROWSER_TEST_P(PrinterProviderApiTest, NoCapabilityEventListener) {
  RunPrinterCapabilitiesRequestTest("NO_LISTENER", "{}");
}

IN_PROC_BROWSER_TEST_P(PrinterProviderApiTest, CapabilityInvalidValue) {
  RunPrinterCapabilitiesRequestTest("INVALID_VALUE", "{}");
}

IN_PROC_BROWSER_TEST_P(PrinterProviderApiTest, GetCapabilityExtensionUnloaded) {
  ResultCatcher catcher;

  ExtensionId extension_id;
  InitializePrinterProviderTestExtension("printer_provider/request_capability",
                                         "IGNORE_CALLBACK", &extension_id);
  ASSERT_FALSE(extension_id.empty());

  base::test::TestFuture<base::Value::Dict> capability_future;
  StartCapabilityRequest(extension_id, capability_future.GetCallback());
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  ASSERT_TRUE(SimulateExtensionUnload(extension_id));
  auto result = SerializeDict(capability_future.Get());
  EXPECT_EQ("{}", result);
}

IN_PROC_BROWSER_TEST_P(PrinterProviderApiTest, GetPrintersSuccess) {
  ResultCatcher catcher;

  ExtensionId extension_id;
  InitializePrinterProviderTestExtension("printer_provider/request_printers",
                                         "OK", &extension_id);
  ASSERT_FALSE(extension_id.empty());

  auto get_printers_repeating_future =
      std::make_unique<GetPrintersRepeatingFuture>();
  StartGetPrintersRequest(get_printers_repeating_future->GetCallback());

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  std::vector<base::Value::Dict> expected_printers;
  expected_printers.push_back(
      base::Value::Dict()
          .Set("description", "Test printer")
          .Set("extensionId", extension_id)
          .Set("extensionName", "Test printer provider")
          .Set("id", base::StringPrintf("%s:printer1", extension_id.c_str()))
          .Set("name", "Printer 1"));
  expected_printers.push_back(
      base::Value::Dict()
          .Set("extensionId", extension_id)
          .Set("extensionName", "Test printer provider")
          .Set("id",
               base::StringPrintf("%s:printerNoDesc", extension_id.c_str()))
          .Set("name", "Printer 2"));

  auto printers =
      FetchPrintersFromFuture(std::move(get_printers_repeating_future));
  ValidatePrinterListValue(printers, expected_printers);
}

IN_PROC_BROWSER_TEST_P(PrinterProviderApiTest, GetPrintersAsyncSuccess) {
  ResultCatcher catcher;

  ExtensionId extension_id;
  InitializePrinterProviderTestExtension("printer_provider/request_printers",
                                         "ASYNC_RESPONSE", &extension_id);
  ASSERT_FALSE(extension_id.empty());

  auto get_printers_repeating_future =
      std::make_unique<GetPrintersRepeatingFuture>();
  StartGetPrintersRequest(get_printers_repeating_future->GetCallback());

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  std::vector<base::Value::Dict> expected_printers;
  expected_printers.push_back(
      base::Value::Dict()
          .Set("description", "Test printer")
          .Set("extensionId", extension_id)
          .Set("extensionName", "Test printer provider")
          .Set("id", base::StringPrintf("%s:printer1", extension_id.c_str()))
          .Set("name", "Printer 1"));

  auto printers =
      FetchPrintersFromFuture(std::move(get_printers_repeating_future));
  ValidatePrinterListValue(printers, expected_printers);
}

IN_PROC_BROWSER_TEST_P(PrinterProviderApiTest, GetPrintersTwoExtensions) {
  ResultCatcher catcher;

  ExtensionId extension_id_1;
  InitializePrinterProviderTestExtension("printer_provider/request_printers",
                                         "OK", &extension_id_1);
  ASSERT_FALSE(extension_id_1.empty());

  ExtensionId extension_id_2;
  InitializePrinterProviderTestExtension(
      "printer_provider/request_printers_second", "OK", &extension_id_2);
  ASSERT_FALSE(extension_id_2.empty());

  auto get_printers_repeating_future =
      std::make_unique<GetPrintersRepeatingFuture>();
  StartGetPrintersRequest(get_printers_repeating_future->GetCallback());

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  std::vector<base::Value::Dict> expected_printers;
  expected_printers.push_back(
      base::Value::Dict()
          .Set("description", "Test printer")
          .Set("extensionId", extension_id_1)
          .Set("extensionName", "Test printer provider")
          .Set("id", base::StringPrintf("%s:printer1", extension_id_1.c_str()))
          .Set("name", "Printer 1"));
  expected_printers.push_back(
      base::Value::Dict()
          .Set("extensionId", extension_id_1)
          .Set("extensionName", "Test printer provider")
          .Set("id",
               base::StringPrintf("%s:printerNoDesc", extension_id_1.c_str()))
          .Set("name", "Printer 2"));
  expected_printers.push_back(
      base::Value::Dict()
          .Set("description", "Test printer")
          .Set("extensionId", extension_id_2)
          .Set("extensionName", "Test printer provider")
          .Set("id", base::StringPrintf("%s:printer1", extension_id_2.c_str()))
          .Set("name", "Printer 1"));
  expected_printers.push_back(
      base::Value::Dict()
          .Set("extensionId", extension_id_2)
          .Set("extensionName", "Test printer provider")
          .Set("id",
               base::StringPrintf("%s:printerNoDesc", extension_id_2.c_str()))
          .Set("name", "Printer 2"));

  auto printers =
      FetchPrintersFromFuture(std::move(get_printers_repeating_future));
  ValidatePrinterListValue(printers, expected_printers);
}

IN_PROC_BROWSER_TEST_P(PrinterProviderApiTest,
                       GetPrintersTwoExtensionsBothUnloaded) {
  ResultCatcher catcher;

  ExtensionId extension_id_1;
  InitializePrinterProviderTestExtension("printer_provider/request_printers",
                                         "IGNORE_CALLBACK", &extension_id_1);
  ASSERT_FALSE(extension_id_1.empty());

  ExtensionId extension_id_2;
  InitializePrinterProviderTestExtension(
      "printer_provider/request_printers_second", "IGNORE_CALLBACK",
      &extension_id_2);
  ASSERT_FALSE(extension_id_2.empty());

  auto get_printers_repeating_future =
      std::make_unique<GetPrintersRepeatingFuture>();
  StartGetPrintersRequest(get_printers_repeating_future->GetCallback());

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  ASSERT_TRUE(SimulateExtensionUnload(extension_id_1));
  ASSERT_TRUE(SimulateExtensionUnload(extension_id_2));

  auto printers =
      FetchPrintersFromFuture(std::move(get_printers_repeating_future));
  EXPECT_TRUE(printers.empty());
}

IN_PROC_BROWSER_TEST_P(PrinterProviderApiTest,
                       GetPrintersTwoExtensionsOneFails) {
  ResultCatcher catcher;

  ExtensionId extension_id_1;
  InitializePrinterProviderTestExtension("printer_provider/request_printers",
                                         "NOT_ARRAY", &extension_id_1);
  ASSERT_FALSE(extension_id_1.empty());

  ExtensionId extension_id_2;
  InitializePrinterProviderTestExtension(
      "printer_provider/request_printers_second", "OK", &extension_id_2);
  ASSERT_FALSE(extension_id_2.empty());

  auto get_printers_repeating_future =
      std::make_unique<GetPrintersRepeatingFuture>();
  StartGetPrintersRequest(get_printers_repeating_future->GetCallback());

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  std::vector<base::Value::Dict> expected_printers;
  expected_printers.push_back(
      base::Value::Dict()
          .Set("description", "Test printer")
          .Set("extensionId", extension_id_2)
          .Set("extensionName", "Test printer provider")
          .Set("id", base::StringPrintf("%s:printer1", extension_id_2.c_str()))
          .Set("name", "Printer 1"));
  expected_printers.push_back(
      base::Value::Dict()
          .Set("extensionId", extension_id_2)
          .Set("extensionName", "Test printer provider")
          .Set("id",
               base::StringPrintf("%s:printerNoDesc", extension_id_2.c_str()))
          .Set("name", "Printer 2"));

  auto printers =
      FetchPrintersFromFuture(std::move(get_printers_repeating_future));
  ValidatePrinterListValue(printers, expected_printers);
}

IN_PROC_BROWSER_TEST_P(PrinterProviderApiTest,
                       GetPrintersTwoExtensionsOneWithNoListener) {
  ResultCatcher catcher;

  ExtensionId extension_id_1;
  InitializePrinterProviderTestExtension("printer_provider/request_printers",
                                         "NO_LISTENER", &extension_id_1);
  ASSERT_FALSE(extension_id_1.empty());

  ExtensionId extension_id_2;
  InitializePrinterProviderTestExtension(
      "printer_provider/request_printers_second", "OK", &extension_id_2);
  ASSERT_FALSE(extension_id_2.empty());

  auto get_printers_repeating_future =
      std::make_unique<GetPrintersRepeatingFuture>();
  StartGetPrintersRequest(get_printers_repeating_future->GetCallback());

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  std::vector<base::Value::Dict> expected_printers;
  expected_printers.push_back(
      base::Value::Dict()
          .Set("description", "Test printer")
          .Set("extensionId", extension_id_2)
          .Set("extensionName", "Test printer provider")
          .Set("id", base::StringPrintf("%s:printer1", extension_id_2.c_str()))
          .Set("name", "Printer 1"));
  expected_printers.push_back(
      base::Value::Dict()
          .Set("extensionId", extension_id_2)
          .Set("extensionName", "Test printer provider")
          .Set("id",
               base::StringPrintf("%s:printerNoDesc", extension_id_2.c_str()))
          .Set("name", "Printer 2"));

  auto printers =
      FetchPrintersFromFuture(std::move(get_printers_repeating_future));
  ValidatePrinterListValue(printers, expected_printers);
}

IN_PROC_BROWSER_TEST_P(PrinterProviderApiTest, GetPrintersNoListener) {
  ResultCatcher catcher;

  ExtensionId extension_id;
  InitializePrinterProviderTestExtension("printer_provider/request_printers",
                                         "NO_LISTENER", &extension_id);
  ASSERT_FALSE(extension_id.empty());

  auto get_printers_repeating_future =
      std::make_unique<GetPrintersRepeatingFuture>();
  StartGetPrintersRequest(get_printers_repeating_future->GetCallback());

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  auto printers =
      FetchPrintersFromFuture(std::move(get_printers_repeating_future));
  EXPECT_TRUE(printers.empty());
}

IN_PROC_BROWSER_TEST_P(PrinterProviderApiTest, GetPrintersNotArray) {
  ResultCatcher catcher;

  ExtensionId extension_id;
  InitializePrinterProviderTestExtension("printer_provider/request_printers",
                                         "NOT_ARRAY", &extension_id);
  ASSERT_FALSE(extension_id.empty());

  auto get_printers_repeating_future =
      std::make_unique<GetPrintersRepeatingFuture>();
  StartGetPrintersRequest(get_printers_repeating_future->GetCallback());

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  auto printers =
      FetchPrintersFromFuture(std::move(get_printers_repeating_future));
  EXPECT_TRUE(printers.empty());
}

IN_PROC_BROWSER_TEST_P(PrinterProviderApiTest,
                       GetPrintersInvalidPrinterValueType) {
  ResultCatcher catcher;

  ExtensionId extension_id;
  InitializePrinterProviderTestExtension("printer_provider/request_printers",
                                         "INVALID_PRINTER_TYPE", &extension_id);
  ASSERT_FALSE(extension_id.empty());

  auto get_printers_repeating_future =
      std::make_unique<GetPrintersRepeatingFuture>();
  StartGetPrintersRequest(get_printers_repeating_future->GetCallback());

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  auto printers =
      FetchPrintersFromFuture(std::move(get_printers_repeating_future));
  EXPECT_TRUE(printers.empty());
}

IN_PROC_BROWSER_TEST_P(PrinterProviderApiTest, GetPrintersInvalidPrinterValue) {
  ResultCatcher catcher;

  ExtensionId extension_id;
  InitializePrinterProviderTestExtension("printer_provider/request_printers",
                                         "INVALID_PRINTER", &extension_id);
  ASSERT_FALSE(extension_id.empty());

  auto get_printers_repeating_future =
      std::make_unique<GetPrintersRepeatingFuture>();
  StartGetPrintersRequest(get_printers_repeating_future->GetCallback());

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  auto printers =
      FetchPrintersFromFuture(std::move(get_printers_repeating_future));
  EXPECT_TRUE(printers.empty());
}

// These tests are separate out from the main test class because the USB api
// is only available to apps.
class PrinterProviderUsbApiTest : public PrinterProviderApiTest {
 public:
  PrinterProviderUsbApiTest() = default;
  PrinterProviderUsbApiTest(const PrinterProviderUsbApiTest&) = delete;
  PrinterProviderUsbApiTest& operator=(const PrinterProviderUsbApiTest&) =
      delete;

 protected:
  void StartGetUsbPrinterInfoRequest(
      const ExtensionId& extension_id,
      const device::mojom::UsbDeviceInfo& device,
      PrinterProviderAPI::GetPrinterInfoCallback callback) {
    PrinterProviderAPIFactory::GetInstance()
        ->GetForBrowserContext(profile())
        ->DispatchGetUsbPrinterInfoRequested(extension_id, device,
                                             std::move(callback));
  }

  // Run a test for the chrome.printerProvider.onGetUsbPrinterInfoRequested
  // event.
  // |test_param|: The test that should be run.
  // |expected_result|: The printer info that the app is expected to report.
  void RunUsbPrinterInfoRequestTest(const std::string& test_param) {
    ResultCatcher catcher;
    device::mojom::UsbDeviceInfoPtr device =
        usb_manager_.CreateAndAddDevice(0, 0, "Google", "USB Printer", "");

    ExtensionId extension_id;
    InitializePrinterProviderTestExtension("printer_provider/usb_printers",
                                           test_param, &extension_id);
    ASSERT_FALSE(extension_id.empty());

    base::test::TestFuture<base::Value::Dict> future;
    StartGetUsbPrinterInfoRequest(extension_id, *device, future.GetCallback());
    ASSERT_TRUE(future.Get().empty());

    ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  }

  device::FakeUsbDeviceManager usb_manager_;
};

// These are only instantiated for event page-based packaged apps.
INSTANTIATE_TEST_SUITE_P(EventPage,
                         PrinterProviderUsbApiTest,
                         ::testing::Values(ContextType::kEventPage));

IN_PROC_BROWSER_TEST_P(PrinterProviderUsbApiTest, GetUsbPrinterInfo) {
  ResultCatcher catcher;
  device::mojom::UsbDeviceInfoPtr device =
      usb_manager_.CreateAndAddDevice(0, 0, "Google", "USB Printer", "");

  ExtensionId extension_id;
  InitializePrinterProviderTestExtension("printer_provider/usb_printers", "OK",
                                         &extension_id);
  ASSERT_FALSE(extension_id.empty());

  UsbDeviceManager* device_manager = UsbDeviceManager::Get(profile());
  base::Value::Dict expected_printer_info =
      base::Value::Dict()
          .Set("description", "This printer is a USB device.")
          .Set("extensionId", extension_id)
          .Set("extensionName", "Test USB printer provider")
          .Set("id",
               base::StringPrintf("%s:usbDevice-%u", extension_id.c_str(),
                                  device_manager->GetIdFromGuid(device->guid)))
          .Set("name", "Test Printer");
  base::test::TestFuture<base::Value::Dict> future;
  StartGetUsbPrinterInfoRequest(extension_id, *device, future.GetCallback());
  ASSERT_EQ(future.Get(), expected_printer_info);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(PrinterProviderUsbApiTest,
                       GetUsbPrinterInfoEmptyResponse) {
  RunUsbPrinterInfoRequestTest("EMPTY_RESPONSE");
}

IN_PROC_BROWSER_TEST_P(PrinterProviderUsbApiTest, GetUsbPrinterInfoNoListener) {
  RunUsbPrinterInfoRequestTest("NO_LISTENER");
}

}  // namespace

}  // namespace extensions
