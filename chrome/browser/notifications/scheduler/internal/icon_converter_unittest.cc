// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/notifications/scheduler/internal/png_icon_converter_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifications {
namespace {

class IconConverterTest : public testing::Test {
 public:
  IconConverterTest() : encoded_result_(), decoded_result_() {}
  ~IconConverterTest() override = default;

  void SetUp() override {
    auto icon_converter = std::make_unique<PngIconConverterImpl>();
    icon_converter_ = std::move(icon_converter);
  }

  void OnIconsEncoded(base::OnceClosure quit_closure,
                      std::unique_ptr<EncodeResult> encoded_result) {
    encoded_result_ = std::move(encoded_result);
    std::move(quit_closure).Run();
  }

  void OnIconsDecoded(base::OnceClosure quit_closure,
                      std::unique_ptr<DecodeResult> decoded_result) {
    decoded_result_ = std::move(decoded_result);
    std::move(quit_closure).Run();
  }

  void VerifyEncodeRoundTrip(std::vector<SkBitmap> input) {
    EXPECT_EQ(decoded_icons()->size(), input.size());
    for (size_t i = 0; i < input.size(); i++) {
      EXPECT_EQ(input[i].height(), decoded_icons()->at(i).height());
      EXPECT_EQ(input[i].width(), decoded_icons()->at(i).width());
    }
  }

 protected:
  IconConverter* icon_converter() { return icon_converter_.get(); }
  std::vector<SkBitmap>* decoded_icons() {
    return &decoded_result_->decoded_icons;
  }
  std::vector<std::string>* encoded_data() {
    return &encoded_result_->encoded_data;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<IconConverter> icon_converter_;
  std::unique_ptr<EncodeResult> encoded_result_;
  std::unique_ptr<DecodeResult> decoded_result_;

  DISALLOW_COPY_AND_ASSIGN(IconConverterTest);
};

TEST_F(IconConverterTest, EncodeRoundTrip) {
  SkBitmap icon1, icon2;
  icon1.allocN32Pixels(3, 4);
  icon2.allocN32Pixels(12, 13);
  std::vector<SkBitmap> input = {std::move(icon1), std::move(icon2)};
  {
    base::RunLoop run_loop;
    icon_converter()->ConvertIconToString(
        input, base::BindOnce(&IconConverterTest::OnIconsEncoded,
                              base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }
  EXPECT_EQ(encoded_data()->size(), input.size());
  {
    base::RunLoop run_loop;
    icon_converter()->ConvertStringToIcon(
        *encoded_data(),
        base::BindOnce(&IconConverterTest::OnIconsDecoded,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    VerifyEncodeRoundTrip(input);
  }
}

}  // namespace
}  // namespace notifications
