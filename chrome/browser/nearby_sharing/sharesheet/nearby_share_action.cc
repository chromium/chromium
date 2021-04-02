// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/sharesheet/nearby_share_action.h"

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/nearby_sharing/attachment.h"
#include "chrome/browser/nearby_sharing/file_attachment.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share_dialog_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/webview.h"
#include "url/gurl.h"

namespace {

std::vector<base::FilePath> ResolveFileUrls(
    Profile* profile,
    const std::vector<GURL>& file_urls) {
  std::vector<base::FilePath> file_paths;
  storage::FileSystemContext* fs_context =
      file_manager::util::GetFileSystemContextForExtensionId(
          profile, file_manager::kFileManagerAppId);
  for (const auto& file_url : file_urls) {
    storage::FileSystemURL fs_url = fs_context->CrackURL(file_url);
    file_paths.push_back(fs_url.path());
  }
  return file_paths;
}

std::string GetFirstFilenameFromFileUrls(
    Profile* profile,
    base::Optional<std::vector<GURL>> file_urls) {
  if (!file_urls) {
    return std::string();
  }

  auto file_paths = ResolveFileUrls(profile, *file_urls);
  return file_paths.size() == 1u ? file_paths[0].BaseName().AsUTF8Unsafe()
                                 : std::string();
}

std::vector<std::unique_ptr<Attachment>> CreateTextAttachmentFromIntent(
    Profile* profile,
    const apps::mojom::IntentPtr& intent) {
  // TODO(crbug.com/1186730): Detect address and phone number text shares and
  // apply the correct |TextAttachment::Type|.
  TextAttachment::Type type = intent->share_text ? TextAttachment::Type::kText
                                                 : TextAttachment::Type::kUrl;
  std::string title = intent->share_title ? *intent->share_title
                                          : GetFirstFilenameFromFileUrls(
                                                profile, intent->file_urls);

  std::string text;
  if (intent->share_text)
    text = *intent->share_text;
  else if (intent->url)
    text = intent->url->spec();
  else if (intent->drive_share_url)
    text = intent->drive_share_url->spec();

  if (text.empty()) {
    NS_LOG(WARNING) << "Failed to create TextAttachment from sharesheet intent";
    return std::vector<std::unique_ptr<Attachment>>();
  }

  std::vector<std::unique_ptr<Attachment>> attachments;
  attachments.push_back(
      std::make_unique<TextAttachment>(type, text, title, intent->mime_type));
  return attachments;
}

std::vector<std::unique_ptr<Attachment>> CreateFileAttachmentsFromIntent(
    Profile* profile,
    const apps::mojom::IntentPtr& intent) {
  std::vector<base::FilePath> file_paths =
      ResolveFileUrls(profile, *intent->file_urls);

  std::vector<std::unique_ptr<Attachment>> attachments;
  for (auto& file_path : file_paths) {
    attachments.push_back(
        std::make_unique<FileAttachment>(std::move(file_path)));
  }
  return attachments;
}

}  // namespace

namespace {

constexpr int kCornerRadius = 12;

gfx::Size ComputeSize() {
  // TODO(vecore): compute expected size based on screen size
  return {/*width=*/512, /*height=*/420};
}

}  // namespace

NearbyShareAction::NearbyShareAction() = default;

NearbyShareAction::~NearbyShareAction() = default;

const std::u16string NearbyShareAction::GetActionName() {
  return l10n_util::GetStringUTF16(IDS_NEARBY_SHARE_FEATURE_NAME);
}

const gfx::VectorIcon& NearbyShareAction::GetActionIcon() {
  return kNearbyShareIcon;
}

