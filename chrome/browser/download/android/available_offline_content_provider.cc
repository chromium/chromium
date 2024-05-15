// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/available_offline_content_provider.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/download/android/download_open_source.h"
#include "chrome/browser/download/android/download_utils.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_content_provider.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/offline_items_collection/core/offline_item_state.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/base/l10n/time_format.h"

namespace android {
using chrome::mojom::AvailableContentType;
using GetVisualsOptions =
    offline_items_collection::OfflineContentProvider::GetVisualsOptions;
using offline_items_collection::OfflineItem;
using offline_items_collection::OfflineItemState;

namespace {

// Minimum number of interesting offline items required to be available for any
// content card to be presented in the dino page.
const int kMinInterestingItemCount = 4;
// Maximum number of items that should be presented in the list of offline
// items.
const int kMaxListItemsToReturn = 3;
static_assert(
    kMaxListItemsToReturn <= kMinInterestingItemCount,
    "The number of items to list must be less or equal to the minimum number "
    "of items that allow offline content to be presented");

// Returns a value that represents the priority of the content type.
// Smaller priority values are more important.
int ContentTypePriority(AvailableContentType type) {
  switch (type) {
    case AvailableContentType::kPrefetchedPage:
      return 0;
    case AvailableContentType::kVideo:
      return 1;
    case AvailableContentType::kAudio:
      return 2;
    case AvailableContentType::kOtherPage:
      return 3;
    case AvailableContentType::kUninteresting:
      return 10000;
  }
  NOTREACHED_IN_MIGRATION();
}

AvailableContentType ContentType(const OfflineItem& item) {
  // TODO(crbug.com/40111585): Make provider namespace a reusable constant.
  if (item.is_transient || item.is_off_the_record ||
      item.state != OfflineItemState::COMPLETE || item.is_dangerous ||
      item.id.name_space == "content_index") {
    return AvailableContentType::kUninteresting;
  }
  switch (item.filter) {
    case offline_items_collection::FILTER_PAGE:
      if (item.is_suggested)
        return AvailableContentType::kPrefetchedPage;
      return AvailableContentType::kOtherPage;
    case offline_items_collection::FILTER_VIDEO:
      return AvailableContentType::kVideo;
    case offline_items_collection::FILTER_AUDIO:
      return AvailableContentType::kAudio;
    default:
      break;
  }
  return AvailableContentType::kUninteresting;
}

bool CompareItemsByUsefulness(const OfflineItem& a, const OfflineItem& b) {
  int a_priority = ContentTypePriority(ContentType(a));
  int b_priority = ContentTypePriority(ContentType(b));
  if (a_priority != b_priority)
    return a_priority < b_priority;
  // Break a tie by creation_time: more recent first.
  if (a.creation_time != b.creation_time)
    return a.creation_time > b.creation_time;
  // Make sure only one ordering is possible.
  return a.id < b.id;
}

class ThumbnailFetch {
 public:
  struct VisualsDataUris {
    GURL thumbnail;
    GURL favicon;
  };

  ThumbnailFetch(const ThumbnailFetch&) = delete;
  ThumbnailFetch& operator=(const ThumbnailFetch&) = delete;

  // Gets visuals for a list of visuals. Calls |complete_callback| with
  // a list of VisualsDataUris structs containing data URIs for thumbnails and
  // favicons for |content_ids|, in the same order. If no thumbnail or favicon
  // is available, the corresponding result string is left empty.
  static void Start(
      offline_items_collection::OfflineContentAggregator* aggregator,
      std::vector<offline_items_collection::ContentId> content_ids,
      base::OnceCallback<void(std::vector<VisualsDataUris>)>
          complete_callback) {
    // ThumbnailFetch instances are self-deleting.
    ThumbnailFetch* fetch = new ThumbnailFetch(std::move(content_ids),
                                               std::move(complete_callback));
    fetch->Start(aggregator);
  }

 private:
  ThumbnailFetch(
      std::vector<offline_items_collection::ContentId> content_ids,
      base::OnceCallback<void(std::vector<VisualsDataUris>)> complete_callback)
      : content_ids_(std::move(content_ids)),
        complete_callback_(std::move(complete_callback)) {
    visuals_.resize(content_ids_.size());
  }

