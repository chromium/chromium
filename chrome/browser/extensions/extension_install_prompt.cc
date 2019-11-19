// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_prompt.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/extension_install_ui_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/image_loader.h"
#include "extensions/browser/install/extension_install_ui.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/image/image_skia.h"

using extensions::Extension;
using extensions::Manifest;
using extensions::PermissionMessage;
using extensions::PermissionMessages;
using extensions::PermissionSet;

namespace {

bool AllowWebstoreData(ExtensionInstallPrompt::PromptType type) {
  return type == ExtensionInstallPrompt::EXTERNAL_INSTALL_PROMPT ||
         type == ExtensionInstallPrompt::REPAIR_PROMPT ||
         type == ExtensionInstallPrompt::WEBSTORE_WIDGET_PROMPT;
}

// Returns bitmap for the default icon with size equal to the default icon's
// pixel size under maximal supported scale factor.
SkBitmap GetDefaultIconBitmapForMaxScaleFactor(bool is_app) {
  const gfx::ImageSkia& image = is_app ?
      extensions::util::GetDefaultAppIcon() :
      extensions::util::GetDefaultExtensionIcon();
  return image.GetRepresentation(gfx::ImageSkia::GetMaxSupportedScale())
      .GetBitmap();
}

}  // namespace

ExtensionInstallPrompt::Prompt::InstallPromptPermissions::
    InstallPromptPermissions() {
}
ExtensionInstallPrompt::Prompt::InstallPromptPermissions::
    ~InstallPromptPermissions() {
}

ExtensionInstallPrompt::PromptType
ExtensionInstallPrompt::g_last_prompt_type_for_tests =
    ExtensionInstallPrompt::UNSET_PROMPT_TYPE;

ExtensionInstallPrompt::Prompt::Prompt(PromptType type)
    : type_(type),
      is_showing_details_for_retained_files_(false),
      is_showing_details_for_retained_devices_(false),
      extension_(nullptr),
      average_rating_(0.0),
      rating_count_(0),
      show_user_count_(false),
      has_webstore_data_(false) {
  DCHECK_NE(type_, UNSET_PROMPT_TYPE);
  DCHECK_NE(type_, NUM_PROMPT_TYPES);
}

ExtensionInstallPrompt::Prompt::~Prompt() {
}

void ExtensionInstallPrompt::Prompt::AddPermissions(
    const PermissionMessages& permissions) {
  for (const PermissionMessage& msg : permissions) {
    prompt_permissions_.permissions.push_back(msg.message());
    // Add a dash to the front of each permission detail.
    base::string16 details;
    if (!msg.submessages().empty()) {
      std::vector<base::string16> detail_lines_with_bullets;
      for (const auto& detail_line : msg.submessages()) {
        detail_lines_with_bullets.push_back(base::ASCIIToUTF16("- ") +
                                            detail_line);
      }

      details = base::JoinString(detail_lines_with_bullets,
                                 base::ASCIIToUTF16("\n"));
    }
    prompt_permissions_.details.push_back(details);
    prompt_permissions_.is_showing_details.push_back(false);
  }
}

void ExtensionInstallPrompt::Prompt::SetIsShowingDetails(
    DetailsType type,
    size_t index,
    bool is_showing_details) {
  switch (type) {
    case PERMISSIONS_DETAILS:
      prompt_permissions_.is_showing_details[index] = is_showing_details;
      break;
    case RETAINED_FILES_DETAILS:
      is_showing_details_for_retained_files_ = is_showing_details;
      break;
    case RETAINED_DEVICES_DETAILS:
      is_showing_details_for_retained_devices_ = is_showing_details;
      break;
  }
}

void ExtensionInstallPrompt::Prompt::SetWebstoreData(
    const std::string& localized_user_count,
    bool show_user_count,
    double average_rating,
    int rating_count) {
  CHECK(AllowWebstoreData(type_));
  localized_user_count_ = localized_user_count;
  show_user_count_ = show_user_count;
  average_rating_ = average_rating;
  rating_count_ = rating_count;
  has_webstore_data_ = true;
}

