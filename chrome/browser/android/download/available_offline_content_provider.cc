// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/download/available_offline_content_provider.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/android/download/download_manager_service.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/offline_items_collection/core/offline_item_state.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "ui/base/l10n/time_format.h"

namespace android {
using chrome::mojom::AvailableContentType;
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
  NOTREACHED();
}

AvailableContentType ContentType(const OfflineItem& item) {
  if (item.is_transient || item.is_off_the_record ||
      item.state != OfflineItemState::COMPLETE || item.is_dangerous) {
    return AvailableContentType::kUninteresting;
  }
  switch (item.filter) {
    case offline_items_collection::FILTER_PAGE:
      if (item.is_suggested)
        return AvailableContentType::kPrefetchedPage;
      return AvailableContentType::kOtherPage;
      break;
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
  // Gets visuals for a list of thumbnails. Calls |complete_callback| with
  // a list of data URIs containing the thumbnails for |content_ids|, in the
  // same order. If no thumbnail is available, the corresponding result
  // string is left empty.
  static void Start(
      offline_items_collection::OfflineContentAggregator* aggregator,
      std::vector<offline_items_collection::ContentId> content_ids,
      base::OnceCallback<void(std::vector<GURL>)> complete_callback) {
    // ThumbnailFetch instances are self-deleting.
    ThumbnailFetch* fetch = new ThumbnailFetch(std::move(content_ids),
                                               std::move(complete_callback));
    fetch->Start(aggregator);
  }

 private:
  ThumbnailFetch(std::vector<offline_items_collection::ContentId> content_ids,
                 base::OnceCallback<void(std::vector<GURL>)> complete_callback)
      : content_ids_(std::move(content_ids)),
        complete_callback_(std::move(complete_callback)) {
    thumbnails_.resize(content_ids_.size());
  }

  void Start(offline_items_collection::OfflineContentAggregator* aggregator) {
    if (content_ids_.empty()) {
      Complete();
      return;
    }
    auto callback = base::BindRepeating(&ThumbnailFetch::VisualsReceived,
                                        base::Unretained(this));
    for (offline_items_collection::ContentId id : content_ids_) {
      aggregator->GetVisualsForItem(id, callback);
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
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(complete_callback_), std::move(thumbnails_)));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](ThumbnailFetch* thumbnail_fetch) { delete thumbnail_fetch; },
            this));
  }

  void AddVisual(
      const offline_items_collection::ContentId& id,
      std::unique_ptr<offline_items_collection::OfflineItemVisuals> visuals) {
    if (!visuals)
      return;
    scoped_refptr<base::RefCountedMemory> data = visuals->icon.As1xPNGBytes();
    if (!data || data->size() == 0)
      return;
    std::string content_base64;
    base::Base64Encode(base::StringPiece(data->front_as<char>(), data->size()),
                       &content_base64);
    for (size_t i = 0; i < content_ids_.size(); ++i) {
      if (content_ids_[i] == id) {
        thumbnails_[i] =
            GURL(base::StrCat({"data:image/png;base64,", content_base64}));
        break;
      }
    }
  }

  // The list of item IDs for which to fetch visuals.
  std::vector<offline_items_collection::ContentId> content_ids_;
  // The thumbnail data URIs to be returned. |thumbnails_| size is equal to
  // |content_ids_| size.
  std::vector<GURL> thumbnails_;
  base::OnceCallback<void(std::vector<GURL>)> complete_callback_;
  size_t callback_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailFetch);
};

chrome::mojom::AvailableOfflineContentPtr CreateAvailableOfflineContent(
    const OfflineItem& item,
    const GURL& thumbnail_url) {
  return chrome::mojom::AvailableOfflineContent::New(
      item.id.id, item.id.name_space, item.title, item.description,
      base::UTF16ToUTF8(ui::TimeFormat::Simple(
          ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_SHORT,
          base::Time::Now() - item.creation_time)),
      "",  // TODO(crbug.com/852872): Add attribution
      std::move(thumbnail_url), ContentType(item));
}
}  // namespace

AvailableOfflineContentProvider::AvailableOfflineContentProvider(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context), weak_ptr_factory_(this) {}

AvailableOfflineContentProvider::~AvailableOfflineContentProvider() = default;

