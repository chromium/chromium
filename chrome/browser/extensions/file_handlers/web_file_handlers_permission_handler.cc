// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/file_handlers/web_file_handlers_permission_handler.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/safe_base_name.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/message_formatter.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/pref_types.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/manifest_handlers/web_file_handlers_info.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"

DEFINE_ELEMENT_IDENTIFIER_VALUE(kWebFileHandlersFileLaunchDialogCheckbox);

namespace extensions {

namespace {

constexpr PrefMap kPrefShouldOpen{"web_file_handlers_should_open",
                                  PrefType::kBool,
                                  PrefScope::kExtensionSpecific};

// Get extension prefs for profile.
ExtensionPrefs* GetExtensionPrefs(Profile* profile) {
  return ExtensionPrefs::Get(profile);
}

// Get dictionary of extension prefs.
std::optional<bool> GetExtensionPrefsAsBoolean(
    ExtensionPrefs* extension_prefs,
    const ExtensionId& extension_id) {
  bool out_value = false;
  if (extension_prefs->ReadPrefAsBoolean(extension_id, kPrefShouldOpen,
                                         &out_value)) {
    return out_value;
  }
  return std::nullopt;
}

// Get extension pref result.
std::optional<bool> GetExtensionPrefsAsBoolean(const Extension& extension,
                                               Profile* profile) {
  return GetExtensionPrefsAsBoolean(GetExtensionPrefs(profile), extension.id());
}

class WebFileHandlersFileLaunchDialogDelegate : public ui::DialogModelDelegate {
 public:
  explicit WebFileHandlersFileLaunchDialogDelegate(
      base::OnceCallback<void(bool, bool)> callback,
      ui::ElementIdentifier checkbox_identifier)
      : callback_(std::move(callback)),
        checkbox_identifier_(checkbox_identifier) {}

  ~WebFileHandlersFileLaunchDialogDelegate() override = default;

  // Determine whether to open the file and maybe remember the decision.
  void OnDialogAccepted() { Run(/*should_open=*/true); }

  void OnDialogClosed() { Run(/*should_open=*/false); }

  // If escape is pressed, neither open the file nor remember the decision.
  void SetActionCloseCallback() {
    std::move(callback_).Run(/*should_open=*/false, /*should_remember=*/false);
  }

 private:
  base::OnceCallback<void(bool, bool)> callback_;

  void Run(bool should_open) {
    auto* checkbox =
        this->dialog_model()->GetCheckboxByUniqueId(checkbox_identifier_);
    bool should_remember = checkbox->is_checked();
    std::move(callback_).Run(should_open, should_remember);
  }