base::string16 ExtensionInstallPrompt::Prompt::GetDialogTitle() const {
  int id = -1;
  switch (type_) {
    case INSTALL_PROMPT:
    case WEBSTORE_WIDGET_PROMPT:
      id = IDS_EXTENSION_INSTALL_PROMPT_TITLE;
      break;
    case RE_ENABLE_PROMPT:
      id = IDS_EXTENSION_RE_ENABLE_PROMPT_TITLE;
      break;
    case PERMISSIONS_PROMPT:
      id = IDS_EXTENSION_PERMISSIONS_PROMPT_TITLE;
      break;
    case EXTERNAL_INSTALL_PROMPT:
      if (extension_->is_app())
        id = IDS_EXTENSION_EXTERNAL_INSTALL_PROMPT_TITLE_APP;
      else if (extension_->is_theme())
        id = IDS_EXTENSION_EXTERNAL_INSTALL_PROMPT_TITLE_THEME;
      else
        id = IDS_EXTENSION_EXTERNAL_INSTALL_PROMPT_TITLE_EXTENSION;
      break;
    case POST_INSTALL_PERMISSIONS_PROMPT:
      id = IDS_EXTENSION_POST_INSTALL_PERMISSIONS_PROMPT_TITLE;
      break;
    case REMOTE_INSTALL_PROMPT:
      id = IDS_EXTENSION_REMOTE_INSTALL_PROMPT_TITLE;
      break;
    case REPAIR_PROMPT:
      id = IDS_EXTENSION_REPAIR_PROMPT_TITLE;
      break;
    case DELEGATED_PERMISSIONS_PROMPT:
      // Special case: need to include the delegated username.
      return l10n_util::GetStringFUTF16(
          IDS_EXTENSION_DELEGATED_INSTALL_PROMPT_TITLE,
          base::UTF8ToUTF16(extension_->name()),
          base::UTF8ToUTF16(delegated_username_));
    case UNSET_PROMPT_TYPE:
    case NUM_PROMPT_TYPES:
      NOTREACHED();
  }

  return l10n_util::GetStringFUTF16(id, base::UTF8ToUTF16(extension_->name()));
}

int ExtensionInstallPrompt::Prompt::GetDialogButtons() const {
  // The "OK" button in the post install permissions dialog allows revoking
  // file/device access, and is only shown if such permissions exist; see
  // ShouldDisplayRevokeButton().
  if (type_ == POST_INSTALL_PERMISSIONS_PROMPT &&
      !ShouldDisplayRevokeButton()) {
    return ui::DIALOG_BUTTON_CANCEL;
  }
  return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
}

base::string16 ExtensionInstallPrompt::Prompt::GetAcceptButtonLabel() const {
  int id = -1;
  switch (type_) {
    case INSTALL_PROMPT:
    case WEBSTORE_WIDGET_PROMPT:
      if (extension_->is_app())
        id = IDS_EXTENSION_INSTALL_PROMPT_ACCEPT_BUTTON_APP;
      else if (extension_->is_theme())
        id = IDS_EXTENSION_INSTALL_PROMPT_ACCEPT_BUTTON_THEME;
      else
        id = IDS_EXTENSION_INSTALL_PROMPT_ACCEPT_BUTTON_EXTENSION;
      break;
    case RE_ENABLE_PROMPT:
      id = IDS_EXTENSION_PROMPT_RE_ENABLE_BUTTON;
      break;
    case PERMISSIONS_PROMPT:
      id = IDS_EXTENSION_PROMPT_PERMISSIONS_BUTTON;
      break;
    case EXTERNAL_INSTALL_PROMPT:
      if (extension_->is_app())
        id = IDS_EXTENSION_EXTERNAL_INSTALL_PROMPT_ACCEPT_BUTTON_APP;
      else if (extension_->is_theme())
        id = IDS_EXTENSION_EXTERNAL_INSTALL_PROMPT_ACCEPT_BUTTON_THEME;
      else
        id = IDS_EXTENSION_EXTERNAL_INSTALL_PROMPT_ACCEPT_BUTTON_EXTENSION;
      break;
    case POST_INSTALL_PERMISSIONS_PROMPT:
      if (GetRetainedFileCount() && GetRetainedDeviceCount()) {
        id =
            IDS_EXTENSION_PROMPT_PERMISSIONS_CLEAR_RETAINED_FILES_AND_DEVICES_BUTTON;
      } else if (GetRetainedFileCount()) {
        id = IDS_EXTENSION_PROMPT_PERMISSIONS_CLEAR_RETAINED_FILES_BUTTON;
      } else if (GetRetainedDeviceCount()) {
        id = IDS_EXTENSION_PROMPT_PERMISSIONS_CLEAR_RETAINED_DEVICES_BUTTON;
      }
      // If there are neither retained files nor devices, leave id -1 so there
      // will be no "accept" button.
      break;
    case REMOTE_INSTALL_PROMPT:
      if (extension_->is_app())
        id = IDS_EXTENSION_PROMPT_REMOTE_INSTALL_BUTTON_APP;
      else
        id = IDS_EXTENSION_PROMPT_REMOTE_INSTALL_BUTTON_EXTENSION;
      break;
    case REPAIR_PROMPT:
      if (extension_->is_app())
        id = IDS_EXTENSION_PROMPT_REPAIR_BUTTON_APP;
      else
        id = IDS_EXTENSION_PROMPT_REPAIR_BUTTON_EXTENSION;
      break;
    case DELEGATED_PERMISSIONS_PROMPT:
      id = IDS_EXTENSION_PROMPT_INSTALL_BUTTON;
      break;
    case UNSET_PROMPT_TYPE:
    case NUM_PROMPT_TYPES:
      NOTREACHED();
  }

  return id != -1 ? l10n_util::GetStringUTF16(id) : base::string16();
}

