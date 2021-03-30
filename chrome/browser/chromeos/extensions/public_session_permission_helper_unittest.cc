// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/public_session_permission_helper.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chromeos/login/login_state/scoped_test_public_session_login_state.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permission_set.h"
#include "testing/gtest/include/gtest/gtest.h"

using extension_test_util::LoadManifestUnchecked;
using content::WebContents;
using extensions::APIPermission;
using extensions::Extension;
using extensions::Manifest;
using Result = ExtensionInstallPrompt::Result;
using extensions::mojom::APIPermissionID;

namespace extensions {
namespace permission_helper {
namespace {

auto permission_a = APIPermissionID::kAudio;
auto permission_b = APIPermissionID::kBookmark;
bool did_show_dialog;

const char kWhitelistedId[] = "cbkkbcmdlboombapidmoeolnmdacpkch";
const char kNonWhitelistedId[] = "bogus";

scoped_refptr<Extension> LoadManifestHelper(const std::string& id) {
  std::string error;
  scoped_refptr<Extension> extension =
      LoadManifestUnchecked("common/background_page", "manifest.json",
                            mojom::ManifestLocation::kInvalidLocation,
                            Extension::NO_FLAGS, id, &error);
  EXPECT_TRUE(extension.get()) << error;
  return extension;
}

bool get_did_show_dialog_and_reset() {
  bool tmp = did_show_dialog;
  did_show_dialog = false;
  return tmp;
}

base::OnceCallback<void(const PermissionIDSet&)> BindQuitLoop(
    base::RunLoop* loop) {
  return base::BindOnce(
      [](base::RunLoop* loop, const PermissionIDSet&) { loop->Quit(); }, loop);
}

class ProgrammableInstallPrompt
    : public ExtensionInstallPrompt,
      public base::SupportsWeakPtr<ProgrammableInstallPrompt> {
 public:
  explicit ProgrammableInstallPrompt(WebContents* contents)
      : ExtensionInstallPrompt(contents) {}

  ~ProgrammableInstallPrompt() override {}

  void ShowDialog(
      DoneCallback done_callback,
      const extensions::Extension* extension,
      const SkBitmap* icon,
      std::unique_ptr<Prompt> prompt,
      std::unique_ptr<const extensions::PermissionSet> custom_permissions,
      const ShowDialogCallback& show_dialog_callback) override {
    done_callback_ = std::move(done_callback);
    did_show_dialog = true;
  }

  void Resolve(ExtensionInstallPrompt::Result result) {
    std::move(done_callback_).Run(result);
  }

 private:
  ExtensionInstallPrompt::DoneCallback done_callback_;

  DISALLOW_COPY_AND_ASSIGN(ProgrammableInstallPrompt);
};

}  // namespace

class PublicSessionPermissionHelperTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PublicSessionPermissionHelperTest() {}
  ~PublicSessionPermissionHelperTest() override {}

  // testing::Test
  void SetUp() override;
  void TearDown() override;

  // Class helpers.
  void RequestResolved(const PermissionIDSet& allowed_permissions);
  std::unique_ptr<ExtensionInstallPrompt> ReturnPrompt(
      std::unique_ptr<ExtensionInstallPrompt> prompt,
      WebContents* web_contents);
  base::WeakPtr<ProgrammableInstallPrompt> CallHandlePermissionRequest(
      const scoped_refptr<Extension>& extension,
      const PermissionIDSet& permissions);

 protected:
  scoped_refptr<Extension> extension_a_;
  scoped_refptr<Extension> extension_b_;

  std::vector<PermissionIDSet> allowed_permissions_;

  std::unique_ptr<chromeos::ScopedTestPublicSessionLoginState> login_state_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PublicSessionPermissionHelperTest);
};

void PublicSessionPermissionHelperTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  login_state_.reset(new chromeos::ScopedTestPublicSessionLoginState());
  extension_a_ = LoadManifestHelper("extension_a");
  extension_b_ = LoadManifestHelper("extension_b");
}

void PublicSessionPermissionHelperTest::TearDown() {
  login_state_.reset();
  ResetPermissionsForTesting();
  ChromeRenderViewHostTestHarness::TearDown();
}

void PublicSessionPermissionHelperTest::RequestResolved(
    const PermissionIDSet& allowed_permissions) {
  allowed_permissions_.push_back(allowed_permissions);
}

std::unique_ptr<ExtensionInstallPrompt>
PublicSessionPermissionHelperTest::ReturnPrompt(
    std::unique_ptr<ExtensionInstallPrompt> prompt,
    WebContents* web_contents) {
  return prompt;
}