  // The element identifier is declared once by this parent and reused here.
  ui::ElementIdentifier checkbox_identifier_;
};

}  // namespace

// static
bool WebFileHandlersPermissionHandler::remember_selection_ = false;

// Open file using Web File Handlers. Maybe show a file launch dialog first.
WebFileHandlersPermissionHandler::WebFileHandlersPermissionHandler(
    Profile* profile)
    : profile_(profile) {}

WebFileHandlersPermissionHandler::~WebFileHandlersPermissionHandler() = default;

base::AutoReset<bool>
WebFileHandlersPermissionHandler::SetRememberSelectionForTesting(
    bool remember_selection) {
  remember_selection_ = remember_selection;
  return base::AutoReset<bool>(&remember_selection_, remember_selection);
}

void WebFileHandlersPermissionHandler::Confirm(
    const Extension& extension,
    const std::vector<base::SafeBaseName>& base_names,
    CallbackType launch_callback) {
  CHECK(!base_names.empty());

  // Default installed extensions can skip the file launch dialog.
  // TODO(crbug.com/40269541): Remove the allowlist check after development.
  // Also, for development and manual testing purposes, the manifest_features
  // allowlist can also bypass the dialog.
  if (WebFileHandlers::CanBypassPermissionDialog(extension)) {
    std::move(launch_callback).Run(/*should_open=*/true);
    return;
  }

  auto apps_file_handlers = GetAppsFileHandlers(extension);
  const std::vector<std::u16string> file_types =
      web_app::TransformFileExtensionsForDisplay(
          apps::GetFileExtensionsFromFileHandlers(apps_file_handlers));

  // Get saved preferences that represents previous file opening decisions.
  const auto current_prefs = GetExtensionPrefsAsBoolean(extension, profile_);
  if (current_prefs.has_value()) {
    std::move(launch_callback).Run(current_prefs.value());
    return;
  }

  // Maybe open the file. Maybe remember the decision to open the file.
  auto callback_after_dialog =
      base::BindOnce(&WebFileHandlersPermissionHandler::CallbackAfterDialog,
                     weak_factory_.GetWeakPtr(), extension.id(), file_types,
                     std::move(launch_callback));

  // Present a contextual file launch dialog.
  ShowFileLaunchDialog(std::move(base_names), file_types,
                       std::move(callback_after_dialog));
}

// static
void WebFileHandlersPermissionHandler::ShowFileLaunchDialog(
    const std::vector<base::SafeBaseName>& base_names,
    const std::vector<std::u16string>& file_types,
    base::OnceCallback<void(bool, bool)> callback) {
  auto checkbox_id =
      ui::ElementIdentifier(kWebFileHandlersFileLaunchDialogCheckbox);

  auto bubble_delegate_unique =
      std::make_unique<WebFileHandlersFileLaunchDialogDelegate>(
          std::move(callback), checkbox_id);
  WebFileHandlersFileLaunchDialogDelegate* bubble_delegate =
      bubble_delegate_unique.get();

  std::u16string title = base::i18n::MessageFormatter::FormatWithNamedArgs(
      l10n_util::GetStringUTF16(IDS_EXTENSIONS_WFH_PERMISSION_HANDLER_FILES),
      "FILE_COUNT", static_cast<int>(base_names.size()), "FILE1",
      base_names[0].path().value());

  // Prepare every file extension for display in the checkbox.
  const auto file_types_for_display = base::JoinString(
      file_types,
      l10n_util::GetStringUTF16(IDS_WEB_APP_FILE_HANDLING_LIST_SEPARATOR));
  ui::DialogModelLabel checkbox_label =
      ui::DialogModelLabel(base::i18n::MessageFormatter::FormatWithNamedArgs(
          l10n_util::GetStringUTF16(
              IDS_WEB_APP_FILE_HANDLING_DIALOG_STICKY_CHOICE),
          "FILE_TYPE_COUNT", static_cast<int>(file_types.size()), "FILE_TYPES",
          file_types_for_display));

  // Prepare checkbox for testing.
  ui::DialogModelCheckbox::Params checkbox_params;
  checkbox_params.SetIsChecked(remember_selection_);
  checkbox_params.SetVisible(true);

  // TODO(crbug.com/40269541): Add extension name and icon. Show files. Design:
  // https://docs.google.com/document/d/1h7ZjryB2zYEjUG9DqPLzAM1iSUXr8ZadUUY02ycExAQ
  std::unique_ptr<ui::DialogModel> dialog_model =
      ui::DialogModel::Builder(std::move(bubble_delegate_unique))
          .SetInternalName("WebFileHandlersFileLaunchDialogView")
          .SetTitle(title)
          .AddCheckbox(checkbox_id, checkbox_label, checkbox_params)
          .AddOkButton(
              base::BindOnce(
                  &WebFileHandlersFileLaunchDialogDelegate::OnDialogAccepted,
                  base::Unretained(bubble_delegate)),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_WEB_APP_FILE_HANDLING_POSITIVE_BUTTON)))
          .AddCancelButton(
              base::BindOnce(
                  &WebFileHandlersFileLaunchDialogDelegate::OnDialogClosed,
                  base::Unretained(bubble_delegate)),
              ui::DialogModel::Button::Params().SetLabel(
                  l10n_util::GetStringUTF16(
                      IDS_WEB_APP_FILE_HANDLING_NEGATIVE_BUTTON)))
          .SetCloseActionCallback(base::BindOnce(
              &WebFileHandlersFileLaunchDialogDelegate::SetActionCloseCallback,
              base::Unretained(bubble_delegate)))
          .Build();

  std::unique_ptr<views::BubbleDialogModelHost> dialog =
      views::BubbleDialogModelHost::CreateModal(std::move(dialog_model),
                                                ui::mojom::ModalType::kWindow);
  dialog->SetOwnedByWidget(views::WidgetDelegate::OwnedByWidgetPassKey());
  views::Widget* modal_dialog = views::DialogDelegate::CreateDialogWidget(
      std::move(dialog), /*context=*/nullptr, /*parent=*/nullptr);
  modal_dialog->Show();
}

void WebFileHandlersPermissionHandler::CallbackAfterDialog(
    const ExtensionId& extension_id,
    const std::vector<std::u16string>& file_types,
    CallbackType launch_callback,
    bool should_open,
    bool should_remember) {
  // Maybe remember the decision to open the file.
  if (should_remember) {
    auto* extension_prefs = GetExtensionPrefs(profile_);
    extension_prefs->SetBooleanPref(extension_id, kPrefShouldOpen, should_open);
  }

  // Maybe open the file.
  std::move(launch_callback).Run(should_open);
}

// static
const apps::FileHandlers WebFileHandlersPermissionHandler::GetAppsFileHandlers(
    const Extension& extension) {
  apps::FileHandlers web_file_handlers;
  auto* file_handlers = WebFileHandlers::GetFileHandlers(extension);

  if (!file_handlers) {
    return web_file_handlers;
  }

  for (const auto& web_file_handler : *file_handlers) {
    apps::FileHandler file_handler;
    file_handler.action = GURL(web_file_handler.file_handler.action);
    file_handler.display_name =
        base::UTF8ToUTF16(web_file_handler.file_handler.name);

    // Compute `accept`, which contains all mime types and file extensions.
    apps::FileHandler::Accept accept;
    for (const auto [mime_type, file_extension_list] :
         web_file_handler.file_handler.accept.additional_properties) {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = mime_type;
      base::flat_set<std::string> file_extensions;
      for (const auto& file_extension : file_extension_list.GetList()) {
        file_extensions.insert(file_extension.GetString());
      }
      accept_entry.file_extensions = file_extensions;
      accept.emplace_back(accept_entry);
    }
    file_handler.accept = accept;

    // The default launch type is single client.
    file_handler.launch_type =
        web_file_handler.launch_type ==
                WebFileHandler::LaunchType::kSingleClient
            ? apps::FileHandler::LaunchType::kSingleClient
            : apps::FileHandler::LaunchType::kMultipleClients;

    web_file_handlers.emplace_back(file_handler);
  }

  return web_file_handlers;
}

}  // namespace extensions