base::string16 ExtensionInstallPrompt::Prompt::GetAbortButtonLabel() const {
  int id = -1;
  switch (type_) {
    case INSTALL_PROMPT:
    case WEBSTORE_WIDGET_PROMPT:
    case RE_ENABLE_PROMPT:
    case REMOTE_INSTALL_PROMPT:
    case REPAIR_PROMPT:
    case DELEGATED_PERMISSIONS_PROMPT:
      id = IDS_CANCEL;
      break;
    case PERMISSIONS_PROMPT:
      id = IDS_EXTENSION_PROMPT_PERMISSIONS_ABORT_BUTTON;
      break;
    case EXTERNAL_INSTALL_PROMPT:
      id = IDS_EXTENSION_EXTERNAL_INSTALL_PROMPT_ABORT_BUTTON;
      break;
    case POST_INSTALL_PERMISSIONS_PROMPT:
      id = IDS_CLOSE;
      break;
    case UNSET_PROMPT_TYPE:
    case NUM_PROMPT_TYPES:
      NOTREACHED();
  }

  return l10n_util::GetStringUTF16(id);
}

base::string16 ExtensionInstallPrompt::Prompt::GetPermissionsHeading() const {
  int id = -1;
  switch (type_) {
    case INSTALL_PROMPT:
    case WEBSTORE_WIDGET_PROMPT:
    case EXTERNAL_INSTALL_PROMPT:
    case REMOTE_INSTALL_PROMPT:
    case DELEGATED_PERMISSIONS_PROMPT:
      id = IDS_EXTENSION_PROMPT_WILL_HAVE_ACCESS_TO;
      break;
    case RE_ENABLE_PROMPT:
      id = IDS_EXTENSION_PROMPT_WILL_NOW_HAVE_ACCESS_TO;
      break;
    case PERMISSIONS_PROMPT:
      id = IDS_EXTENSION_PROMPT_WANTS_ACCESS_TO;
      break;
    case POST_INSTALL_PERMISSIONS_PROMPT:
    case REPAIR_PROMPT:
      id = IDS_EXTENSION_PROMPT_CAN_ACCESS;
      break;
    case UNSET_PROMPT_TYPE:
    case NUM_PROMPT_TYPES:
      NOTREACHED();
  }
  return l10n_util::GetStringUTF16(id);
}

base::string16 ExtensionInstallPrompt::Prompt::GetRetainedFilesHeading() const {
  return l10n_util::GetPluralStringFUTF16(
      IDS_EXTENSION_PROMPT_RETAINED_FILES, GetRetainedFileCount());
}

