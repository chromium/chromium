// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/mahi_browser_delegate_ash.h"

#include <memory>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/mahi.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {
// Check the received page content is equal to the expected page context.
void OnGetContent(crosapi::mojom::MahiPageContentPtr expected_page_content,
                  crosapi::mojom::MahiPageContentPtr page_content) {
  // If no page content is returned, both should be empty.
  if (!expected_page_content) {
    EXPECT_FALSE(page_content);
    return;
  }
  // Otherwise, the returned page content should be the same as expected.
  EXPECT_EQ(expected_page_content->client_id, page_content->client_id);
  EXPECT_EQ(expected_page_content->page_id, page_content->page_id);
  EXPECT_EQ(expected_page_content->page_content, page_content->page_content);
}

}  // namespace

class FakeMahiBrowserCppClient : public crosapi::mojom::MahiBrowserClient {
 public:
  explicit FakeMahiBrowserCppClient(
      MahiBrowserDelegateAsh& mahi_browser_delegate)
      : client_id_(base::UnguessableToken::Create()),
        mahi_browser_delegate_(mahi_browser_delegate) {
    mahi_browser_delegate_->RegisterCppClient(this, client_id_);
  }
  FakeMahiBrowserCppClient(const FakeMahiBrowserCppClient&) = delete;
  FakeMahiBrowserCppClient& operator=(const FakeMahiBrowserCppClient&) = delete;
  ~FakeMahiBrowserCppClient() override {
    mahi_browser_delegate_->UnregisterClient(client_id_);
  }

  void set_valid_id(const base::UnguessableToken& id) { valid_id_ = id; }
  void set_content(const std::u16string content) { content_ = content; }
  const base::UnguessableToken& client_id() { return client_id_; }
  const base::UnguessableToken& last_id() { return last_id_; }

  void GetContent(const base::UnguessableToken& content_id,
                  GetContentCallback callback) override {
    last_id_ = content_id;
    if (!valid_id_ || valid_id_ != content_id) {
      std::move(callback).Run(std::move(nullptr));
      return;
    }

    crosapi::mojom::MahiPageContentPtr page_content =
        crosapi::mojom::MahiPageContent::New();
    page_content->client_id = client_id_;
    page_content->page_id = content_id;
    page_content->page_content = content_;

    std::move(callback).Run(std::move(page_content));
  }

 private:
  base::UnguessableToken client_id_;
  const raw_ref<MahiBrowserDelegateAsh> mahi_browser_delegate_;

  // The valid id for the content.
  base::UnguessableToken valid_id_;
  std::u16string content_;
  base::UnguessableToken last_id_;
};

class MahiBrowserDelegateTest : public testing::Test {
 public:
  MahiBrowserDelegateAsh& mahi_browser_delegate() {
    return mahi_browser_delegate_;
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  MahiBrowserDelegateAsh mahi_browser_delegate_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(MahiBrowserDelegateTest, GetContent) {
  std::unique_ptr<FakeMahiBrowserCppClient> client1 =
      std::make_unique<FakeMahiBrowserCppClient>(mahi_browser_delegate());
  const base::UnguessableToken client1_content_id =
      base::UnguessableToken::Create();
  const std::u16string client1_content = u"content for client 1";
  client1->set_valid_id(client1_content_id);
  client1->set_content(client1_content);

  // Basic functioning.
  // Unmatched contents won't be returned.
  const base::UnguessableToken random_id = base::UnguessableToken::Create();
  ASSERT_NE(client1_content_id, random_id);

  crosapi::mojom::MahiPageContentPtr null_page_content = nullptr;
  mahi_browser_delegate().GetContentFromClient(
      client1->client_id(), random_id,
      base::BindOnce(&OnGetContent, std::move(null_page_content)));
  RunUntilIdle();
  // Client1 receives the request.
  EXPECT_EQ(client1->last_id(), random_id);

  // Matched contents will be returned properly.
  crosapi::mojom::MahiPageContentPtr expected_page_content1 =
      crosapi::mojom::MahiPageContent::New();
  expected_page_content1->client_id = client1->client_id();
  expected_page_content1->page_id = client1_content_id;
  expected_page_content1->page_content = client1_content;
  mahi_browser_delegate().GetContentFromClient(
      client1->client_id(), client1_content_id,
      base::BindOnce(&OnGetContent, std::move(expected_page_content1)));
  RunUntilIdle();
  EXPECT_EQ(client1->last_id(), client1_content_id);

  // Multiple clients.
  std::unique_ptr<FakeMahiBrowserCppClient> client2 =
      std::make_unique<FakeMahiBrowserCppClient>(mahi_browser_delegate());
  const base::UnguessableToken client2_content_id =
      base::UnguessableToken::Create();
  ASSERT_NE(client1_content_id, client2_content_id);
  const std::u16string client2_content = u"content for client 2";
  client2->set_valid_id(client2_content_id);
  client2->set_content(client2_content);

  // Only the client with matched contents will return.
  crosapi::mojom::MahiPageContentPtr expected_page_content2 =
      crosapi::mojom::MahiPageContent::New();
  expected_page_content2->client_id = client2->client_id();
  expected_page_content2->page_id = client2_content_id;
  expected_page_content2->page_content = client2_content;
  mahi_browser_delegate().GetContentFromClient(
      client2->client_id(), client2_content_id,
      base::BindOnce(&OnGetContent, std::move(expected_page_content2)));
  RunUntilIdle();
  EXPECT_EQ(client2->last_id(), client2_content_id);
  // client1 is not visited.
  EXPECT_EQ(client1->last_id(), client1_content_id);

  // Destructed client will be removed from the list.
  const base::UnguessableToken client1_id = client1->client_id();
  client1.reset();
  mahi_browser_delegate().GetContentFromClient(
      client1_id, client1_content_id,
      base::BindOnce(&OnGetContent, std::move(null_page_content)));
  RunUntilIdle();
}

}  // namespace ash
