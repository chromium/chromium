// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using content::WebContents;

namespace extensions {

namespace {

const char kSubscribePageActionV2[] = "subscribe_page_action_v2/src";
const char kSubscribePageActionV3[] = "subscribe_page_action_v3/src";
const char kSubscribePage[] = "/subscribe.html";
const char kFeedPageMultiRel[] = "/feeds/feed_multi_rel.html";
const char kValidFeedNoLinks[] = "/feeds/feed_nolinks.xml";
const char kValidFeed0[] = "/feeds/feed_script.xml";
const char kValidFeed1[] = "/feeds/feed1.xml";
const char kValidFeed2[] = "/feeds/feed2.xml";
const char kValidFeed3[] = "/feeds/feed3.xml";
const char kValidFeed4[] = "/feeds/feed4.xml";
const char kValidFeed5[] = "/feeds/feed5.xml";
const char kValidFeed6[] = "/feeds/feed6.xml";
const char kInvalidFeed1[] = "/feeds/feed_invalid1.xml";
const char kInvalidFeed2[] = "/feeds/feed_invalid2.xml";
// We need a triple encoded string to prove that we are not decoding twice in
// subscribe.js because one layer is also stripped off when subscribe.js passes
// it to the XMLHttpRequest object.
const char kFeedTripleEncoded[] = "/feeds/url%25255Fdecoding.html";

static const char kScriptFeedTitle[] =
    "document.getElementById('title') ? "
    "  document.getElementById('title').textContent : "
    "  \"element 'title' not found\"";
static const char kScriptAnchor[] =
    "document.getElementById('anchor_0') ? "
    "  document.getElementById('anchor_0').textContent : "
    "  \"element 'anchor_0' not found\"";
static const char kScriptDesc[] =
    "document.getElementById('desc_0') ? "
    "  document.getElementById('desc_0').textContent : "
    "  \"element 'desc_0' not found\"";
static const char kScriptError[] =
    "document.getElementById('error') ? "
    "  document.getElementById('error').textContent : "
    "  \"No error\"";

GURL GetFeedUrl(net::EmbeddedTestServer* server,
                const std::string& feed_page,
                bool direct_url,
                std::string extension_id) {
  GURL feed_url = server->GetURL(feed_page);
  if (direct_url) {
    // We navigate directly to the subscribe page for feeds where the feed
    // sniffing won't work, in other words, as is the case for malformed feeds.
    return GURL(std::string(kExtensionScheme) + url::kStandardSchemeSeparator +
                extension_id + std::string(kSubscribePage) + std::string("?") +
                feed_url.spec());
  } else {
    // Navigate to the feed content (which will cause the extension to try to
    // sniff the type and display the subscribe page in another tab.
    return GURL(feed_url.spec());
  }
}

class NamedFrameCreatedObserver : public content::WebContentsObserver {
 public:
  NamedFrameCreatedObserver(WebContents* web_contents,
                            const std::string& frame_name)
      : WebContentsObserver(web_contents), frame_name_(frame_name) {}

  NamedFrameCreatedObserver(const NamedFrameCreatedObserver&) = delete;
  NamedFrameCreatedObserver& operator=(const NamedFrameCreatedObserver&) =
      delete;

  content::RenderFrameHost* Wait() {
    if (!frame_) {
      run_loop_.Run();
    }

    return frame_;
  }

 private:
  void RenderFrameCreated(
      content::RenderFrameHost* render_frame_host) override {
    if (render_frame_host->GetFrameName() != frame_name_) {
      return;
    }

    frame_ = render_frame_host;
    run_loop_.Quit();
  }

  void RenderFrameDeleted(
      content::RenderFrameHost* render_frame_host) override {
    if (render_frame_host->GetFrameName() != frame_name_) {
      return;
    }

    frame_ = nullptr;
  }

