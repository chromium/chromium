// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_prompt.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/extension_install_ui.h"
#include "chrome/common/buildflags.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/image_loader.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/icons/extension_icon_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/permissions/permission_set.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using extensions::Extension;
using extensions::InstallPromptData;
using extensions::Manifest;
using extensions::PermissionMessage;
using extensions::PermissionMessages;
using extensions::PermissionSet;

namespace {

// Returns bitmap for the default icon with size equal to the default icon's
// pixel size under maximal supported scale factor.
SkBitmap GetDefaultIconBitmapForMaxScaleFactor(bool is_app) {
  const gfx::ImageSkia& image = is_app ?
      extensions::util::GetDefaultAppIcon() :
      extensions::util::GetDefaultExtensionIcon();
  return image
      .GetRepresentation(ui::GetScaleForMaxSupportedResourceScaleFactor())
      .GetBitmap();
}

}  // namespace

InstallPromptData::PromptType
    ExtensionInstallPrompt::g_last_prompt_type_for_tests =
        InstallPromptData::UNSET_PROMPT_TYPE;

// static
InstallPromptData::PromptType
ExtensionInstallPrompt::GetReEnablePromptTypeForExtension(
    content::BrowserContext* context,
    const extensions::Extension* extension) {
  bool is_remote_install =
      context &&
      extensions::ExtensionPrefs::Get(context)->HasDisableReason(
          extension->id(), extensions::disable_reason::DISABLE_REMOTE_INSTALL);

  return is_remote_install ? InstallPromptData::REMOTE_INSTALL_PROMPT
                           : InstallPromptData::RE_ENABLE_PROMPT;
}

ExtensionInstallPrompt::ExtensionInstallPrompt(
    content::WebContents* contents,
    std::unique_ptr<InstallPromptData> prompt)
    : profile_(contents
                   ? Profile::FromBrowserContext(contents->GetBrowserContext())
                   : nullptr),
      extension_(nullptr),
      install_ui_(ExtensionInstallUI::Create(profile_)),
      show_params_(new ExtensionInstallPromptShowParams(contents)),
      prompt_(std::move(prompt)),
      did_call_show_dialog_(false) {}

ExtensionInstallPrompt::ExtensionInstallPrompt(
    Profile* profile,
    gfx::NativeWindow native_window,
    std::unique_ptr<InstallPromptData> prompt)
    : profile_(profile),
      extension_(nullptr),
      install_ui_(ExtensionInstallUI::Create(profile_)),
      show_params_(
          new ExtensionInstallPromptShowParams(profile, native_window)),
      prompt_(std::move(prompt)),
      did_call_show_dialog_(false) {}

ExtensionInstallPrompt::~ExtensionInstallPrompt() = default;

void ExtensionInstallPrompt::ShowDialog(
    DoneCallback done_callback,
    const Extension* extension,
    const SkBitmap* icon,
    const ShowDialogCallback& show_dialog_callback) {
  ShowDialog(std::move(done_callback), extension, icon, nullptr,
             show_dialog_callback);
}

void ExtensionInstallPrompt::ShowDialog(
    DoneCallback done_callback,
    const Extension* extension,
    const SkBitmap* icon,
    std::unique_ptr<const PermissionSet> custom_permissions,
    const ShowDialogCallback& show_dialog_callback) {
  DCHECK(ui_thread_checker_.CalledOnValidThread());
  DCHECK(prompt_);
  CHECK_NE(InstallPromptData::UNSET_PROMPT_TYPE, prompt_->type());

  extension_ = extension;
  done_callback_ = std::move(done_callback);
  if (icon && !icon->empty())
    SetIcon(icon);
  custom_permissions_ = std::move(custom_permissions);
  show_dialog_callback_ = show_dialog_callback;

  // We special-case themes to not show any confirm UI. Instead they are
  // immediately installed, and then we show an infobar (see OnInstallSuccess)
  // to allow the user to revert if they don't like it.
  if (extension->is_theme() && extension->from_webstore() &&
      prompt_->type() != InstallPromptData::EXTENSION_REQUEST_PROMPT &&
      prompt_->type() != InstallPromptData::EXTENSION_PENDING_REQUEST_PROMPT) {
    std::move(done_callback_).Run(DoneCallbackPayload(Result::ACCEPTED));
    return;
  }

  LoadImageIfNeeded();
}

