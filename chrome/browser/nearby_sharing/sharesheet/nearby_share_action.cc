// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/sharesheet/nearby_share_action.h"

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/nearby_sharing/attachment.h"
#include "chrome/browser/nearby_sharing/file_attachment.h"
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

std::vector<std::unique_ptr<Attachment>> CreateAttachmentsFromIntent(
    Profile* profile,
    apps::mojom::IntentPtr intent) {
  DCHECK(intent->file_urls);
  std::vector<std::unique_ptr<Attachment>> attachments;
  std::vector<base::FilePath> file_paths =
      ResolveFileUrls(profile, *intent->file_urls);
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

const base::string16 NearbyShareAction::GetActionName() {
  return l10n_util::GetStringUTF16(IDS_NEARBY_SHARE_FEATURE_NAME);
}

const gfx::VectorIcon& NearbyShareAction::GetActionIcon() {
  return kNearbyShareIcon;
}

void NearbyShareAction::LaunchAction(
    sharesheet::SharesheetController* controller,
    views::View* root_view,
    apps::mojom::IntentPtr intent) {
  // Store so we can trigger the share sheet close later.
  controller_ = controller;

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

  nearby_ui_ =
      webui->GetController()->GetAs<nearby_share::NearbyShareDialogUI>();
  DCHECK(nearby_ui_ != nullptr);

  nearby_ui_->AddObserver(this);
  nearby_ui_->SetAttachments(
      CreateAttachmentsFromIntent(profile, std::move(intent)));
}

void NearbyShareAction::OnClose() {
  // The nearby WebUI requested to close through user action
  if (controller_) {
    controller_->CloseSharesheet();

    // We need to clear out the controller here to protect against calling
    // CloseShareSheet() more than once, which will cause a crash.
    controller_ = nullptr;
  }
}

bool NearbyShareAction::ShouldShowAction(const apps::mojom::IntentPtr& intent,
                                         bool contains_hosted_document) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile) {
    return false;
  }
  NearbySharingService* nearby_share_service =
      NearbySharingServiceFactory::GetForBrowserContext(profile);
  if (!nearby_share_service) {
    return false;
  }
  return !nearby_share_service->GetSettings()->IsDisabledByPolicy();
}

void NearbyShareAction::OnClosing(
    sharesheet::SharesheetController* controller) {
  if (nearby_ui_) {
    nearby_ui_->RemoveObserver(this);
    nearby_ui_ = nullptr;
  }
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