base::WeakPtr<ProgrammableInstallPrompt>
PublicSessionPermissionHelperTest::CallHandlePermissionRequest(
    const scoped_refptr<Extension>& extension,
    const PermissionIDSet& permissions) {
  auto* prompt = new ProgrammableInstallPrompt(web_contents());
  auto prompt_weak_ptr = prompt->AsWeakPtr();
  auto factory_callback = base::BindOnce(
      &PublicSessionPermissionHelperTest::ReturnPrompt, base::Unretained(this),
      base::WrapUnique<ExtensionInstallPrompt>(prompt));
  HandlePermissionRequest(
      *extension.get(), permissions, web_contents(),
      base::BindOnce(&PublicSessionPermissionHelperTest::RequestResolved,
                     base::Unretained(this)),
      std::move(factory_callback));
  // In case all permissions were already prompted, ReturnPrompt isn't called
  // because of an early return in HandlePermissionRequest, and in that case the
  // prompt is free'd as soon as HandlePermissionRequest returns (because it's
  // owned by a unique_ptr). Using a weak ptr we can detect when this happens.
  return prompt_weak_ptr;
}

TEST_F(PublicSessionPermissionHelperTest, TestPermissionAllowed) {
  // Allow permission_a for extension_a.
  auto prompt = CallHandlePermissionRequest(extension_a_, {permission_a});
  EXPECT_TRUE(prompt);
  EXPECT_TRUE(get_did_show_dialog_and_reset());
  prompt->Resolve(Result::ACCEPTED);
  EXPECT_TRUE(allowed_permissions_.at(0).Equals({permission_a}));

  // permission_a was already allowed for extension_a hence no prompt is being
  // shown, and the ProgrammableInstallPrompt that is passed in is already
  // free'd after CallHandlePermissionRequest returns so the returned weak
  // pointer should evaluate to false.
  EXPECT_FALSE(CallHandlePermissionRequest(extension_a_, {permission_a}));
  EXPECT_FALSE(get_did_show_dialog_and_reset());
  EXPECT_TRUE(allowed_permissions_.at(1).Equals({permission_a}));

  // permission_a was allowed only for extension_a.
  prompt = CallHandlePermissionRequest(extension_b_, {permission_a});
  EXPECT_TRUE(prompt);
  EXPECT_TRUE(get_did_show_dialog_and_reset());
  prompt->Resolve(Result::USER_CANCELED);
  EXPECT_TRUE(allowed_permissions_.at(2).Equals({}));
}

TEST_F(PublicSessionPermissionHelperTest, TestPermissionDenied) {
  // Deny permission_a for extension_a.
  auto prompt = CallHandlePermissionRequest(extension_a_, {permission_a});
  EXPECT_TRUE(prompt);
  EXPECT_TRUE(get_did_show_dialog_and_reset());
  prompt->Resolve(Result::USER_CANCELED);
  EXPECT_TRUE(allowed_permissions_.at(0).Equals({}));

  // Still denied (previous choice is remembered).
  EXPECT_FALSE(CallHandlePermissionRequest(extension_a_, {permission_a}));
  EXPECT_FALSE(get_did_show_dialog_and_reset());
  EXPECT_TRUE(allowed_permissions_.at(1).Equals({}));

  // permission_a was denied only for extension_a.
  prompt = CallHandlePermissionRequest(extension_b_, {permission_a});
  EXPECT_TRUE(prompt);
  EXPECT_TRUE(get_did_show_dialog_and_reset());
  prompt->Resolve(Result::ACCEPTED);
  EXPECT_TRUE(allowed_permissions_.at(2).Equals({permission_a}));
}

TEST_F(PublicSessionPermissionHelperTest, TestTwoPromptsA) {
  // Open two permission prompts.
  auto prompt1 =
      CallHandlePermissionRequest(extension_a_, {permission_a, permission_b});
  EXPECT_TRUE(prompt1);
  EXPECT_TRUE(get_did_show_dialog_and_reset());
  EXPECT_FALSE(CallHandlePermissionRequest(extension_a_, {permission_b}));
  EXPECT_FALSE(get_did_show_dialog_and_reset());
  // prompt1 resolves both permission requests (second permission request
  // doesn't show a prompt as permission_b is already prompted by first
  // permission request).
  prompt1->Resolve(Result::ACCEPTED);
  EXPECT_TRUE(allowed_permissions_.at(0).Equals({permission_a, permission_b}));
  EXPECT_TRUE(allowed_permissions_.at(1).Equals({permission_b}));
}