void NearbyShareAction::LaunchAction(
    sharesheet::SharesheetController* controller,
    views::View* root_view,
    apps::mojom::IntentPtr intent) {
  gfx::Size size = ComputeSize();
  controller->SetSharesheetSize(size.width(), size.height());

  auto* profile = controller->GetProfile();
  auto view = std::make_unique<views::WebView>(profile);
  // If this is not done, we don't see anything in our view.
  view->SetPreferredSize(size);
  web_view_ = root_view->AddChildView(std::move(view));
  web_view_->GetWebContents()->SetDelegate(this);
  // TODO(vecore): Query this from the container view
  web_view_->holder()->SetCornerRadii(gfx::RoundedCornersF(kCornerRadius));

  // load chrome://nearby into the webview
  web_view_->LoadInitialURL(GURL(chrome::kChromeUINearbyShareURL));

  // Without requesting focus, the sharesheet will launch in an unfocused state
  // which raises accessibility issues with the "Device name" input.
  web_view_->RequestFocus();

  auto* webui = web_view_->GetWebContents()->GetWebUI();
  DCHECK(webui != nullptr);

  auto* nearby_ui =
      webui->GetController()->GetAs<nearby_share::NearbyShareDialogUI>();
  DCHECK(nearby_ui != nullptr);

  nearby_ui->SetSharesheetController(controller);
  nearby_ui->SetAttachments(
      CreateAttachmentsFromIntent(profile, std::move(intent)));
}

bool NearbyShareAction::ShouldShowAction(const apps::mojom::IntentPtr& intent,
                                         bool contains_hosted_document) {
  bool valid_file_share =
      (intent->action == apps_util::kIntentActionSend ||
       intent->action == apps_util::kIntentActionSendMultiple) &&
      intent->file_urls && !intent->file_urls->empty() && !intent->share_text &&
      !intent->url && !intent->drive_share_url && !contains_hosted_document;

  bool valid_text_share = intent->action == apps_util::kIntentActionSend &&
                          intent->share_text && !intent->file_urls;

  bool valid_url_share = intent->action == apps_util::kIntentActionView &&
                         intent->url && intent->url->is_valid() &&
                         !intent->share_text;

  // Disallow sharing multiple drive files at once. There isn't a valid
  // |drive_share_url| in this case.
  bool valid_drive_share =
      intent->action == apps_util::kIntentActionSend &&
      intent->drive_share_url && intent->drive_share_url->is_valid() &&
      intent->file_urls && intent->file_urls->size() == 1u &&
      !intent->share_text;

  return (valid_file_share || valid_text_share || valid_url_share ||
          valid_drive_share) &&
         !IsNearbyShareDisabledByPolicy();
}

bool NearbyShareAction::IsNearbyShareDisabledByPolicy() {
  if (nearby_share_disabled_by_policy_for_testing_.has_value()) {
    return *nearby_share_disabled_by_policy_for_testing_;
  }

  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile) {
    return false;
  }
  NearbySharingService* nearby_share_service =
      NearbySharingServiceFactory::GetForBrowserContext(profile);
  if (!nearby_share_service) {
    return false;
  }
  return nearby_share_service->GetSettings()->IsDisabledByPolicy();
}

std::vector<std::unique_ptr<Attachment>>
NearbyShareAction::CreateAttachmentsFromIntent(Profile* profile,
                                               apps::mojom::IntentPtr intent) {
  if (intent->share_text || intent->url || intent->drive_share_url) {
    return CreateTextAttachmentFromIntent(profile, intent);
  } else {
    // Only create a file attachment if there is no text or URL. Google docs may
    // have associated file paths, but are still treated as text shares.
    return CreateFileAttachmentsFromIntent(profile, intent);
  }
}

bool NearbyShareAction::OnAcceleratorPressed(
    const ui::Accelerator& accelerator) {
  // This is overridden because the default case returns false
  // which means the accelerator has not been handled by the ShareAction. In
  // that case, the sharesheet handles it by closing the UI. We return true
  // instead to indicate we will handle the accelerator ourselves, which
  // prevents the UI from being closed by the sharesheet.
  return true;
}

bool NearbyShareAction::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, web_view_->GetFocusManager());
}

void NearbyShareAction::WebContentsCreated(
    content::WebContents* source_contents,
    int opener_render_process_id,
    int opener_render_frame_id,
    const std::string& frame_name,
    const GURL& target_url,
    content::WebContents* new_contents) {
  chrome::ScopedTabbedBrowserDisplayer displayer(
      Profile::FromBrowserContext(web_view_->GetBrowserContext()));
  NavigateParams nav_params(displayer.browser(), target_url,
                            ui::PageTransition::PAGE_TRANSITION_LINK);
  Navigate(&nav_params);
}