  void Start(offline_items_collection::OfflineContentAggregator* aggregator) {
    if (content_ids_.empty()) {
      Complete();
      return;
    }
    auto callback = base::BindRepeating(&ThumbnailFetch::VisualsReceived,
                                        base::Unretained(this));
    for (offline_items_collection::ContentId id : content_ids_) {
      aggregator->GetVisualsForItem(
          id, GetVisualsOptions::IconAndCustomFavicon(), callback);
    }
  }

  void VisualsReceived(
      const offline_items_collection::ContentId& id,
      std::unique_ptr<offline_items_collection::OfflineItemVisuals> visuals) {
    DCHECK(callback_count_ < content_ids_.size());
    AddVisual(id, std::move(visuals));
    if (++callback_count_ == content_ids_.size())
      Complete();
  }

  void Complete() {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(complete_callback_), std::move(visuals_)));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](ThumbnailFetch* thumbnail_fetch) { delete thumbnail_fetch; },
            this));
  }

  GURL GetImageAsDataUri(const gfx::Image& image) {
    scoped_refptr<base::RefCountedMemory> data = image.As1xPNGBytes();
    if (!data || data->size() == 0)
      return GURL();
    std::string png_base64 = base::Base64Encode(*data);
    return GURL(base::StrCat({"data:image/png;base64,", png_base64}));
  }

  void AddVisual(
      const offline_items_collection::ContentId& id,
      std::unique_ptr<offline_items_collection::OfflineItemVisuals> visuals) {
    if (!visuals)
      return;

    GURL thumbnail_data_uri = GetImageAsDataUri(visuals->icon);
    GURL favicon_data_uri = GetImageAsDataUri(visuals->custom_favicon);
    for (size_t i = 0; i < content_ids_.size(); ++i) {
      if (content_ids_[i] == id) {
        visuals_[i] = {std::move(thumbnail_data_uri),
                       std::move(favicon_data_uri)};
        break;
      }
    }
  }

  // The list of item IDs for which to fetch visuals.
  std::vector<offline_items_collection::ContentId> content_ids_;
  // The thumbnail and favicon data URIs to be returned. |visuals_| size is
  // equal to |content_ids_| size.
  std::vector<VisualsDataUris> visuals_;
  base::OnceCallback<void(std::vector<VisualsDataUris>)> complete_callback_;
  size_t callback_count_ = 0;
};

chrome::mojom::AvailableOfflineContentPtr CreateAvailableOfflineContent(
    const OfflineItem& item,
    ThumbnailFetch::VisualsDataUris visuals_data_uris) {
  return chrome::mojom::AvailableOfflineContent::New(
      item.id.id, item.id.name_space, item.title, item.description,
      base::UTF16ToUTF8(ui::TimeFormat::Simple(
          ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_SHORT,
          base::Time::Now() - item.creation_time)),
      item.attribution, std::move(visuals_data_uris.thumbnail),
      std::move(visuals_data_uris.favicon), ContentType(item));
}
}  // namespace

AvailableOfflineContentProvider::AvailableOfflineContentProvider(
    int render_process_host_id)
    : render_process_host_id_(render_process_host_id) {}

AvailableOfflineContentProvider::~AvailableOfflineContentProvider() = default;

void AvailableOfflineContentProvider::List(ListCallback callback) {
  Profile* profile = GetProfile();
  if (!profile) {
    CloseSelfOwnedReceiverIfNeeded();
    return;
  }
  offline_items_collection::OfflineContentAggregator* aggregator =
      OfflineContentAggregatorFactory::GetForKey(profile->GetProfileKey());
  aggregator->GetAllItems(
      base::BindOnce(&AvailableOfflineContentProvider::ListFinalize,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     base::Unretained(aggregator)));
}

void AvailableOfflineContentProvider::LaunchItem(
    const std::string& item_id,
    const std::string& name_space) {
  Profile* profile = GetProfile();
  if (!profile)
    return;
  offline_items_collection::OfflineContentAggregator* aggregator =
      OfflineContentAggregatorFactory::GetForKey(profile->GetProfileKey());

  offline_items_collection::OpenParams open_params(
      offline_items_collection::LaunchLocation::NET_ERROR_SUGGESTION);
  open_params.open_in_incognito = profile->IsOffTheRecord();
  aggregator->OpenItem(
      open_params, offline_items_collection::ContentId(name_space, item_id));
}

