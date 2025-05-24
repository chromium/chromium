// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/sharesheet/nearby_share_action.h"

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/nearby_sharing/attachment.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_resource_getter.h"
#include "chrome/browser/nearby_sharing/file_attachment.h"
#include "chrome/browser/nearby_sharing/nearby_share_settings.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share_dialog_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/cross_device/logging/logging.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "net/base/filename_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/webview.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/nearby_sharing/internal/icons/vector_icons.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace {

base::FilePath ResolveFileUrl(Profile* profile,
                              const apps::IntentFilePtr& file) {
  storage::FileSystemContext* fs_context =
      file_manager::util::GetFileManagerFileSystemContext(profile);
  DCHECK(fs_context);

  // file: type URLs are used by ARC Nearby Share.
  if (file->url.SchemeIsFile()) {
    base::FilePath out;
    net::FileURLToFilePath(file->url, &out);
    return out;
  }

  // filesystem: type URLs, for paths managed by file_manager (e.g. MyFiles).
  DCHECK(file->url.SchemeIsFileSystem());
  const storage::FileSystemURL fs_url =
      fs_context->CrackURLInFirstPartyContext(file->url);
  if (fs_url.is_valid()) {
    return fs_url.path();
  }

  return base::FilePath();
}

std::string GetFirstFilenameFromFileUrls(
    Profile* profile,
    const std::vector<apps::IntentFilePtr>& file_urls) {
  if (file_urls.empty()) {
    return std::string();
  }

  if (file_urls[0]->file_name.has_value()) {
    return file_urls[0]->file_name->path().AsUTF8Unsafe();
  }

  return ResolveFileUrl(profile, file_urls[0]).BaseName().AsUTF8Unsafe();
}

std::vector<std::unique_ptr<Attachment>> CreateTextAttachmentFromIntent(
    Profile* profile,
    const apps::IntentPtr& intent) {
  // TODO(crbug.com/1186730): Detect address and phone number text shares and
  // apply the correct |TextAttachment::Type|.
  TextAttachment::Type type = intent->share_text ? TextAttachment::Type::kText
                                                 : TextAttachment::Type::kUrl;
  std::string title = intent->share_title ? *intent->share_title
                                          : GetFirstFilenameFromFileUrls(
                                                profile, intent->files);

  std::string text;
  if (intent->share_text)
    text = *intent->share_text;
  else if (intent->url)
    text = intent->url->spec();
  else if (intent->drive_share_url)
    text = intent->drive_share_url->spec();

  if (text.empty()) {
    CD_LOG(WARNING, Feature::NS)
        << "Failed to create TextAttachment from sharesheet intent";
    return std::vector<std::unique_ptr<Attachment>>();
  }

  std::vector<std::unique_ptr<Attachment>> attachments;
  attachments.push_back(
      std::make_unique<TextAttachment>(type, text, title, intent->mime_type));
  return attachments;
}