base::string16 ExtensionInstallPrompt::Prompt::GetRetainedDevicesHeading()
    const {
  return l10n_util::GetPluralStringFUTF16(
      IDS_EXTENSION_PROMPT_RETAINED_DEVICES, GetRetainedDeviceCount());
}

bool ExtensionInstallPrompt::Prompt::ShouldShowPermissions() const {
  return GetPermissionCount() > 0 || type_ == POST_INSTALL_PERMISSIONS_PROMPT;
}

void ExtensionInstallPrompt::Prompt::AppendRatingStars(
    StarAppender appender, void* data) const {
  CHECK(appender);
  CHECK(AllowWebstoreData(type_));
  int rating_integer = floor(average_rating_);
  double rating_fractional = average_rating_ - rating_integer;

  if (rating_fractional > 0.66) {
    rating_integer++;
  }

  if (rating_fractional < 0.33 || rating_fractional > 0.66) {
    rating_fractional = 0;
  }

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  int i;
  for (i = 0; i < rating_integer; i++) {
    appender(rb.GetImageSkiaNamed(IDR_EXTENSIONS_RATING_STAR_ON), data);
  }
  if (rating_fractional) {
    appender(rb.GetImageSkiaNamed(IDR_EXTENSIONS_RATING_STAR_HALF_LEFT), data);
    i++;
  }
  for (; i < kMaxExtensionRating; i++) {
    appender(rb.GetImageSkiaNamed(IDR_EXTENSIONS_RATING_STAR_OFF), data);
  }
}

base::string16 ExtensionInstallPrompt::Prompt::GetRatingCount() const {
  CHECK(AllowWebstoreData(type_));
  return l10n_util::GetStringFUTF16(IDS_EXTENSION_RATING_COUNT,
                                    base::NumberToString16(rating_count_));
}

base::string16 ExtensionInstallPrompt::Prompt::GetUserCount() const {
  CHECK(AllowWebstoreData(type_));

  if (show_user_count_) {
    return l10n_util::GetStringFUTF16(IDS_EXTENSION_USER_COUNT,
                                      base::UTF8ToUTF16(localized_user_count_));
  }
  return base::string16();
}

size_t ExtensionInstallPrompt::Prompt::GetPermissionCount() const {
  return prompt_permissions_.permissions.size();
}

size_t ExtensionInstallPrompt::Prompt::GetPermissionsDetailsCount() const {
  return prompt_permissions_.details.size();
}

base::string16 ExtensionInstallPrompt::Prompt::GetPermission(
    size_t index) const {
  CHECK_LT(index, prompt_permissions_.permissions.size());
  return prompt_permissions_.permissions[index];
}

base::string16 ExtensionInstallPrompt::Prompt::GetPermissionsDetails(
    size_t index) const {
  CHECK_LT(index, prompt_permissions_.details.size());
  return prompt_permissions_.details[index];
}

bool ExtensionInstallPrompt::Prompt::GetIsShowingDetails(
    DetailsType type, size_t index) const {
  switch (type) {
    case PERMISSIONS_DETAILS:
      CHECK_LT(index, prompt_permissions_.is_showing_details.size());
      return prompt_permissions_.is_showing_details[index];
    case RETAINED_FILES_DETAILS:
      return is_showing_details_for_retained_files_;
    case RETAINED_DEVICES_DETAILS:
      return is_showing_details_for_retained_devices_;
  }
  return false;
}

size_t ExtensionInstallPrompt::Prompt::GetRetainedFileCount() const {
  return retained_files_.size();
}

base::string16 ExtensionInstallPrompt::Prompt::GetRetainedFile(size_t index)
    const {
  CHECK_LT(index, retained_files_.size());
  return retained_files_[index].AsUTF16Unsafe();
}

size_t ExtensionInstallPrompt::Prompt::GetRetainedDeviceCount() const {
  return retained_device_messages_.size();
}

base::string16 ExtensionInstallPrompt::Prompt::GetRetainedDeviceMessageString(
    size_t index) const {
  CHECK_LT(index, retained_device_messages_.size());
  return retained_device_messages_[index];
}

bool ExtensionInstallPrompt::Prompt::ShouldDisplayRevokeButton() const {
  return !retained_files_.empty() || !retained_device_messages_.empty();
}

