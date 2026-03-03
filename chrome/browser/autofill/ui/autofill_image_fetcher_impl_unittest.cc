// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/ui/autofill_image_fetcher_impl.h"

#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/resources/grit/ui_resources.h"
#include "url/gurl.h"

namespace autofill {

namespace {

class ScopedFakeScreen : public display::Screen {
 public:
  ScopedFakeScreen() { display::Screen::SetScreenInstance(this); }
  ~ScopedFakeScreen() override { display::Screen::SetScreenInstance(nullptr); }
  display::Display GetPrimaryDisplay() const override { return display_; }

  void SetDeviceScaleFactor(float scale_factor) {
    display_.set_device_scale_factor(scale_factor);
    displays_ = {display_};
  }

  // Unused functions
  gfx::Point GetCursorScreenPoint() override { return gfx::Point(); }
  bool IsWindowUnderCursor(gfx::NativeWindow window) override { return false; }
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override {
    return gfx::NativeWindow();
  }

  gfx::NativeWindow GetLocalProcessWindowAtPoint(
      const gfx::Point& point,
      const std::set<gfx::NativeWindow>& ignore) override {
    return gfx::NativeWindow();
  }
  display::Display GetDisplayNearestWindow(
      gfx::NativeWindow window) const override {
    return display_;
  }
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override {
    return display_;
  }
  int GetNumDisplays() const override { return 0; }
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override {
    return display_;
  }
  void AddObserver(display::DisplayObserver* observer) override {}
  void RemoveObserver(display::DisplayObserver* observer) override {}
  const std::vector<display::Display>& GetAllDisplays() const override {
    return displays_;
  }

 private:
  display::Display display_;
  std::vector<display::Display> displays_;
};

}  // namespace

class AutofillImageFetcherImplTest : public testing::Test {
 public:
  AutofillImageFetcherImplTest()
      : autofill_image_fetcher_(
            std::make_unique<AutofillImageFetcherImpl>(nullptr)) {}

  gfx::Image& GetTestImage(int resource_id) {
    return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
        resource_id);
  }

  AutofillImageFetcherImpl* autofill_image_fetcher() {
    return autofill_image_fetcher_.get();
  }

 private:
  std::unique_ptr<AutofillImageFetcherImpl> autofill_image_fetcher_;
};

TEST_F(AutofillImageFetcherImplTest, ResolveCardArtURL) {
  // We fetch the image at height of 48.
  EXPECT_EQ(GURL("https://www.example.com/fake_image1=h48-pa"),
            autofill_image_fetcher()->ResolveImageURL(
                GURL("https://www.example.com/fake_image1"),
                AutofillImageFetcherBase::ImageType::kCreditCardArtImage));

  // The capitalone image is 'special' however, and we swap it out for the
  // larger variant.
  GURL capital_one_url = GURL(kCapitalOneCardArtUrl);
  EXPECT_EQ(GURL(kCapitalOneLargeCardArtUrl),
            autofill_image_fetcher()->ResolveImageURL(
                capital_one_url,
                AutofillImageFetcherBase::ImageType::kCreditCardArtImage));
}

TEST_F(AutofillImageFetcherImplTest, ResolveCardArtURL_HighDPI) {
  ScopedFakeScreen screen;
  screen.SetDeviceScaleFactor(2.0f);

  // We fetch the image at height of 48 * 2 = 96.
  EXPECT_EQ(GURL("https://www.example.com/fake_image1=h96-pa"),
            autofill_image_fetcher()->ResolveImageURL(
                GURL("https://www.example.com/fake_image1"),
                AutofillImageFetcherBase::ImageType::kCreditCardArtImage));
}

TEST_F(AutofillImageFetcherImplTest, ResolveValuableImageURL) {
  // Valuable images are resized to (96x96) and cropped.
  EXPECT_EQ(GURL("https://www.example.com/fake_image1=h96-w96-cc-rp"),
            autofill_image_fetcher()->ResolveImageURL(
                GURL("https://www.example.com/fake_image1"),
                AutofillImageFetcherBase::ImageType::kValuableImage));
}

}  // namespace autofill
