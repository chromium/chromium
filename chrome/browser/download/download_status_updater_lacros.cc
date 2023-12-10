// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_status_updater.h"

#include <memory>
#include <string>

#include "base/containers/contains.h"
#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/download/bubble/download_bubble_ui_controller.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chromeos/crosapi/mojom/download_controller.mojom.h"
#include "chromeos/crosapi/mojom/download_status_updater.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/download/content/public/all_download_item_notifier.h"
#include "components/download/public/common/download_item_utils.h"
#include "content/public/browser/download_item_utils.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/display/types/display_constants.h"

namespace {

// Helpers ---------------------------------------------------------------------

crosapi::mojom::DownloadStatusUpdater* GetRemote(
    absl::optional<uint32_t> min_version = absl::nullopt) {
  using DownloadStatusUpdater = crosapi::mojom::DownloadStatusUpdater;
  auto* service = chromeos::LacrosService::Get();
  if (!service || !service->IsAvailable<DownloadStatusUpdater>()) {
    return nullptr;
  }
  // NOTE: Use `remote.version()` rather than `service->GetInterfaceVersion()`
  // as the latter does not respect versions of remotes injected for testing.
  auto& remote = service->GetRemote<DownloadStatusUpdater>();
  return remote.version() >= min_version.value_or(remote.version())
             ? remote.get()
             : nullptr;
}

bool IsCommandEnabled(DownloadItemModel& model,
                      DownloadCommands::Command command) {
  // To support other commands, we may need to update checks below to also
  // inspect `BubbleUIInfo` subpage buttons.
  CHECK(command == DownloadCommands::CANCEL ||
        command == DownloadCommands::PAUSE ||
        command == DownloadCommands::RESUME);

  const DownloadUIModel::BubbleUIInfo info = model.GetBubbleUIInfo();

  // A command is enabled if `BubbleUIInfo` contains a quick action for it. This
  // is preferred over non-`BubbleUIInfo`-based determination of command
  // enablement as it takes more signals into account, e.g. if the download has
  // been marked dangerous.
  return base::Contains(info.quick_actions, command,
                        &DownloadUIModel::BubbleUIInfo::QuickAction::command);
}

crosapi::mojom::DownloadStatusPtr ConvertToMojoDownloadStatus(
    download::DownloadItem* download) {
  DownloadItemModel model(download);
  auto status = crosapi::mojom::DownloadStatus::New();
  status->guid = download->GetGuid();
  status->state = download::download_item_utils::ConvertToMojoDownloadState(
      download->GetState());
  status->received_bytes = download->GetReceivedBytes();
  status->total_bytes = download->GetTotalBytes();
  status->target_file_path = download->GetTargetFilePath();
  status->full_path = download->GetFullPath();
  status->cancellable = IsCommandEnabled(model, DownloadCommands::CANCEL);
  status->pausable = IsCommandEnabled(model, DownloadCommands::PAUSE);
  status->resumable = IsCommandEnabled(model, DownloadCommands::RESUME);
  return status;
}

}  // namespace

// DownloadStatusUpdater::Delegate ---------------------------------------------