bool ExtensionInstallPrompt::Prompt::ShouldDisplayRevokeFilesButton() const {
  return !retained_files_.empty();
}

// static
ExtensionInstallPrompt::PromptType
ExtensionInstallPrompt::GetReEnablePromptTypeForExtension(
    content::BrowserContext* context,
    const extensions::Extension* extension) {
  bool is_remote_install =
      context &&
      extensions::ExtensionPrefs::Get(context)->HasDisableReason(
          extension->id(), extensions::disable_reason::DISABLE_REMOTE_INSTALL);

  return is_remote_install ? REMOTE_INSTALL_PROMPT : RE_ENABLE_PROMPT;
}

// static
scoped_refptr<Extension>
    ExtensionInstallPrompt::GetLocalizedExtensionForDisplay(
    const base::DictionaryValue* manifest,
    int flags,
    const std::string& id,
    const std::string& localized_name,
    const std::string& localized_description,
    std::string* error) {
  std::unique_ptr<base::DictionaryValue> localized_manifest;
  if (!localized_name.empty() || !localized_description.empty()) {
    localized_manifest.reset(manifest->DeepCopy());
    if (!localized_name.empty()) {
      localized_manifest->SetString(extensions::manifest_keys::kName,
                                    localized_name);
    }
    if (!localized_description.empty()) {
      localized_manifest->SetString(extensions::manifest_keys::kDescription,
                                    localized_description);
    }
  }

  return Extension::Create(
      base::FilePath(), Manifest::INTERNAL,
      localized_manifest.get() ? *localized_manifest : *manifest, flags, id,
      error);
}

ExtensionInstallPrompt::ExtensionInstallPrompt(content::WebContents* contents)
    : profile_(contents
                   ? Profile::FromBrowserContext(contents->GetBrowserContext())
                   : nullptr),
      extension_(nullptr),
      install_ui_(extensions::CreateExtensionInstallUI(profile_)),
      show_params_(new ExtensionInstallPromptShowParams(contents)),
      did_call_show_dialog_(false) {}

ExtensionInstallPrompt::ExtensionInstallPrompt(Profile* profile,
                                               gfx::NativeWindow native_window)
    : profile_(profile),
      extension_(nullptr),
      install_ui_(extensions::CreateExtensionInstallUI(profile)),
      show_params_(
          new ExtensionInstallPromptShowParams(profile, native_window)),
      did_call_show_dialog_(false) {}

ExtensionInstallPrompt::~ExtensionInstallPrompt() {
}

void ExtensionInstallPrompt::ShowDialog(
    const DoneCallback& done_callback,
    const Extension* extension,
    const SkBitmap* icon,
    const ShowDialogCallback& show_dialog_callback) {
  ShowDialog(done_callback, extension, icon,
             std::make_unique<Prompt>(INSTALL_PROMPT), show_dialog_callback);
}

void ExtensionInstallPrompt::ShowDialog(
    const DoneCallback& done_callback,
    const Extension* extension,
    const SkBitmap* icon,
    std::unique_ptr<Prompt> prompt,
    const ShowDialogCallback& show_dialog_callback) {
  ShowDialog(done_callback, extension, icon, std::move(prompt), nullptr,
             show_dialog_callback);
}

void ExtensionInstallPrompt::ShowDialog(
    const DoneCallback& done_callback,
    const Extension* extension,
    const SkBitmap* icon,
    std::unique_ptr<Prompt> prompt,
    std::unique_ptr<const PermissionSet> custom_permissions,
    const ShowDialogCallback& show_dialog_callback) {
  DCHECK(ui_thread_checker_.CalledOnValidThread());
  DCHECK(prompt);
  extension_ = extension;
  done_callback_ = done_callback;
  if (icon && !icon->empty())
    SetIcon(icon);
  prompt_ = std::move(prompt);
  custom_permissions_ = std::move(custom_permissions);
  show_dialog_callback_ = show_dialog_callback;

  // We special-case themes to not show any confirm UI. Instead they are
  // immediately installed, and then we show an infobar (see OnInstallSuccess)
  // to allow the user to revert if they don't like it.
  if (extension->is_theme() && extension->from_webstore()) {
    std::move(done_callback_).Run(Result::ACCEPTED);
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
      ExtensionIconSet::MATCH_BIGGER);

  // Load the image asynchronously. The response will be sent to OnImageLoaded.
  extensions::ImageLoader* loader = extensions::ImageLoader::Get(profile_);

  std::vector<extensions::ImageLoader::ImageRepresentation> images_list;
  images_list.push_back(extensions::ImageLoader::ImageRepresentation(
      image,
      extensions::ImageLoader::ImageRepresentation::NEVER_RESIZE,
      gfx::Size(),
      ui::SCALE_FACTOR_100P));
  loader->LoadImagesAsync(extension_.get(), images_list,
                          base::BindOnce(&ExtensionInstallPrompt::OnImageLoaded,
                                         weak_factory_.GetWeakPtr()));
}

