// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/parent_access_ash.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/ash/parent_access/fake_parent_access_dialog.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_dialog.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/permissions/permission_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

using crosapi::mojom::ParentAccessResultPtr;

using Action = ash::FakeParentAccessDialogProvider::Action;

namespace {
constexpr char test_url[] = "http://example.com";
const std::u16string test_child_display_name = u"child display name";
const gfx::ImageSkia test_favicon = gfx::test::CreateImageSkia(1, 2);
}  // namespace

class ParentAccessAshTest : public testing::Test {
 public:
  ParentAccessAshTest() = default;
  ~ParentAccessAshTest() override = default;

  // testing::Test:
  void SetUp() override {
    parent_access_ash_ = std::make_unique<crosapi::ParentAccessAsh>();
    parent_access_ash_->BindReceiver(
        parent_access_remote_.BindNewPipeAndPassReceiver());
    auto dialog_provider =
        std::make_unique<ash::FakeParentAccessDialogProvider>();
    dialog_provider_ = dialog_provider.get();
    parent_access_ash_->SetDialogProviderForTest(std::move(dialog_provider));
  }

  void TearDown() override {
    dialog_provider_ = nullptr;
    parent_access_ash_.reset();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<crosapi::mojom::ParentAccess> parent_access_remote_;
  std::unique_ptr<crosapi::ParentAccessAsh> parent_access_ash_;
  raw_ptr<ash::FakeParentAccessDialogProvider> dialog_provider_;
};

// Tests that the correct parameters were passed through to the dialog for
// the GetWebsiteParentApproval method.
TEST_F(ParentAccessAshTest, GetWebsiteParentApprovalParams) {
  dialog_provider_->SetNextAction(Action::CaptureCallback(base::DoNothing()));
  parent_access_ash_->GetWebsiteParentApproval(
      GURL(test_url), test_child_display_name, test_favicon, base::DoNothing());

  parent_access_ui::mojom::ParentAccessParamsPtr params =
      dialog_provider_->TakeLastParams();

  // Check for the correct flow type.
  EXPECT_EQ(
      params->flow_type,
      parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess);
  // Check for the correct URL.
  EXPECT_EQ(params->flow_type_params->get_web_approvals_params()->url,
            GURL(test_url));
  // Check for the correct child display name.
  EXPECT_EQ(
      params->flow_type_params->get_web_approvals_params()->child_display_name,
      test_child_display_name);

  // Encode the test bitmap.
  std::optional<std::vector<uint8_t>> favicon_bitmap =
      gfx::PNGCodec::FastEncodeBGRASkBitmap(*test_favicon.bitmap(), false);
  // Make sure it's there.
  EXPECT_EQ(
      params->flow_type_params->get_web_approvals_params()->favicon_png_bytes,
      favicon_bitmap.value());
}


// Makes sure the correct result is returned by the crosapi when the request is
// approved.
TEST_F(ParentAccessAshTest, GetWebsiteParentApproval_Approved) {
  auto dialog_result = std::make_unique<ash::ParentAccessDialog::Result>();
  dialog_result->status = ash::ParentAccessDialog::Result::Status::kApproved;
  dialog_result->parent_access_token = "ABC123";
  dialog_result->parent_access_token_expire_timestamp =
      base::Time::FromSecondsSinceUnixEpoch(123456UL);
  dialog_provider_->SetNextAction(Action::WithResult(std::move(dialog_result)));

  base::test::TestFuture<ParentAccessResultPtr> future;
  parent_access_ash_->GetWebsiteParentApproval(
      GURL(test_url), test_child_display_name, test_favicon,
      future.GetCallback());

  const ParentAccessResultPtr result = future.Take();
  ASSERT_TRUE(result->is_approved());
  EXPECT_EQ(result->get_approved()->parent_access_token, "ABC123");
  EXPECT_EQ(result->get_approved()->parent_access_token_expire_timestamp,
            base::Time::FromSecondsSinceUnixEpoch(123456UL));
}

// Makes sure the correct result is returned by the crosapi when the request
// is declined.
TEST_F(ParentAccessAshTest, GetWebsiteParentApproval_Declined) {
  auto dialog_result = std::make_unique<ash::ParentAccessDialog::Result>();
  dialog_result->status = ash::ParentAccessDialog::Result::Status::kDeclined;
  dialog_provider_->SetNextAction(Action::WithResult(std::move(dialog_result)));

  base::test::TestFuture<ParentAccessResultPtr> future;
  parent_access_ash_->GetWebsiteParentApproval(
      GURL(test_url), test_child_display_name, test_favicon,
      future.GetCallback());

  const ParentAccessResultPtr result = future.Take();
  EXPECT_TRUE(result->is_declined());
}

// Makes sure an cancel result is returned by the crosapi when the request
// is canceled.
TEST_F(ParentAccessAshTest, GetWebsiteParentApproval_Canceled) {
  auto dialog_result = std::make_unique<ash::ParentAccessDialog::Result>();
  dialog_result->status = ash::ParentAccessDialog::Result::Status::kCanceled;
  dialog_provider_->SetNextAction(Action::WithResult(std::move(dialog_result)));

  base::test::TestFuture<ParentAccessResultPtr> future;
  parent_access_ash_->GetWebsiteParentApproval(
      GURL(test_url), test_child_display_name, test_favicon,
      future.GetCallback());

  const ParentAccessResultPtr result = future.Take();
  EXPECT_TRUE(result->is_canceled());
}

// Makes sure an error result is returned by the crosapi when the request
// had an error.
TEST_F(ParentAccessAshTest, GetWebsiteParentApproval_Error) {
  auto dialog_result = std::make_unique<ash::ParentAccessDialog::Result>();
  dialog_result->status = ash::ParentAccessDialog::Result::Status::kError;
  dialog_provider_->SetNextAction(Action::WithResult(std::move(dialog_result)));

  base::test::TestFuture<ParentAccessResultPtr> future;
  parent_access_ash_->GetWebsiteParentApproval(
      GURL(test_url), test_child_display_name, test_favicon,
      future.GetCallback());

  const ParentAccessResultPtr result = future.Take();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error()->type,
            crosapi::mojom::ParentAccessErrorResult::Type::kUnknown);
}