void ExtensionInstallPrompt::OnInstallSuccess(
    scoped_refptr<const Extension> extension,
    SkBitmap* icon) {
  extension_ = extension;
  SetIcon(icon);

  install_ui_->OnInstallSuccess(extension, &icon_);
}

void ExtensionInstallPrompt::OnInstallFailure(
    const extensions::CrxInstallError& error) {
  install_ui_->OnInstallFailure(error);
}

void ExtensionInstallPrompt::SetUseAppInstalledBubble(bool use_bubble) {
  install_ui_->SetUseAppInstalledBubble(use_bubble);
}

void ExtensionInstallPrompt::SetSkipPostInstallUI(bool skip_ui) {
  install_ui_->SetSkipPostInstallUI(skip_ui);
}

void ExtensionInstallPrompt::ConfirmInstall(
    DoneCallback install_callback,
    const extensions::Extension* extension) {
  DCHECK(prompt_);
  // This is only called from CrxInstaller, which is always constructed with an
  // unset type.
  CHECK_EQ(InstallPromptData::UNSET_PROMPT_TYPE, prompt_->type());

  prompt_->set_type(InstallPromptData::INSTALL_PROMPT);
  ShowDialog(std::move(install_callback), extension, nullptr,
             GetDefaultShowDialogCallback());
}

void ExtensionInstallPrompt::ConfirmReEnable(
    DoneCallback install_callback,
    const extensions::Extension* extension,
    content::BrowserContext* browser_context) {
  DCHECK(prompt_);
  // This is only called from CrxInstaller, which is always constructed with an
  // unset type.
  CHECK_EQ(InstallPromptData::UNSET_PROMPT_TYPE, prompt_->type());

  InstallPromptData::PromptType type =
      GetReEnablePromptTypeForExtension(browser_context, extension);
  prompt_->set_type(type);
  ShowDialog(std::move(install_callback), extension, nullptr,
             GetDefaultShowDialogCallback());
}

void ExtensionInstallPrompt::ShowInstallDialog(DoneCallback install_callback,
                                               const Extension* extension,
                                               const SkBitmap* icon) {
  DCHECK(prompt_);
  ShowDialog(std::move(install_callback), extension, icon,
             GetDefaultShowDialogCallback());
}

void ExtensionInstallPrompt::ConfirmPermissions(
    DoneCallback done_callback,
    const Extension* extension,
    std::unique_ptr<const PermissionSet> custom_permissions) {
  DCHECK(prompt_);
  DCHECK(custom_permissions);
  ShowDialog(std::move(done_callback), extension, nullptr,
             std::move(custom_permissions), GetDefaultShowDialogCallback());
}

std::unique_ptr<InstallPromptData>
ExtensionInstallPrompt::GetPromptForTesting() {
  return std::move(prompt_);
}

void ExtensionInstallPrompt::SetIcon(const SkBitmap* image) {
  if (image)
    icon_ = *image;
  else
    icon_ = SkBitmap();
  if (icon_.empty()) {
    // Let's set default icon bitmap whose size is equal to the default icon's
    // pixel size under maximal supported scale factor. If the bitmap is larger
    // than the one we need, it will be scaled down by the ui code.
    icon_ = GetDefaultIconBitmapForMaxScaleFactor(
        extension_ ? extension_->is_app() : false);
  }
}

void ExtensionInstallPrompt::OnImageLoaded(const gfx::Image& image) {
  SetIcon(image.IsEmpty() ? nullptr : image.ToSkBitmap());
  ShowConfirmation();
}

void ExtensionInstallPrompt::LoadImageIfNeeded() {
  // Don't override an icon that was passed in. Also, |profile_| can be null in
  // unit tests.
  if (!icon_.empty() || !profile_) {
    ShowConfirmation();
    return;
  }

  extensions::ExtensionResource image = extensions::IconsInfo::GetIconResource(
      extension_.get(), extension_misc::EXTENSION_ICON_LARGE,
      ExtensionIconSet::Match::kBigger);

  // Load the image asynchronously. The response will be sent to OnImageLoaded.
  extensions::ImageLoader* loader = extensions::ImageLoader::Get(profile_);

  std::vector<extensions::ImageLoader::ImageRepresentation> images_list;
  images_list.push_back(extensions::ImageLoader::ImageRepresentation(
      image, extensions::ImageLoader::ImageRepresentation::NEVER_RESIZE,
      gfx::Size(), ui::k100Percent));
  loader->LoadImagesAsync(extension_.get(), images_list,
                          base::BindOnce(&ExtensionInstallPrompt::OnImageLoaded,
                                         weak_factory_.GetWeakPtr()));
}