void ExtensionInstallPrompt::ShowConfirmation() {
  std::unique_ptr<const PermissionSet> permissions_wrapper;
  const PermissionSet* permissions_to_display = nullptr;
  if (custom_permissions_.get()) {
    permissions_to_display = custom_permissions_.get();
  } else if (extension_) {
    // Initialize permissions if they have not already been set so that
    // any transformations are correctly reflected in the install prompt.
    extensions::PermissionsUpdater(
        profile_, extensions::PermissionsUpdater::INIT_FLAG_TRANSIENT)
        .InitializePermissions(extension_.get());
    permissions_to_display =
        &extension_->permissions_data()->active_permissions();
    // For delegated installs, all optional permissions are pre-approved by the
    // person who triggers the install, so add them to the list.
    if (prompt_->type() == DELEGATED_PERMISSIONS_PROMPT) {
      const PermissionSet& optional_permissions =
          extensions::PermissionsParser::GetOptionalPermissions(
              extension_.get());
      permissions_wrapper = PermissionSet::CreateUnion(*permissions_to_display,
                                                       optional_permissions);
      permissions_to_display = permissions_wrapper.get();
    }
  }

  if (permissions_to_display) {
    Manifest::Type type =
        extension_ ? extension_->GetType() : Manifest::TYPE_UNKNOWN;
    const extensions::PermissionMessageProvider* message_provider =
        extensions::PermissionMessageProvider::Get();

    prompt_->AddPermissions(message_provider->GetPermissionMessages(
        message_provider->GetAllPermissionIDs(*permissions_to_display, type)));
  }

  prompt_->set_extension(extension_.get());
  prompt_->set_icon(gfx::Image::CreateFrom1xBitmap(icon_));

  if (show_params_->WasParentDestroyed()) {
    std::move(done_callback_).Run(Result::ABORTED);
    return;
  }

  g_last_prompt_type_for_tests = prompt_->type();
  did_call_show_dialog_ = true;

  // If true, auto confirm is enabled and already handled the result.
  if (AutoConfirmPromptIfEnabled())
    return;

  if (show_dialog_callback_.is_null())
    show_dialog_callback_ = GetDefaultShowDialogCallback();
  // TODO(https://crbug.com/957713): Use OnceCallback and eliminate the need for
  // a callback on the stack.
  auto cb = std::move(done_callback_);
  std::move(show_dialog_callback_)
      .Run(show_params_.get(), cb, std::move(prompt_));
}

bool ExtensionInstallPrompt::AutoConfirmPromptIfEnabled() {
  switch (extensions::ScopedTestDialogAutoConfirm::GetAutoConfirmValue()) {
    case extensions::ScopedTestDialogAutoConfirm::NONE:
      return false;
    // We use PostTask instead of calling the callback directly here, because in
    // the real implementations it's highly likely the message loop will be
    // pumping a few times before the user clicks accept or cancel.
    case extensions::ScopedTestDialogAutoConfirm::ACCEPT:
    case extensions::ScopedTestDialogAutoConfirm::ACCEPT_AND_OPTION:
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(done_callback_),
                                    ExtensionInstallPrompt::Result::ACCEPTED));
      return true;
    case extensions::ScopedTestDialogAutoConfirm::CANCEL:
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(done_callback_),
                         ExtensionInstallPrompt::Result::USER_CANCELED));
      return true;
  }

  NOTREACHED();
  return false;
}