// Makes sure only one ParentAccess request can exist at a time..
TEST_F(ParentAccessAshTest, GetWebsiteParentApproval_AlreadyVisible) {
  base::test::TestFuture<ash::ParentAccessDialog::Callback> dialog_callback;
  dialog_provider_->SetNextAction(
      Action::CaptureCallback(dialog_callback.GetCallback()));
  base::test::TestFuture<ParentAccessResultPtr> successful_show_future;
  parent_access_ash_->GetWebsiteParentApproval(
      GURL(test_url), test_child_display_name, test_favicon,
      successful_show_future.GetCallback());

  dialog_provider_->SetNextAction(Action::DialogAlreadyVisible());

  // Show dialog again, should be blocked because it is already visible.
  base::test::TestFuture<ParentAccessResultPtr> already_visible_future;
  parent_access_ash_->GetWebsiteParentApproval(
      GURL(test_url), test_child_display_name, test_favicon,
      already_visible_future.GetCallback());

  const ParentAccessResultPtr already_visible_result =
      already_visible_future.Take();
  ASSERT_TRUE(already_visible_result->is_error());
  // The error was that it is already visible.
  EXPECT_EQ(already_visible_result->get_error()->type,
            crosapi::mojom::ParentAccessErrorResult::Type::kAlreadyVisible);

  auto dialog_result = std::make_unique<ash::ParentAccessDialog::Result>();
  dialog_result->status = ash::ParentAccessDialog::Result::Status::kApproved;
  dialog_callback.Take().Run(std::move(dialog_result));

  const ParentAccessResultPtr show_result = successful_show_future.Take();
  EXPECT_TRUE(show_result->is_approved());
}

// Makes sure regular users can't request parent access.
TEST_F(ParentAccessAshTest, GetWebsiteParentApproval_NotAChildUser) {
  dialog_provider_->SetNextAction(Action::NotAChildUser());

  base::test::TestFuture<ParentAccessResultPtr> future;
  parent_access_ash_->GetWebsiteParentApproval(
      GURL(test_url), test_child_display_name, test_favicon,
      future.GetCallback());

  const ParentAccessResultPtr result = future.Take();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error()->type,
            crosapi::mojom::ParentAccessErrorResult::Type::kNotAChildUser);
}
