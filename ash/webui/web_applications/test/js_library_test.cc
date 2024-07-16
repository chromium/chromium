// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/web_applications/test/js_library_test.h"

#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "url/gurl.h"

namespace {

constexpr base::FilePath::CharType kRootDir[] =
    FILE_PATH_LITERAL("ash/webui/system_apps/public/js/");

constexpr char kSystemAppTestHost[] = "system-app-test";
constexpr char kSystemAppTestURL[] = "chrome://system-app-test";
constexpr char kUntrustedSystemAppTestURL[] =
    "chrome-untrusted://system-app-test/";

bool IsSystemAppTestURL(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) &&
         url.host() == kSystemAppTestHost;
}

void HandleRequest(const std::string& url_path,
                   content::WebUIDataSource::GotDataCallback callback) {
  const auto& path_for_key = [url_path](base::BasePathKey key) {
    base::FilePath path;
    CHECK(base::PathService::Get(key, &path));
    path = path.Append(kRootDir);
    path = path.AppendASCII(url_path.substr(0, url_path.find('?')));
    return path;
  };
  // First try the source dir, then try generated files.
  base::FilePath path = path_for_key(base::BasePathKey::DIR_SRC_TEST_DATA_ROOT);
  std::string contents;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    if (!base::PathExists(path)) {
      path = path_for_key(base::BasePathKey::DIR_GEN_TEST_DATA_ROOT);
    }
    CHECK(base::ReadFileToString(path, &contents)) << path.value();
  }

  std::move(callback).Run(
      base::MakeRefCounted<base::RefCountedString>(std::move(contents)));
}

void SetRequestFilterForDataSource(content::WebUIDataSource& data_source) {
  data_source.SetRequestFilter(
      base::BindRepeating([](const std::string& path) { return true; }),
      base::BindRepeating(&HandleRequest));
}

void CreateAndAddTrustedSystemAppTestDataSource(
    content::BrowserContext* browser_context) {
  content::WebUIDataSource* trusted_source =
      content::WebUIDataSource::CreateAndAdd(browser_context,
                                             kSystemAppTestHost);

  // We need a CSP override to be able to embed a chrome-untrusted:// iframe.
  std::string csp =
      std::string("frame-src ") + kUntrustedSystemAppTestURL + ";";
  trusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, csp);
  trusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");

  SetRequestFilterForDataSource(*trusted_source);
}

void CreateAndAddUntrustedSystemAppTestDataSource(
    content::BrowserContext* browser_context) {
  content::WebUIDataSource* untrusted_source =
      content::WebUIDataSource::CreateAndAdd(browser_context,
                                             kUntrustedSystemAppTestURL);
  untrusted_source->AddFrameAncestor(GURL(kSystemAppTestURL));

  SetRequestFilterForDataSource(*untrusted_source);
}

class JsLibraryTestWebUIController : public ui::MojoWebUIController {
 public:
  explicit JsLibraryTestWebUIController(content::WebUI* web_ui)
      : ui::MojoWebUIController(web_ui) {
    auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
    CreateAndAddTrustedSystemAppTestDataSource(browser_context);
    CreateAndAddUntrustedSystemAppTestDataSource(browser_context);

    // Add ability to request chrome-untrusted: URLs
    web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);
  }
};

class JsLibraryTestWebUIControllerFactory
    : public content::WebUIControllerFactory {
 public:
  JsLibraryTestWebUIControllerFactory() = default;
  ~JsLibraryTestWebUIControllerFactory() override = default;

  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) override {
    return std::make_unique<JsLibraryTestWebUIController>(web_ui);
  }

  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) override {
    if (IsSystemAppTestURL(url)) {
      return reinterpret_cast<content::WebUI::TypeID>(this);
    }
    return content::WebUI::kNoWebUI;
  }

  bool UseWebUIForURL(content::BrowserContext* browser_context,
                      const GURL& url) override {
    return IsSystemAppTestURL(url);
  }

 private:
  content::ScopedWebUIControllerFactoryRegistration scoped_registration_{this};
};

}  // namespace

JsLibraryTest::JsLibraryTest()
    : factory_(std::make_unique<JsLibraryTestWebUIControllerFactory>()) {}

JsLibraryTest::~JsLibraryTest() = default;
