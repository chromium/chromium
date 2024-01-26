// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/top_sites/top_sites_api.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/history/core/browser/top_sites.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api_test_utils.h"

namespace extensions {

namespace utils = api_test_utils;

namespace {

class TopSitesExtensionTest : public InProcessBrowserTest {
 public:
  TopSitesExtensionTest()
      : top_sites_prepopulated_pages_size_(0),
        top_sites_inited_(false),
        waiting_(false) {}
  void SetUpOnMainThread() override {
    base::RunLoop loop;
    quit_closure_ = loop.QuitWhenIdleClosure();
    scoped_refptr<history::TopSites> top_sites =
        TopSitesFactory::GetForProfile(browser()->profile());

    top_sites_prepopulated_pages_size_ =
        top_sites->GetPrepopulatedPages().size();

    // This may return async or sync. If sync, top_sites_inited_ will be true
    // before we get to the conditional below. Otherwise, we'll run a nested
    // message loop until the async callback.
    top_sites->GetMostVisitedURLs(base::BindOnce(
        &TopSitesExtensionTest::OnTopSitesAvailable, base::Unretained(this)));

    if (!top_sites_inited_) {
      waiting_ = true;
      loop.Run();
    }

    // By this point, we know topsites has loaded. We can run the tests now.
  }

  size_t top_sites_prepopulated_pages_size() const {
    return top_sites_prepopulated_pages_size_;
  }

 private:
  void OnTopSitesAvailable(const history::MostVisitedURLList& data) {
    if (waiting_) {
      std::move(quit_closure_).Run();
      waiting_ = false;
    }
    top_sites_inited_ = true;
  }

  size_t top_sites_prepopulated_pages_size_;
  bool top_sites_inited_;
  bool waiting_;
  base::OnceClosure quit_closure_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(TopSitesExtensionTest, GetTopSites) {
  scoped_refptr<TopSitesGetFunction> get_top_sites_function(
      new TopSitesGetFunction());
  // Without a callback the function will not generate a result.
  get_top_sites_function->set_has_callback(true);

  std::optional<base::Value> result = utils::RunFunctionAndReturnSingleResult(
      get_top_sites_function.get(), "[]", browser()->profile());
  ASSERT_TRUE(result->is_list());
  EXPECT_GE(result->GetList().size(), top_sites_prepopulated_pages_size());
}

}  // namespace extensions