TEST_F(PublicSessionPermissionHelperTest, TestTwoPromptsB) {
  auto prompt1 = CallHandlePermissionRequest(extension_a_, {permission_a});
  EXPECT_TRUE(prompt1);
  EXPECT_TRUE(get_did_show_dialog_and_reset());
  auto prompt2 =
      CallHandlePermissionRequest(extension_a_, {permission_a, permission_b});
  EXPECT_TRUE(prompt2);
  EXPECT_TRUE(get_did_show_dialog_and_reset());
  // prompt2 resolves only permission_b because prompt1 already prompted for
  // permission_a.
  prompt2->Resolve(Result::ACCEPTED);
  EXPECT_EQ(allowed_permissions_.size(), 0u);
  prompt1->Resolve(Result::ACCEPTED);
  EXPECT_TRUE(allowed_permissions_.at(0).Equals({permission_a}));
  EXPECT_TRUE(allowed_permissions_.at(1).Equals({permission_a, permission_b}));
}

TEST_F(PublicSessionPermissionHelperTest, TestTwoPromptsDeny) {
  auto prompt1 = CallHandlePermissionRequest(extension_a_, {permission_a});
  EXPECT_TRUE(prompt1);
  EXPECT_TRUE(get_did_show_dialog_and_reset());
  auto prompt2 =
      CallHandlePermissionRequest(extension_a_, {permission_a, permission_b});
  EXPECT_TRUE(prompt2);
  EXPECT_TRUE(get_did_show_dialog_and_reset());
  prompt1->Resolve(Result::USER_CANCELED);
  EXPECT_TRUE(allowed_permissions_.at(0).Equals({}));
  prompt2->Resolve(Result::ACCEPTED);
  EXPECT_TRUE(allowed_permissions_.at(1).Equals({permission_b}));
}

TEST_F(PublicSessionPermissionHelperTest, WhitelistedExtension) {
  auto extension = LoadManifestHelper(kWhitelistedId);
  // Whitelisted extension can use any permission.
  EXPECT_TRUE(PermissionAllowed(extension.get(), permission_a));
  EXPECT_TRUE(PermissionAllowed(extension.get(), permission_b));
  // Whitelisted extension is already handled (no permission prompt needed).
  EXPECT_TRUE(HandlePermissionRequest(*extension, {permission_a},
                                      web_contents(), RequestResolvedCallback(),
                                      PromptFactory()));
  EXPECT_TRUE(PermissionAllowed(extension.get(), permission_a));
  EXPECT_TRUE(PermissionAllowed(extension.get(), permission_b));
}

TEST_F(PublicSessionPermissionHelperTest, NonWhitelistedExtension) {
  auto extension = LoadManifestHelper(kNonWhitelistedId);
  EXPECT_FALSE(PermissionAllowed(extension.get(), permission_a));
  EXPECT_FALSE(PermissionAllowed(extension.get(), permission_b));
  // Prompt for permission_a, grant it, verify.
  {
    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::ACCEPT);
    // Permission not handled yet, need to show a prompt.
    base::RunLoop loop;
    EXPECT_FALSE(HandlePermissionRequest(*extension, {permission_a},
                                         web_contents(), BindQuitLoop(&loop),
                                         PromptFactory()));
    loop.Run();
    EXPECT_TRUE(PermissionAllowed(extension.get(), permission_a));
    EXPECT_FALSE(PermissionAllowed(extension.get(), permission_b));
  }
  // Already handled (allow), doesn't show a prompt.
  EXPECT_TRUE(HandlePermissionRequest(*extension, {permission_a},
                                      web_contents(), RequestResolvedCallback(),
                                      PromptFactory()));
  // Prompt for permission_b, deny it, verify.
  {
    ScopedTestDialogAutoConfirm auto_confirm(
        ScopedTestDialogAutoConfirm::CANCEL);
    // Permission not handled yet, need to show a prompt.
    base::RunLoop loop;
    EXPECT_FALSE(HandlePermissionRequest(*extension, {permission_b},
                                         web_contents(), BindQuitLoop(&loop),
                                         PromptFactory()));
    loop.Run();
    EXPECT_TRUE(PermissionAllowed(extension.get(), permission_a));
    EXPECT_FALSE(PermissionAllowed(extension.get(), permission_b));
  }
  // Already handled (deny), doesn't show a prompt.
  EXPECT_TRUE(HandlePermissionRequest(*extension, {permission_b},
                                      web_contents(), RequestResolvedCallback(),
                                      PromptFactory()));
}

}  // namespace permission_helper
}  // namespace extensions