std::vector<std::unique_ptr<Attachment>> CreateFileAttachmentsFromIntent(
    Profile* profile,
    const apps::IntentPtr& intent) {
  std::vector<std::unique_ptr<Attachment>> attachments;

  for (const auto& file : intent->files) {
    base::FilePath file_path = ResolveFileUrl(profile, file);
    if (file_path.empty()) {
      continue;
    }
    if (file->file_name.has_value()) {
      attachments.push_back(std::make_unique<FileAttachment>(
          std::move(file_path), file->file_name->path()));
    } else {
      attachments.push_back(
          std::make_unique<FileAttachment>(std::move(file_path)));
    }
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

NearbyShareAction::NearbyShareAction(Profile* profile) : profile_(profile) {}

NearbyShareAction::~NearbyShareAction() = default;

sharesheet::ShareActionType NearbyShareAction::GetActionType() const {
  return sharesheet::ShareActionType::kNearbyShare;
}

const std::u16string NearbyShareAction::GetActionName() {
  return features::IsNameEnabled()
             ? NearbyShareResourceGetter::GetInstance()
                   ->GetStringWithFeatureName(IDS_NEARBY_SHARE_FEATURE_NAME_PH)
             : l10n_util::GetStringUTF16(IDS_NEARBY_SHARE_FEATURE_NAME);
}

const gfx::VectorIcon& NearbyShareAction::GetActionIcon() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (features::IsNameEnabled()) {
    return kNearbyShareInternalIcon;
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return kNearbyShareIcon;
}

void NearbyShareAction::LaunchAction(
    sharesheet::SharesheetController* controller,
    views::View* root_view,
    apps::IntentPtr intent) {
  gfx::Size size = ComputeSize();
  controller->SetBubbleSize(size.width(), size.height());

  auto view = std::make_unique<views::WebView>(profile_);
  // If this is not done, we don't see anything in our view.
  view->SetPreferredSize(size);
  views::WebView* web_view = root_view->AddChildView(std::move(view));
  // TODO(vecore): Query this from the container view
  web_view->holder()->SetCornerRadii(gfx::RoundedCornersF(kCornerRadius));

  // load chrome://nearby into the webview
  web_view->LoadInitialURL(GURL(chrome::kChromeUINearbyShareURL));

  // Without requesting focus, the sharesheet will launch in an unfocused state
  // which raises accessibility issues with the "Device name" input.
  web_view->RequestFocus();

  auto* webui = web_view->GetWebContents()->GetWebUI();
  DCHECK(webui != nullptr);

  auto* nearby_ui =
      webui->GetController()->GetAs<nearby_share::NearbyShareDialogUI>();
  DCHECK(nearby_ui != nullptr);

  nearby_ui->SetSharesheetController(controller);
  nearby_ui->SetAttachments(
      CreateAttachmentsFromIntent(profile_, std::move(intent)));
  nearby_ui->SetWebView(web_view);
}

bool NearbyShareAction::HasActionView() {
  // Return true so that the Nearby UI is shown after it has been selected.
  return true;
}

bool NearbyShareAction::ShouldShowAction(const apps::IntentPtr& intent,
                                         bool contains_hosted_document) {
  bool valid_file_share = intent && intent->IsShareIntent() &&
                          !intent->files.empty() && !intent->share_text &&
                          !intent->url && !intent->drive_share_url &&
                          !contains_hosted_document;

  bool valid_text_share = intent->action == apps_util::kIntentActionSend &&
                          intent->share_text && intent->files.empty();

  bool valid_url_share = intent->action == apps_util::kIntentActionView &&
                         intent->url && intent->url->is_valid() &&
                         !intent->share_text;

  // Disallow sharing multiple drive files at once. There isn't a valid
  // |drive_share_url| in this case.
  bool valid_drive_share = intent->action == apps_util::kIntentActionSend &&
                           intent->drive_share_url &&
                           intent->drive_share_url->is_valid() &&
                           intent->files.size() == 1u && !intent->share_text;

  return (valid_file_share || valid_text_share || valid_url_share ||
          valid_drive_share) &&
         !IsNearbyShareDisabledByPolicy();
}

bool NearbyShareAction::IsNearbyShareDisabledByPolicy() {
  if (nearby_share_disabled_by_policy_for_testing_.has_value()) {
    return *nearby_share_disabled_by_policy_for_testing_;
  }
  NearbySharingService* nearby_sharing_service =
      NearbySharingServiceFactory::GetForBrowserContext(profile_);
  if (!nearby_sharing_service) {
    return false;
  }
  return nearby_sharing_service->GetSettings()->IsDisabledByPolicy();
}

std::vector<std::unique_ptr<Attachment>>
NearbyShareAction::CreateAttachmentsFromIntent(Profile* profile,
                                               apps::IntentPtr intent) {
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

void NearbyShareAction::SetActionCleanupCallbackForArc(
    base::OnceCallback<void()> callback) {
  if (callback.is_null()) {
    return;
  }
  NearbySharingService* nearby_sharing_service =
      NearbySharingServiceFactory::GetForBrowserContext(profile_);
  if (!nearby_sharing_service) {
    std::move(callback).Run();
    return;
  }
  nearby_sharing_service->SetArcTransferCleanupCallback(std::move(callback));
}
