// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/extensions/arc_support_message_host.h"

#include <memory>
#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class TestClient : public extensions::NativeMessageHost::Client {
 public:
  TestClient() = default;

  TestClient(const TestClient&) = delete;
  TestClient& operator=(const TestClient&) = delete;

  ~TestClient() override = default;

  void PostMessageFromNativeHost(const std::string& message) override {
    messages_.push_back(message);
  }

  void CloseChannel(const std::string& error_message) override {
    // Do nothing.
  }

  const std::vector<std::string>& messages() const { return messages_; }

 private:
  std::vector<std::string> messages_;
};

class TestObserver : public ArcSupportMessageHost::Observer {
 public:
  TestObserver() = default;

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  ~TestObserver() override = default;

  void OnMessage(const base::Value::Dict& message) override {
    values_.push_back(message.Clone());
  }

  const std::vector<base::Value::Dict>& values() const { return values_; }

 private:
  std::vector<base::Value::Dict> values_;
};

class ArcSupportMessageHostTest : public testing::Test {
 public:
  ArcSupportMessageHostTest() = default;

  ArcSupportMessageHostTest(const ArcSupportMessageHostTest&) = delete;
  ArcSupportMessageHostTest& operator=(const ArcSupportMessageHostTest&) =
      delete;

  ~ArcSupportMessageHostTest() override = default;

  void SetUp() override {
    client_ = std::make_unique<TestClient>();
    message_host_ = ArcSupportMessageHost::Create(nullptr);
    message_host_->Start(client_.get());
  }

  void TearDown() override {
    message_host_.reset();
    client_.reset();
  }

  ArcSupportMessageHost* message_host() {
    return static_cast<ArcSupportMessageHost*>(message_host_.get());
  }

  TestClient* client() { return client_.get(); }

  void OnMessage(const std::string& message) {
    message_host_->OnMessage(message);
  }

 private:
  // Fake as if the current testing thread is UI thread.
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestClient> client_;
  std::unique_ptr<extensions::NativeMessageHost> message_host_;
};

TEST_F(ArcSupportMessageHostTest, SendMessage) {
  base::Value::Dict value;
  value.Set("foo", "bar");
  value.Set("baz", true);

  message_host()->SendMessage(value);

  ASSERT_EQ(1u, client()->messages().size());
  std::optional<base::Value> recieved_value =
      base::JSONReader::Read(client()->messages()[0]);
  EXPECT_EQ(value, recieved_value->GetDict());
}

TEST_F(ArcSupportMessageHostTest, ReceiveMessage) {
  base::Value::Dict value;
  value.Set("foo", "bar");
  value.Set("baz", true);

  TestObserver observer;
  message_host()->SetObserver(&observer);

  std::string value_string;
  base::JSONWriter::Write(value, &value_string);
  OnMessage(value_string);

  message_host()->SetObserver(nullptr);

  ASSERT_EQ(1u, observer.values().size());
  EXPECT_EQ(value, observer.values()[0]);
}

}  // namespace arc