void AvailableOfflineContentProvider::LaunchDownloadsPage(
    bool open_prefetched_articles_tab) {
  DownloadUtils::ShowDownloadManager(
      open_prefetched_articles_tab,
      DownloadOpenSource::kDinoPageOfflineContent);
}

void AvailableOfflineContentProvider::ListVisibilityChanged(bool is_visible) {
  Profile* profile = GetProfile();
  if (!profile)
    return;
  profile->GetPrefs()->SetBoolean(feed::prefs::kArticlesListVisible,
                                  is_visible);
}

// static
void AvailableOfflineContentProvider::Create(
    int render_process_host_id,
    mojo::PendingReceiver<chrome::mojom::AvailableOfflineContentProvider>
        receiver) {
  // Self owned receivers remain as long as the pipe is error free.
  auto provider_self_owned_receiver = mojo::MakeSelfOwnedReceiver(
      std::make_unique<AvailableOfflineContentProvider>(render_process_host_id),
      std::move(receiver));
  // TODO(curranmax): Rework this code so the static_cast is not needed.
  auto* provider = static_cast<AvailableOfflineContentProvider*>(
      provider_self_owned_receiver->impl());
  provider->SetSelfOwnedReceiver(provider_self_owned_receiver);
}

// Picks the best available offline content items, and passes them to callback.
void AvailableOfflineContentProvider::ListFinalize(
    AvailableOfflineContentProvider::ListCallback callback,
    offline_items_collection::OfflineContentAggregator* aggregator,
    const std::vector<OfflineItem>& all_items) {
  Profile* profile = GetProfile();
  if (!profile) {
    CloseSelfOwnedReceiverIfNeeded();
    return;
  }

  std::vector<OfflineItem> selected(kMinInterestingItemCount);
  const auto end = std::partial_sort_copy(all_items.begin(), all_items.end(),
                                          selected.begin(), selected.end(),
                                          CompareItemsByUsefulness);
  // If the number of interesting items is lower then the minimum don't show any
  // suggestions. Otherwise trim it down to the number of expected items.
  size_t copied_count = end - selected.begin();
  DCHECK(copied_count <= kMinInterestingItemCount);
  if (copied_count < kMinInterestingItemCount ||
      ContentType(selected.back()) == AvailableContentType::kUninteresting) {
    selected.clear();
  } else {
    selected.resize(kMaxListItemsToReturn);
  }

  std::vector<offline_items_collection::ContentId> selected_ids;
  for (const OfflineItem& item : selected)
    selected_ids.push_back(item.id);

  bool list_visible_by_prefs =
      profile->GetPrefs()->GetBoolean(feed::prefs::kArticlesListVisible);

  auto complete =
      [](AvailableOfflineContentProvider::ListCallback callback,
         std::vector<OfflineItem> selected, bool list_visible_by_prefs,
         std::vector<ThumbnailFetch::VisualsDataUris> visuals_data_uris) {
        // Translate OfflineItem to AvailableOfflineContentPtr.
        std::vector<chrome::mojom::AvailableOfflineContentPtr> result;
        for (size_t i = 0; i < selected.size(); ++i) {
          result.push_back(CreateAvailableOfflineContent(
              selected[i], std::move(visuals_data_uris[i])));
        }

        std::move(callback).Run(list_visible_by_prefs, std::move(result));
      };

  ThumbnailFetch::Start(
      aggregator, selected_ids,
      base::BindOnce(complete, std::move(callback), std::move(selected),
                     list_visible_by_prefs));
}

Profile* AvailableOfflineContentProvider::GetProfile() {
  content::RenderProcessHost* render_process_host =
      content::RenderProcessHost::FromID(render_process_host_id_);
  if (!render_process_host)
    return nullptr;
  return Profile::FromBrowserContext(render_process_host->GetBrowserContext());
}

void AvailableOfflineContentProvider::SetSelfOwnedReceiver(
    const mojo::SelfOwnedReceiverRef<
        chrome::mojom::AvailableOfflineContentProvider>&
        provider_self_owned_receiver) {
  provider_self_owned_receiver_ = provider_self_owned_receiver;
}

void AvailableOfflineContentProvider::CloseSelfOwnedReceiverIfNeeded() {
  // Closing the mojo pipe invalidates any pending callbacks, and they should
  // not be used after the receiver is closed.
  if (provider_self_owned_receiver_)
    provider_self_owned_receiver_->Close();
}

}  // namespace android