void ExtensionInstallPrompt::ShowConfirmation() {
  std::unique_ptr<const PermissionSet> permissions_to_display;

  if (custom_permissions_.get()) {
    permissions_to_display = custom_permissions_->Clone();
  } else if (extension_) {
    permissions_to_display =
        extensions::util::GetInstallPromptPermissionSetForExtension(
            extension_.get(), profile_);
  }

  prompt_->set_extension(extension_.get());
  if (permissions_to_display) {
    prompt_->AddPermissionSet(*permissions_to_display);
  }

  prompt_->set_icon(gfx::Image::CreateFrom1xBitmap(icon_));

  if (show_params_->WasParentDestroyed()) {
    std::move(done_callback_).Run(DoneCallbackPayload(Result::ABORTED));
    return;
  }

  g_last_prompt_type_for_tests = prompt_->type();
  did_call_show_dialog_ = true;

  // Notify observers.
  prompt_->OnDialogOpened();

  // If true, auto confirm is enabled and already handled the result.
  if (AutoConfirmPromptIfEnabled())
    return;

  if (show_dialog_callback_.is_null())
    show_dialog_callback_ = GetDefaultShowDialogCallback();
  // TODO(crbug.com/40625151): Use OnceCallback and eliminate the need for
  // a callback on the stack.
  auto cb = std::move(done_callback_);
  std::move(show_dialog_callback_)
      .Run(std::move(show_params_), std::move(cb), std::move(prompt_));
}

bool ExtensionInstallPrompt::AutoConfirmPromptIfEnabled() {
  auto confirm_value =
      extensions::ScopedTestDialogAutoConfirm::GetAutoConfirmValue();
  switch (confirm_value) {
    case extensions::ScopedTestDialogAutoConfirm::NONE:
      return false;
    // We use PostTask instead of calling the callback directly here, because in
    // the real implementations it's highly likely the message loop will be
    // pumping a few times before the user clicks accept or cancel.
    case extensions::ScopedTestDialogAutoConfirm::ACCEPT:
    case extensions::ScopedTestDialogAutoConfirm::ACCEPT_AND_OPTION: {
      // Permissions are withheld at installation when the prompt specifies it
      // and option wasn't selected (which grants permissions when selected).
      auto result =
          confirm_value == extensions::ScopedTestDialogAutoConfirm::ACCEPT &&
                  prompt_->ShouldWithheldPermissionsOnDialogAccept()
              ? ExtensionInstallPrompt::Result::
                    ACCEPTED_WITH_WITHHELD_PERMISSIONS
              : ExtensionInstallPrompt::Result::ACCEPTED;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(done_callback_),
                         DoneCallbackPayload(
                             result, extensions::ScopedTestDialogAutoConfirm::
                                         GetJustification())));
#if BUILDFLAG(IS_ANDROID)
      // Since the Android tests for supervised user extension installation does
      // not directly create the ExtensionInstallPrompt, this is needed to
      // ensure the observers are notified when we auto confirm the prompt.
      prompt_->OnDialogAccepted();
#endif  // BUILDFLAG(IS_ANDROID)
      return true;
    }
    case extensions::ScopedTestDialogAutoConfirm::CANCEL: {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(done_callback_),
                         DoneCallbackPayload(
                             ExtensionInstallPrompt::Result::USER_CANCELED)));
#if BUILDFLAG(IS_ANDROID)
      // Since the Android tests for supervised user extension installation does
      // not directly create the ExtensionInstallPrompt, this is needed to
      // ensure the observers are notified when we auto confirm the prompt.
      prompt_->OnDialogCanceled();
#endif  // BUILDFLAG(IS_ANDROID)
      return true;
    }
  }

  NOTREACHED();
}