// The delegate of the `DownloadStatusUpdater` in Lacros Chrome which serves as
// the client for the `DownloadStatusUpdater` in Ash Chrome.
class DownloadStatusUpdater::Delegate
    : public crosapi::mojom::DownloadStatusUpdaterClient {
 public:
  using GetDownloadItemCallback =
      base::RepeatingCallback<download::DownloadItem*(const std::string&)>;

  explicit Delegate(GetDownloadItemCallback get_download_item_callback)
      : get_download_item_callback_(std::move(get_download_item_callback)) {
    CHECK(!get_download_item_callback_.is_null());
    using crosapi::mojom::DownloadStatusUpdater;
    if (auto* remote =
            GetRemote(DownloadStatusUpdater::kBindClientMinVersion)) {
      remote->BindClient(receiver_.BindNewPipeAndPassRemoteWithVersion());
    }
  }

  Delegate(const Delegate&) = delete;
  Delegate& operator=(const Delegate&) = delete;
  ~Delegate() override = default;

 private:
  download::DownloadItem* GetDownloadItem(const std::string& guid) {
    return get_download_item_callback_.Run(guid);
  }

  // crosapi::mojom::DownloadStatusUpdaterClient:
  void Cancel(const std::string& guid, CancelCallback callback) override {
    bool handled = false;
    if (download::DownloadItem* item = GetDownloadItem(guid); item) {
      handled = true;
      item->Cancel(/*user_cancel=*/true);
    }
    std::move(callback).Run(handled);
  }

  void Pause(const std::string& guid, PauseCallback callback) override {
    bool handled = false;
    if (download::DownloadItem* item = GetDownloadItem(guid); item) {
      handled = true;
      if (!item->IsPaused()) {
        item->Pause();
      }
    }
    std::move(callback).Run(handled);
  }

  void Resume(const std::string& guid, ResumeCallback callback) override {
    bool handled = false;
    if (download::DownloadItem* item = GetDownloadItem(guid); item) {
      handled = true;
      if (item->CanResume()) {
        item->Resume(/*user_resume=*/true);
      }
    }
    std::move(callback).Run(handled);
  }

  void ShowInBrowser(const std::string& guid,
                     ShowInBrowserCallback callback) override {
    // Look up the profile from the download item and find a relevant browser to
    // display the download bubble in.
    Profile* profile = nullptr;
    Browser* browser = nullptr;
    if (download::DownloadItem* item = GetDownloadItem(guid); item) {
      content::BrowserContext* browser_context =
          content::DownloadItemUtils::GetBrowserContext(item);
      profile = Profile::FromBrowserContext(browser_context);
      if (profile) {
        // TODO(chlily): This doesn't work for web app initiated downloads.
        browser = chrome::FindTabbedBrowser(profile,
                                            /*match_original_profiles=*/false,
                                            display::kInvalidDisplayId,
                                            /*ignore_closing_browsers=*/true);
      }
    }

    if (browser) {
      // If we found an appropriate browser, show the download bubble in it.
      OnBrowserLocated(guid, std::move(callback), browser);
      return;
    } else if (profile) {
      // Otherwise, attempt to open a new browser window and do the same.
      // This can happen if the last browser window shuts down while there are
      // downloads in progress, and the profile is kept alive. (Some downloads
      // do not block browser shutdown.)
      profiles::OpenBrowserWindowForProfile(
          base::BindOnce(&DownloadStatusUpdater::Delegate::OnBrowserLocated,
                         weak_factory_.GetWeakPtr(), guid, std::move(callback)),
          /*always_create=*/false,
          /*is_new_profile=*/false, /*unblock_extensions=*/true, profile);
      return;
    }
    std::move(callback).Run(/*handled=*/false);
  }

  void OnBrowserLocated(const std::string& guid,
                        ShowInBrowserCallback callback,
                        Browser* browser) {
    if (!browser || !browser->window()) {
      std::move(callback).Run(/*handled=*/false);
      return;
    }

    // Activate the browser so that the bubble or chrome://downloads page can be
    // visible.
    if (browser->window()->IsMinimized()) {
      browser->window()->Restore();
    }
    browser->window()->Activate();

    bool showed_bubble = false;
    DownloadBubbleUIController* bubble_controller =
        browser->window()->GetDownloadBubbleUIController();
    // Look up the guid again because the item may have been removed in the
    // meantime.
    if (download::DownloadItem* item = GetDownloadItem(guid);
        item && bubble_controller) {
      offline_items_collection::ContentId content_id =
          OfflineItemUtils::GetContentIdForDownload(item);
      showed_bubble = bubble_controller->OpenMostSpecificDialog(content_id);
    }
    if (!showed_bubble) {
      // Fall back to showing chrome://downloads.
      chrome::ShowDownloads(browser);
    }
    std::move(callback).Run(/*handled=*/true);
  }

  // The receiver bound to `this` for use by crosapi.
  mojo::Receiver<crosapi::mojom::DownloadStatusUpdaterClient> receiver_{this};

  // Callback allowing the lookup of DownloadItem*s from guids.
  GetDownloadItemCallback get_download_item_callback_;

  base::WeakPtrFactory<DownloadStatusUpdater::Delegate> weak_factory_{this};
};

// DownloadStatusUpdater -------------------------------------------------------

DownloadStatusUpdater::DownloadStatusUpdater()
    : delegate_(std::make_unique<Delegate>(
          base::BindRepeating(&DownloadStatusUpdater::GetDownloadItemFromGuid,
                              base::Unretained(this)))) {}

DownloadStatusUpdater::~DownloadStatusUpdater() = default;

void DownloadStatusUpdater::UpdateAppIconDownloadProgress(
    download::DownloadItem* download) {
  if (auto* remote = GetRemote()) {
    remote->Update(ConvertToMojoDownloadStatus(download));
  }
}

download::DownloadItem* DownloadStatusUpdater::GetDownloadItemFromGuid(
    const std::string& guid) {
  for (const auto& notifier : notifiers_) {
    content::DownloadManager* manager = notifier->GetManager();
    if (!manager) {
      continue;
    }
    download::DownloadItem* item = manager->GetDownloadByGuid(guid);
    if (item) {
      return item;
    }
  }
  return nullptr;
}