  base::RunLoop run_loop_;
  raw_ptr<content::RenderFrameHost> frame_ = nullptr;
  std::string frame_name_;
};

bool ValidatePageElement(content::RenderFrameHost* frame,
                         const std::string& javascript,
                         const std::string& expected_value) {
  EXPECT_EQ(expected_value, content::EvalJs(frame, javascript));
  return true;
}

// Navigates to a feed page and, if |sniff_xml_type| is set, wait for the
// extension to kick in, detect the feed and redirect to a feed preview page.
// |sniff_xml_type| is generally set to true if the feed is sniffable and false
// for invalid feeds.
void NavigateToFeedAndValidate(net::EmbeddedTestServer* server,
                               const std::string& url,
                               Browser* browser,
                               std::string extension_id,
                               bool sniff_xml_type,
                               const std::string& expected_feed_title,
                               const std::string& expected_item_title,
                               const std::string& expected_item_desc,
                               const std::string& expected_error,
                               std::string expected_msg) {
  if (sniff_xml_type) {
    // TODO(finnur): Implement this is a non-flaky way.
  }

  WebContents* tab = browser->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(tab);
  NamedFrameCreatedObserver subframe_observer(tab, "preview");

  // Navigate to the subscribe page directly.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser, GetFeedUrl(server, url, true, extension_id)));
  ASSERT_TRUE(subframe_observer.Wait() != nullptr);

  std::string message;
  ASSERT_TRUE(message_queue.WaitForMessage(&message));
  expected_msg = "\"" + expected_msg + "\"";
  EXPECT_STREQ(expected_msg.c_str(), message.c_str());

  content::RenderFrameHost* frame = content::FrameMatchingPredicate(
      tab->GetPrimaryPage(),
      base::BindRepeating(&content::FrameMatchesName, "preview"));
  ASSERT_TRUE(ValidatePageElement(tab->GetPrimaryMainFrame(), kScriptFeedTitle,
                                  expected_feed_title));
  ASSERT_TRUE(ValidatePageElement(frame, kScriptAnchor, expected_item_title));
  ASSERT_TRUE(ValidatePageElement(frame, kScriptDesc, expected_item_desc));
  ASSERT_TRUE(ValidatePageElement(frame, kScriptError, expected_error));
}

} // namespace

// Makes sure that the RSS detects RSS feed links, even when rel tag contains
// more than just "alternate".
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, RSSMultiRelLink) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII(kSubscribePageActionV2)));

  // Note to future maintainer: This function only works with pageActions (which
  // implies manifest version 2). Once we stop supporting v2, we can delete this
  // test (and v2 of the extension along with it).
  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(0));

  // Navigate to the feed page.
  GURL feed_url = embedded_test_server()->GetURL(kFeedPageMultiRel);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), feed_url));
  // We should now have one page action ready to go in the LocationBar.
  ASSERT_TRUE(WaitForPageActionVisibilityChangeTo(1));
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, RSSParseFeedValidFeed1) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(kSubscribePageActionV3));
  ASSERT_TRUE(extension);
  std::string id = extension->id();

  NavigateToFeedAndValidate(embedded_test_server(), kValidFeed1, browser(), id,
                            true, "Feed for MyFeedTitle", "Title 1", "Desc",
                            "No error", "PreviewReady");
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, RSSParseFeedValidFeed2) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(kSubscribePageActionV3));
  ASSERT_TRUE(extension);
  std::string id = extension->id();

  NavigateToFeedAndValidate(embedded_test_server(), kValidFeed2, browser(), id,
                            true, "Feed for MyFeed2", "My item title1",
                            "This is a summary.", "No error", "PreviewReady");
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, RSSParseFeedValidFeed3) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(kSubscribePageActionV3));
  ASSERT_TRUE(extension);
  std::string id = extension->id();

  NavigateToFeedAndValidate(embedded_test_server(), kValidFeed3, browser(), id,
                            true, "Feed for Google Code buglist rss feed",
                            "My dear title", "My dear content", "No error",
                            "PreviewReady");
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, RSSParseFeedValidFeed4) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(kSubscribePageActionV3));
  ASSERT_TRUE(extension);
  std::string id = extension->id();

  NavigateToFeedAndValidate(embedded_test_server(), kValidFeed4, browser(), id,
                            true, "Feed for Title chars <script> %23 stop",
                            "Title chars  %23 stop", "My dear content %23 stop",
                            "No error", "PreviewReady");
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, RSSParseFeedValidFeed0) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(kSubscribePageActionV3));
  ASSERT_TRUE(extension);
  std::string id = extension->id();

  // Try a feed with a link with an onclick handler (before r27440 this would
  // trigger a NOTREACHED).
  NavigateToFeedAndValidate(embedded_test_server(), kValidFeed0, browser(), id,
                            true, "Feed for MyFeedTitle", "Title 1",
                            "Desc VIDEO", "No error", "PreviewReady");
}

