// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/suggestion_service_ash.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/suggestion_service.mojom-shared.h"
#include "chromeos/crosapi/mojom/suggestion_service.mojom.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "ash/shell.h"

namespace crosapi {
namespace {

using TabsSggestionItemsCallback =
    base::OnceCallback<void(std::vector<mojom::TabSuggestionItemPtr>)>;

crosapi::mojom::TabSuggestionItemPtr MakeMojomTabItem(
    std::string title,
    GURL url,
    base::Time timestamp,
    GURL favicon_url,
    std::string session_name,
    mojom::SuggestionDeviceFormFactor form_factor) {
  crosapi::mojom::TabSuggestionItemPtr item =
      crosapi::mojom::TabSuggestionItem::New();
  item->title = title;
  item->url = url;
  item->timestamp = timestamp;
  item->favicon_url = favicon_url;
  item->session_name = session_name;
  item->form_factor = form_factor;
  return item;
}

class FakeSuggestionServiceProvider : public mojom::SuggestionServiceProvider {
 public:
  void GetTabSuggestionItems(GetTabSuggestionItemsCallback callback) override {
    std::vector<crosapi::mojom::TabSuggestionItemPtr> tab_items;
    tab_items.push_back(MakeMojomTabItem(
        "Title 1", GURL(), base::Time(), GURL(), "Session Name 1",
        mojom::SuggestionDeviceFormFactor::kDesktop));
    tab_items.push_back(MakeMojomTabItem(
        "Title 2", GURL(), base::Time(), GURL(), "Session Name 1",
        mojom::SuggestionDeviceFormFactor::kPhone));
    tab_items.push_back(MakeMojomTabItem(
        "Title 3", GURL(), base::Time(), GURL(), "Session Name 2",
        mojom::SuggestionDeviceFormFactor::kTablet));
    std::move(callback).Run(std::move(tab_items));
  }

  mojo::Receiver<mojom::SuggestionServiceProvider> receiver_{this};
};

class SuggestionServiceAshBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    SuggestionServiceAsh* service =
        CrosapiManager::Get()->crosapi_ash()->suggestion_service_ash();
    service->BindReceiver(
        suggestion_service_remote_.BindNewPipeAndPassReceiver());

    suggestion_service_remote_->AddSuggestionServiceProvider(
        fake_suggestion_service_provider_.receiver_.BindNewPipeAndPassRemote());
    suggestion_service_remote_.FlushForTesting();
  }

 private:
  FakeSuggestionServiceProvider fake_suggestion_service_provider_;
  mojo::Remote<mojom::SuggestionService> suggestion_service_remote_;
};

IN_PROC_BROWSER_TEST_F(SuggestionServiceAshBrowserTest, Basics) {
  base::test::TestFuture<std::vector<crosapi::mojom::TabSuggestionItemPtr>>
      future;
  CrosapiManager::Get()
      ->crosapi_ash()
      ->suggestion_service_ash()
      ->GetTabSuggestionItems(future.GetCallback());

  const auto tabs = future.Take();
  EXPECT_EQ(tabs.size(), 3u);

  EXPECT_EQ(tabs[0]->title, "Title 1");
  EXPECT_EQ(tabs[0]->url, GURL());
  EXPECT_EQ(tabs[0]->timestamp, base::Time());
  EXPECT_EQ(tabs[0]->favicon_url, GURL());
  EXPECT_EQ(tabs[0]->session_name, "Session Name 1");
  EXPECT_EQ(tabs[0]->form_factor, mojom::SuggestionDeviceFormFactor::kDesktop);

  EXPECT_EQ(tabs[1]->title, "Title 2");
  EXPECT_EQ(tabs[1]->url, GURL());
  EXPECT_EQ(tabs[1]->timestamp, base::Time());
  EXPECT_EQ(tabs[1]->favicon_url, GURL());
  EXPECT_EQ(tabs[1]->session_name, "Session Name 1");
  EXPECT_EQ(tabs[1]->form_factor, mojom::SuggestionDeviceFormFactor::kPhone);

  EXPECT_EQ(tabs[2]->title, "Title 3");
  EXPECT_EQ(tabs[2]->url, GURL());
  EXPECT_EQ(tabs[2]->timestamp, base::Time());
  EXPECT_EQ(tabs[2]->favicon_url, GURL());
  EXPECT_EQ(tabs[2]->session_name, "Session Name 2");
  EXPECT_EQ(tabs[2]->form_factor, mojom::SuggestionDeviceFormFactor::kTablet);
}

}  // namespace
}  // namespace crosapi
