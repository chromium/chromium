// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_prompt.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/extension_install_ui.h"
#include "chrome/common/buildflags.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/icons/extension_icon_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/permissions/permission_set.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

using extensions::Extension;
using extensions::Manifest;
using extensions::PermissionMessage;
using extensions::PermissionMessages;
using extensions::PermissionSet;

namespace {

bool AllowWebstoreData(ExtensionInstallPrompt::PromptType type) {
  return type == ExtensionInstallPrompt::EXTERNAL_INSTALL_PROMPT ||
         type == ExtensionInstallPrompt::REPAIR_PROMPT;
}

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

ExtensionInstallPrompt::PromptType
ExtensionInstallPrompt::g_last_prompt_type_for_tests =
    ExtensionInstallPrompt::UNSET_PROMPT_TYPE;

ExtensionInstallPrompt::Prompt::Prompt(PromptType type)
    : type_(type),
      is_requesting_host_permissions_(false),
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

void ExtensionInstallPrompt::Prompt::AddPermissionSet(
    const PermissionSet& permissions) {
  Manifest::Type type =
      extension_ ? extension_->GetType() : Manifest::TYPE_UNKNOWN;
  prompt_permissions_.LoadFromPermissionSet(&permissions, type);
  if (!permissions.effective_hosts().is_empty()) {
    is_requesting_host_permissions_ = true;
  }
}

void ExtensionInstallPrompt::Prompt::AddPermissionMessages(
    const PermissionMessages& permissions) {
  prompt_permissions_.AddPermissionMessages(permissions);
}

void ExtensionInstallPrompt::Prompt::SetWebstoreData(
    const std::string& localized_user_count,
    bool show_user_count,
    double average_rating,
    int rating_count,
    const std::string& localized_rating_count) {
  CHECK(AllowWebstoreData(type_));
  localized_user_count_ = localized_user_count;
  show_user_count_ = show_user_count;
  average_rating_ = average_rating;
  rating_count_ = rating_count;
  localized_rating_count_ = localized_rating_count;
  has_webstore_data_ = true;
}

std::u16string ExtensionInstallPrompt::Prompt::GetDialogTitle() const {
  int id = -1;
  switch (type_) {
    case INSTALL_PROMPT:
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
    case REMOTE_INSTALL_PROMPT:
      id = IDS_EXTENSION_REMOTE_INSTALL_PROMPT_TITLE;
      break;
    case REPAIR_PROMPT:
      id = IDS_EXTENSION_REPAIR_PROMPT_TITLE;
      break;
    case EXTENSION_REQUEST_PROMPT:
      id = IDS_EXTENSION_REQUEST_PROMPT_TITLE;
      break;
    case EXTENSION_PENDING_REQUEST_PROMPT:
      id = IDS_EXTENSION_PENDING_REQUEST_PROMPT_TITLE;
      break;
    case UNSET_PROMPT_TYPE:
    case NUM_PROMPT_TYPES:
      NOTREACHED_IN_MIGRATION();
  }

  return l10n_util::GetStringFUTF16(
      id,
      extensions::util::GetFixupExtensionNameForUIDisplay(extension_->name()));
}

int ExtensionInstallPrompt::Prompt::GetDialogButtons() const {
  // Extension pending request dialog doesn't have confirm button because there
  // is no user action required.
  if (type_ == EXTENSION_PENDING_REQUEST_PROMPT)
    return static_cast<int>(ui::mojom::DialogButton::kCancel);

  return static_cast<int>(ui::mojom::DialogButton::kOk) |
         static_cast<int>(ui::mojom::DialogButton::kCancel);
}

std::u16string ExtensionInstallPrompt::Prompt::GetAcceptButtonLabel() const {
  int id = -1;
  switch (type_) {
    case INSTALL_PROMPT:
      if (requires_parent_permission()) {
        id = IDS_EXTENSION_INSTALL_PROMPT_ASK_A_PARENT_BUTTON;
      } else if (extension_->is_app()) {
        id = IDS_EXTENSION_INSTALL_PROMPT_ACCEPT_BUTTON_APP;
      } else if (extension_->is_theme()) {
        id = IDS_EXTENSION_INSTALL_PROMPT_ACCEPT_BUTTON_THEME;
      } else {
        id = IDS_EXTENSION_INSTALL_PROMPT_ACCEPT_BUTTON_EXTENSION;
      }
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
    case EXTENSION_REQUEST_PROMPT:
      id = IDS_EXTENSION_INSTALL_PROMPT_REQUEST_BUTTON;
      break;
    case EXTENSION_PENDING_REQUEST_PROMPT:
      // Pending request prompt doesn't have accept button.
      break;
    case UNSET_PROMPT_TYPE:
    case NUM_PROMPT_TYPES:
      NOTREACHED_IN_MIGRATION();
  }

  return id != -1 ? l10n_util::GetStringUTF16(id) : std::u16string();
}

std::u16string ExtensionInstallPrompt::Prompt::GetAbortButtonLabel() const {
  int id = -1;
  switch (type_) {
    case INSTALL_PROMPT:
    case RE_ENABLE_PROMPT:
    case REMOTE_INSTALL_PROMPT:
    case REPAIR_PROMPT:
    case EXTENSION_REQUEST_PROMPT:
      id = IDS_CANCEL;
      break;
    case PERMISSIONS_PROMPT:
      id = IDS_EXTENSION_PROMPT_PERMISSIONS_ABORT_BUTTON;
      break;
    case EXTERNAL_INSTALL_PROMPT:
      id = IDS_EXTENSION_EXTERNAL_INSTALL_PROMPT_ABORT_BUTTON;
      break;
    case EXTENSION_PENDING_REQUEST_PROMPT:
      id = IDS_CLOSE;
      break;
    case UNSET_PROMPT_TYPE:
    case NUM_PROMPT_TYPES:
      NOTREACHED_IN_MIGRATION();
  }

  return l10n_util::GetStringUTF16(id);
}

std::u16string ExtensionInstallPrompt::Prompt::GetPermissionsHeading() const {
  int id = -1;
  switch (type_) {
    case INSTALL_PROMPT:
    case EXTERNAL_INSTALL_PROMPT:
    case REMOTE_INSTALL_PROMPT:
    case EXTENSION_REQUEST_PROMPT:
    case EXTENSION_PENDING_REQUEST_PROMPT:
      id = IDS_EXTENSION_PROMPT_WILL_HAVE_ACCESS_TO;
      break;
    case RE_ENABLE_PROMPT:
      id = IDS_EXTENSION_PROMPT_WILL_NOW_HAVE_ACCESS_TO;
      break;
    case PERMISSIONS_PROMPT:
      id = IDS_EXTENSION_PROMPT_WANTS_ACCESS_TO;
      break;
    case REPAIR_PROMPT:
      id = IDS_EXTENSION_PROMPT_CAN_ACCESS;
      break;
    case UNSET_PROMPT_TYPE:
    case NUM_PROMPT_TYPES:
      NOTREACHED_IN_MIGRATION();
  }
  return l10n_util::GetStringUTF16(id);
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

std::u16string ExtensionInstallPrompt::Prompt::GetRatingCount() const {
  CHECK(AllowWebstoreData(type_));
  return l10n_util::GetStringFUTF16(IDS_EXTENSION_RATING_COUNT,
                                    base::UTF8ToUTF16(localized_rating_count_));
}

std::u16string ExtensionInstallPrompt::Prompt::GetUserCount() const {
  CHECK(AllowWebstoreData(type_));

  if (show_user_count_) {
    return l10n_util::GetStringFUTF16(IDS_EXTENSION_USER_COUNT,
                                      base::UTF8ToUTF16(localized_user_count_));
  }
  return std::u16string();
}

size_t ExtensionInstallPrompt::Prompt::GetPermissionCount() const {
  return prompt_permissions_.permissions.size();
}

std::u16string ExtensionInstallPrompt::Prompt::GetPermission(
    size_t index) const {
  CHECK_LT(index, prompt_permissions_.permissions.size());
  return prompt_permissions_.permissions[index];
}

std::u16string ExtensionInstallPrompt::Prompt::GetPermissionsDetails(
    size_t index) const {
  CHECK_LT(index, prompt_permissions_.details.size());
  return prompt_permissions_.details[index];
}

void ExtensionInstallPrompt::Prompt::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ExtensionInstallPrompt::Prompt::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ExtensionInstallPrompt::Prompt::OnDialogOpened() {
  for (Observer& observer : observers_) {
    observer.OnDialogOpened();
  }
}

void ExtensionInstallPrompt::Prompt::OnDialogAccepted() {
  for (Observer& observer : observers_) {
    observer.OnDialogAccepted();
  }
}

void ExtensionInstallPrompt::Prompt::OnDialogCanceled() {
  for (Observer& observer : observers_) {
    observer.OnDialogCanceled();
  }
}

bool ExtensionInstallPrompt::Prompt::ShouldWithheldPermissionsOnDialogAccept()
    const {
  return base::FeatureList::IsEnabled(
             extensions_features::
                 kAllowWithholdingExtensionPermissionsOnInstall) &&
         extensions::util::CanWithholdPermissionsFromExtension(*extension_) &&
         is_requesting_host_permissions_ && type_ == INSTALL_PROMPT;
}

ExtensionInstallPrompt::DoneCallbackPayload::DoneCallbackPayload(Result result)
    : DoneCallbackPayload(result, std::string()) {}

ExtensionInstallPrompt::DoneCallbackPayload::DoneCallbackPayload(
    Result result,
    std::string justification)
    : result(result), justification(std::move(justification)) {}

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
    const base::Value::Dict& manifest,
    int flags,
    const std::string& id,
    const std::string& localized_name,
    const std::string& localized_description,
    std::string* error) {
  std::optional<base::Value::Dict> localized_manifest;
  if (!localized_name.empty() || !localized_description.empty()) {
    localized_manifest = manifest.Clone();
    if (!localized_name.empty()) {
      localized_manifest->Set(extensions::manifest_keys::kName, localized_name);
    }
    if (!localized_description.empty()) {
      localized_manifest->Set(extensions::manifest_keys::kDescription,
                              localized_description);
    }
  }

  return Extension::Create(
      base::FilePath(), extensions::mojom::ManifestLocation::kInternal,
      localized_manifest ? *localized_manifest : manifest, flags, id, error);
}

ExtensionInstallPrompt::ExtensionInstallPrompt(content::WebContents* contents)
    : profile_(contents
                   ? Profile::FromBrowserContext(contents->GetBrowserContext())
                   : nullptr),
      extension_(nullptr),
      install_ui_(std::make_unique<ExtensionInstallUI>(profile_)),
      show_params_(new ExtensionInstallPromptShowParams(contents)),
      did_call_show_dialog_(false) {}

ExtensionInstallPrompt::ExtensionInstallPrompt(Profile* profile,
                                               gfx::NativeWindow native_window)
    : profile_(profile),
      extension_(nullptr),
      install_ui_(std::make_unique<ExtensionInstallUI>(profile_)),
      show_params_(
          new ExtensionInstallPromptShowParams(profile, native_window)),
      did_call_show_dialog_(false) {}

ExtensionInstallPrompt::~ExtensionInstallPrompt() {
}

void ExtensionInstallPrompt::ShowDialog(
    DoneCallback done_callback,
    const Extension* extension,
    const SkBitmap* icon,
    const ShowDialogCallback& show_dialog_callback) {
  ShowDialog(std::move(done_callback), extension, icon,
             std::make_unique<Prompt>(INSTALL_PROMPT), show_dialog_callback);
}

void ExtensionInstallPrompt::ShowDialog(
    DoneCallback done_callback,
    const Extension* extension,
    const SkBitmap* icon,
    std::unique_ptr<Prompt> prompt,
    const ShowDialogCallback& show_dialog_callback) {
  ShowDialog(std::move(done_callback), extension, icon, std::move(prompt),
             nullptr, show_dialog_callback);
}

void ExtensionInstallPrompt::ShowDialog(
    DoneCallback done_callback,
    const Extension* extension,
    const SkBitmap* icon,
    std::unique_ptr<Prompt> prompt,
    std::unique_ptr<const PermissionSet> custom_permissions,
    const ShowDialogCallback& show_dialog_callback) {
  DCHECK(ui_thread_checker_.CalledOnValidThread());
  DCHECK(prompt);
  extension_ = extension;
  done_callback_ = std::move(done_callback);
  if (icon && !icon->empty())
    SetIcon(icon);
  prompt_ = std::move(prompt);
  custom_permissions_ = std::move(custom_permissions);
  show_dialog_callback_ = show_dialog_callback;

  // We special-case themes to not show any confirm UI. Instead they are
  // immediately installed, and then we show an infobar (see OnInstallSuccess)
  // to allow the user to revert if they don't like it.
  if (extension->is_theme() && extension->from_webstore() &&
      prompt_->type() != EXTENSION_REQUEST_PROMPT &&
      prompt_->type() != EXTENSION_PENDING_REQUEST_PROMPT) {
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

std::unique_ptr<ExtensionInstallPrompt::Prompt>
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
      return true;
    }
    case extensions::ScopedTestDialogAutoConfirm::CANCEL: {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(done_callback_),
                         DoneCallbackPayload(
                             ExtensionInstallPrompt::Result::USER_CANCELED)));
      return true;
    }
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}
