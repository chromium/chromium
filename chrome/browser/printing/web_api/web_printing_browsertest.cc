// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/api/printing/printing_test_utils.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chromeos/printing/printer_configuration.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/printing/fake_cups_printers_manager.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/test/gmock_callback_support.h"
#include "chrome/browser/printing/local_printer_utils_chromeos.h"
#include "chrome/test/chromeos/printing/mock_local_printer_chromeos.h"
#include "chromeos/lacros/lacros_service.h"
#endif

namespace printing {

namespace {

constexpr char kId[] = "id";
constexpr char kName[] = "name";

#if BUILDFLAG(IS_CHROMEOS_LACROS)
using testing::_;
using testing::NiceMock;
#endif

}  // namespace

class WebPrintingBrowserTestBase
    : public web_app::IsolatedWebAppBrowserTestHarness {
 public:
  void SetUpOnMainThread() override {
    IsolatedWebAppBrowserTestHarness::SetUpOnMainThread();
    iwa_dev_server_ =
        CreateAndStartServer(FILE_PATH_LITERAL("web_apps/simple_isolated_app"));
    web_app::IsolatedWebAppUrlInfo url_info =
        InstallDevModeProxyIsolatedWebApp(iwa_dev_server_->GetOrigin());
    app_frame_ = OpenApp(url_info.app_id());
  }

  void TearDownOnMainThread() override {
    app_frame_ = nullptr;
    iwa_dev_server_.reset();
  }

 protected:
  content::RenderFrameHost* app_frame() { return app_frame_; }

 private:
  base::test::ScopedFeatureList feature_list_{blink::features::kWebPrinting};

  raw_ptr<content::RenderFrameHost> app_frame_ = nullptr;
  std::unique_ptr<net::EmbeddedTestServer> iwa_dev_server_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
class WebPrintingBrowserTest : public WebPrintingBrowserTestBase {
 public:
  void PreRunTestOnMainThread() override {
    WebPrintingBrowserTestBase::PreRunTestOnMainThread();
    helper_->Init(profile());
  }

  void TearDownOnMainThread() override {
    helper_.reset();
    WebPrintingBrowserTestBase::TearDownOnMainThread();
  }

  void SetUpInProcessBrowserTestFixture() override {
    WebPrintingBrowserTestBase::SetUpInProcessBrowserTestFixture();
    helper_ = std::make_unique<extensions::PrintingTestHelper>();
  }

 protected:
  void AddPrinter(const std::string& printer_id,
                  const std::string& printer_name) {
    chromeos::Printer printer;
    printer.set_id(printer_id);
    printer.set_display_name(printer_name);
    helper_->GetPrintersManager()->AddPrinter(printer,
                                              chromeos::PrinterClass::kSaved);
  }

 private:
  std::unique_ptr<extensions::PrintingTestHelper> helper_;
};
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
class WebPrintingBrowserTest : public WebPrintingBrowserTestBase {
 public:
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    WebPrintingBrowserTestBase::CreatedBrowserMainParts(browser_main_parts);
    chromeos::LacrosService::Get()->InjectRemoteForTesting(
        local_printer_receiver_.BindNewPipeAndPassRemote());
  }

 protected:
  NiceMock<MockLocalPrinter>& local_printer() { return local_printer_; }

 private:
  NiceMock<MockLocalPrinter> local_printer_;
  mojo::Receiver<crosapi::mojom::LocalPrinter> local_printer_receiver_{
      &local_printer_};
};
#endif

IN_PROC_BROWSER_TEST_F(WebPrintingBrowserTest, GetPrinters) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  AddPrinter(kId, kName);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // For some reason first creating a vector of printers and then performing a
  // trick with RunOnceCallback<0>(std::move(printers)) doesn't work.
  EXPECT_CALL(local_printer(), GetPrinters(_))
      .WillOnce([](MockLocalPrinter::GetPrintersCallback callback) {
        chromeos::Printer printer;
        printer.set_id(kId);
        printer.set_display_name(kName);
        std::vector<crosapi::mojom::LocalDestinationInfoPtr> printers;
        printers.push_back(printing::PrinterToMojom(printer));
        std::move(callback).Run(std::move(printers));
      });
#endif

  constexpr base::StringPiece kGetPrintersScript = R"(
    (async () => {
      try {
        const printers = await navigator.printing.getPrinters();
        if (printers.length !== 1 ||
            printers[0].cachedAttributes().printerName !== $1) {
          return false;
        }
        return true;
      } catch (err) {
        console.log(err);
        return false;
      }
    })();
  )";

  ASSERT_TRUE(EvalJs(app_frame(), content::JsReplace(kGetPrintersScript, kName))
                  .ExtractBool());
}

}  // namespace printing
