// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_context.h"

#include <string_view>

#include "base/functional/callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/input_method/editor_geolocation_mock_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::input_method {
namespace {

using base::test::TestFuture;

class FakeObserver : public EditorContext::Observer {
 public:
  explicit FakeObserver(base::OnceCallback<void(std::string_view)> callback)
      : ime_change_callback_(std::move(callback)) {}

  void OnContextUpdated() override {}

  void OnImeChange(std::string_view engine_id) override {
    std::move(ime_change_callback_).Run(engine_id);
  }

 private:
  base::OnceCallback<void(std::string_view)> ime_change_callback_;
};

class FakeSystem : public EditorContext::System {
 public:
  std::optional<ukm::SourceId> GetUkmSourceId() override {
    return std::nullopt;
  }
};

TEST(EditorContextTest, FiresImeOnChange) {
  TestFuture<std::string_view> future;
  EditorGeolocationMockProvider geolocation_provider("au");
  FakeObserver observer(future.GetCallback());
  FakeSystem system;
  EditorContext context(&observer, &system, &geolocation_provider);

  context.OnActivateIme("xkb:es:spa:spa");

  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(future.Get<std::string_view>(), "xkb:es:spa:spa");
}

}  // namespace
}  // namespace ash::input_method