// TODO(crbug.com/331144174): Re-enable this test
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, DISABLED_RSSParseFeedValidFeed5) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(kSubscribePageActionV3));
  ASSERT_TRUE(extension);
  std::string id = extension->id();

  // Feed with valid but mostly empty xml.
  NavigateToFeedAndValidate(
      embedded_test_server(), kValidFeed5, browser(), id, true,
      "Feed for Unknown feed name", "element 'anchor_0' not found",
      "element 'desc_0' not found", "This feed contains no entries.", "Error");
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, RSSParseFeedValidFeed6) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(kSubscribePageActionV3));
  ASSERT_TRUE(extension);
  std::string id = extension->id();

  // Feed that is technically invalid but still parseable.
  NavigateToFeedAndValidate(embedded_test_server(), kValidFeed6, browser(), id,
                            true, "Feed for MyFeedTitle", "Title 1", "Desc",
                            "No error", "PreviewReady");
}

// TODO(finnur): Once we're able to Closure-compile (via the Chrome build
//               process) the extension along with the HTML sanitizer, we should
//               add a test to confirm <img src="foo.jpg" alt="foo" /> is
//               preserved after sanitizing (the xkcd test).

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, RSSParseFeedInvalidFeed1) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(kSubscribePageActionV3));
  ASSERT_TRUE(extension);
  std::string id = extension->id();

  // Try an empty feed.
  NavigateToFeedAndValidate(
      embedded_test_server(), kInvalidFeed1, browser(), id, false,
      "Feed for Unknown feed name", "element 'anchor_0' not found",
      "element 'desc_0' not found", "This feed contains no entries.", "Error");
}

// TODO(https://crbug.com/331144174): Test is flaky across multiple builders.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest,
                       DISABLED_RSSParseFeedInvalidFeed2) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(kSubscribePageActionV3));
  ASSERT_TRUE(extension);
  std::string id = extension->id();

  // Try a garbage feed.
  NavigateToFeedAndValidate(
      embedded_test_server(), kInvalidFeed2, browser(), id, false,
      "Feed for Unknown feed name", "element 'anchor_0' not found",
      "element 'desc_0' not found", "This feed contains no entries.", "Error");
}

// TODO(https://crbug.com/331144174): Flaky on ASan LSan.
#if defined(ADDRESS_SANITIZER) && defined(LEAK_SANITIZER)
#define MAYBE_RSSParseFeedInvalidFeed3 DISABLED_RSSParseFeedInvalidFeed3
#else
#define MAYBE_RSSParseFeedInvalidFeed3 RSSParseFeedInvalidFeed3
#endif
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, MAYBE_RSSParseFeedInvalidFeed3) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(kSubscribePageActionV3));
  ASSERT_TRUE(extension);
  std::string id = extension->id();

  // Try a feed that doesn't exist.
  NavigateToFeedAndValidate(
      embedded_test_server(), "/foo.xml", browser(), id, false,
      "Feed for Unknown feed name", "element 'anchor_0' not found",
      "element 'desc_0' not found", "This feed contains no entries.", "Error");
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, RSSParseFeedInvalidFeed4) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(kSubscribePageActionV3));
  ASSERT_TRUE(extension);
  std::string id = extension->id();

  // subscribe.js shouldn't double-decode the URL passed in. Otherwise feed
  // links such as http://search.twitter.com/search.atom?lang=en&q=%23chrome
  // will result in no feed being downloaded because %23 gets decoded to # and
  // therefore #chrome is not treated as part of the Twitter query. This test
  // uses an underscore instead of a hash, but the principle is the same. If
  // we start erroneously double decoding again, the path (and the feed) will
  // become valid resulting in a failure for this test.
  NavigateToFeedAndValidate(
      embedded_test_server(), kFeedTripleEncoded, browser(), id, true,
      "Feed for Unknown feed name", "element 'anchor_0' not found",
      "element 'desc_0' not found", "This feed contains no entries.", "Error");
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, RSSParseFeedValidFeedNoLinks) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(kSubscribePageActionV3));
  ASSERT_TRUE(extension);
  std::string id = extension->id();

  // Valid feed but containing no links.
  NavigateToFeedAndValidate(embedded_test_server(), kValidFeedNoLinks,
                            browser(), id, true, "Feed for MyFeedTitle",
                            "Title with no link", "Desc", "No error",
                            "PreviewReady");
}

}  // namespace extensions