void AvailableOfflineContentProvider::Summarize(SummarizeCallback callback) {
  offline_items_collection::OfflineContentAggregator* aggregator =
      OfflineContentAggregatorFactory::GetForBrowserContext(browser_context_);
  aggregator->GetAllItems(
      base::BindOnce(&AvailableOfflineContentProvider::SummarizeFinalize,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void AvailableOfflineContentProvider::List(ListCallback callback) {
  if (!base::FeatureList::IsEnabled(features::kNewNetErrorPageUI)) {
    std::move(callback).Run({});
    return;
  }
  offline_items_collection::OfflineContentAggregator* aggregator =
      OfflineContentAggregatorFactory::GetForBrowserContext(browser_context_);
  aggregator->GetAllItems(
      base::BindOnce(&AvailableOfflineContentProvider::ListFinalize,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     // aggregator is a keyed service, and is alive as long as
                     // browser_context_, which outlives this.
                     base::Unretained(aggregator)));
}

void AvailableOfflineContentProvider::LaunchItem(
    const std::string& item_id,
    const std::string& name_space) {
  offline_items_collection::OfflineContentAggregator* aggregator =
      OfflineContentAggregatorFactory::GetForBrowserContext(browser_context_);
  aggregator->OpenItem(
      offline_items_collection::LaunchLocation::NET_ERROR_SUGGESTION,
      offline_items_collection::ContentId(name_space, item_id));
}

void AvailableOfflineContentProvider::LaunchDownloadsPage(
    bool open_prefetched_articles_tab) {
  DownloadManagerService::GetInstance()->ShowDownloadManager(
      open_prefetched_articles_tab);
}

void AvailableOfflineContentProvider::Create(
    content::BrowserContext* browser_context,
    chrome::mojom::AvailableOfflineContentProviderRequest request) {
  // Strong bindings remain as long as the pipe is error free. The renderer is
  // on the other side of the pipe, and the browser_context outlives the
  // RenderProcessHost, so the browser_context will outlive the Mojo pipe.
  mojo::MakeStrongBinding(
      std::make_unique<AvailableOfflineContentProvider>(browser_context),
      std::move(request));
}

void AvailableOfflineContentProvider::SummarizeFinalize(
    AvailableOfflineContentProvider::SummarizeCallback callback,
    const std::vector<OfflineItem>& all_items) {
  auto summary = chrome::mojom::AvailableOfflineContentSummary::New();
  summary->total_items = base::saturated_cast<uint32_t>(all_items.size());
  // Decrement the total item count to find the interesting item count.
  size_t interesting_items = all_items.size();
  for (const OfflineItem& item : all_items) {
    switch (ContentType(item)) {
      case AvailableContentType::kPrefetchedPage:
        summary->has_prefetched_page = true;
        break;
      case AvailableContentType::kVideo:
        summary->has_video = true;
        break;
      case AvailableContentType::kAudio:
        summary->has_audio = true;
        break;
      case AvailableContentType::kOtherPage:
        summary->has_offline_page = true;
        break;
      case AvailableContentType::kUninteresting:
        interesting_items--;
        break;
    }
  }

  // If the number of interesting items is lower then the minimum required then
  // reset all summary data so avoid presenting the card.
  if (interesting_items < kMinInterestingItemCount)
    summary = chrome::mojom::AvailableOfflineContentSummary::New();
  std::move(callback).Run(std::move(summary));
}

// Picks the best available offline content items, and passes them to callback.
void AvailableOfflineContentProvider::ListFinalize(
    AvailableOfflineContentProvider::ListCallback callback,
    offline_items_collection::OfflineContentAggregator* aggregator,
    const std::vector<OfflineItem>& all_items) {
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

  auto complete = [](AvailableOfflineContentProvider::ListCallback callback,
                     std::vector<OfflineItem> selected,
                     std::vector<GURL> thumbnail_data_uris) {
    // Translate OfflineItem to AvailableOfflineContentPtr.
    std::vector<chrome::mojom::AvailableOfflineContentPtr> result;
    for (size_t i = 0; i < selected.size(); ++i) {
      result.push_back(
          CreateAvailableOfflineContent(selected[i], thumbnail_data_uris[i]));
    }
    std::move(callback).Run(std::move(result));
  };

  ThumbnailFetch::Start(
      aggregator, selected_ids,
      base::BindOnce(complete, std::move(callback), std::move(selected)));
}

}  // namespace android
