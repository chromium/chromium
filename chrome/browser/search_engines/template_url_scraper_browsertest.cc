// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"

namespace {

class TemplateURLScraperTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "localhost");
  }
};

class TemplateURLServiceLoader {
 public:
  explicit TemplateURLServiceLoader(TemplateURLService* model) : model_(model) {
    if (model_->loaded())
      return;

    scoped_refptr<content::MessageLoopRunner> message_loop_runner =
        new content::MessageLoopRunner;
    base::CallbackListSubscription subscription =
        model_->RegisterOnLoadedCallback(
            base::BindLambdaForTesting([&]() { message_loop_runner->Quit(); }));
    model_->Load();
    message_loop_runner->Run();
  }

  TemplateURLServiceLoader(const TemplateURLServiceLoader&) = delete;
  TemplateURLServiceLoader& operator=(const TemplateURLServiceLoader&) = delete;

 private:
  raw_ptr<TemplateURLService> model_;
};

std::unique_ptr<net::test_server::HttpResponse> SendResponse(
    const net::test_server::HttpRequest& request) {
  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  base::FilePath index_file = test_data_dir.AppendASCII("template_url_scraper")
                                           .AppendASCII("submit_handler")
                                           .AppendASCII("index.html");
  std::string file_contents;
  EXPECT_TRUE(base::ReadFileToString(index_file, &file_contents));
  std::unique_ptr<net::test_server::BasicHttpResponse> response(
      new net::test_server::BasicHttpResponse);
  response->set_content(file_contents);
  return std::move(response);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(TemplateURLScraperTest, ScrapeWithOnSubmit) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&SendResponse));
  ASSERT_TRUE(embedded_test_server()->Start());

  search_engines::SearchEngineChoiceService* search_engine_choice_service =
      search_engines::SearchEngineChoiceServiceFactory::GetForProfile(
          browser()->profile());
  TemplateURLService* template_urls =
      TemplateURLServiceFactory::GetInstance()->GetForProfile(
          browser()->profile());
  TemplateURLServiceLoader loader(template_urls);

  TemplateURLService::TemplateURLVector all_urls =
      template_urls->GetTemplateURLs();

  // We need to substract the default pre-populated engines that the profile is
  // set up with.
  std::vector<std::unique_ptr<TemplateURLData>> prepopulate_urls =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(
          browser()->profile()->GetPrefs(), search_engine_choice_service);
  std::vector<std::unique_ptr<TemplateURLData>> starter_pack_urls =
      TemplateURLStarterPackData::GetStarterPackEngines();

  EXPECT_EQ(prepopulate_urls.size() + starter_pack_urls.size(),
            all_urls.size());

  std::string port(base::NumberToString(embedded_test_server()->port()));
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL("http://www.foo.com:" + port + "/"), 1);

  std::u16string title;
  ui_test_utils::GetCurrentTabTitle(browser(), &title);
  ASSERT_EQ(u"Submit handler TemplateURL scraping test", title);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(web_contents);
  EXPECT_TRUE(content::ExecJs(web_contents, "submit_form()"));
  observer.Wait();

  all_urls = template_urls->GetTemplateURLs();
  EXPECT_EQ(prepopulate_urls.size() + starter_pack_urls.size() + 1,
            all_urls.size());
}
