// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smart_reader/smart_reader_client_impl.h"

#include <memory>
#include <string>

#include "base/test/bind.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace smart_reader {

// Fake class for testing SmartReaderClientImpl. Has a function to set the
// current page content in the client.
class FakeSmartReaderClient : public SmartReaderClientImpl {
 public:
  FakeSmartReaderClient() = default;

  FakeSmartReaderClient(const FakeSmartReaderClient&) = delete;
  FakeSmartReaderClient& operator=(const FakeSmartReaderClient&) = delete;

  ~FakeSmartReaderClient() override = default;

  void SetPageContent(const std::u16string& contents,
                      const std::u16string& title,
                      GURL url) {
    contents_ = contents;
    title_ = title;
    url_ = url;
  }
};

class SmartReaderClientTest : public InProcessBrowserTest {
 public:
  SmartReaderClientTest() = default;

  SmartReaderClientTest(const SmartReaderClientTest&) = delete;
  SmartReaderClientTest& operator=(const SmartReaderClientTest&) = delete;

  ~SmartReaderClientTest() override = default;
};

IN_PROC_BROWSER_TEST_F(SmartReaderClientTest, GetPageContent) {
  std::unique_ptr<FakeSmartReaderClient> client =
      std::make_unique<FakeSmartReaderClient>();

  std::u16string title = u"Sloth";
  std::u16string contents =
      u"Sloths are a group of Neotropical xenarthran mammals constituting the "
      u"suborder Folivora, including the extant arboreal tree sloths and "
      u"extinct terrestrial ground sloths. Noted for their slowness of "
      u"movement, tree sloths spend most of their lives hanging upside down in "
      u"the trees of the tropical rainforests of South America and Central "
      u"America.";
  std::string url = "www.sloth-facts.com";

  client->SetPageContent(contents, title, GURL(url));

  client->GetPageContent(base::BindLambdaForTesting(
      [&](crosapi::mojom::SmartReaderPageContentPtr page_content) {
        EXPECT_EQ(page_content->title, title);
        EXPECT_EQ(page_content->url, GURL(url));
        EXPECT_EQ(page_content->content, contents);
      }));
}

}  // namespace smart_reader
