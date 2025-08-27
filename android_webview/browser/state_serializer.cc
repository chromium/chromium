// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/state_serializer.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/pickle.h"
#include "base/strings/string_view_util.h"
#include "base/time/time.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_entry_restore_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "third_party/blink/public/common/page_state/page_state.h"

// Reasons for not re-using TabNavigation under chrome/ as of 20121116:
// * Android WebView has different requirements for fields to store since
//   we are the only ones using values like BaseURLForDataURL.
// * TabNavigation does unnecessary copying of data, which in Android
//   WebView case, is undesired since save/restore is called in Android
//   very frequently.
// * TabNavigation is tightly integrated with the rest of chrome session
//   restore and sync code, and has other purpose in addition to serializing
//   NavigationEntry.

using std::string;

namespace android_webview {

namespace {

const uint32_t AW_STATE_VERSION = internal::AW_STATE_VERSION_MOST_RECENT_FIRST;

// The production implementation of NavigationHistory and NavigationHistorySink,
// backed by a NavigationController.
// This class could be split into two independent parts, one for each interface.
class NavigationControllerWrapper : public internal::NavigationHistory,
                                    public internal::NavigationHistorySink {
 public:
  explicit NavigationControllerWrapper(
      content::NavigationController* controller)
      : controller_(controller) {}

  int GetEntryCount() override { return controller_->GetEntryCount(); }

  int GetCurrentEntry() override {
    return controller_->GetLastCommittedEntryIndex();
  }

  content::NavigationEntry* GetEntryAtIndex(int index) override {
    return controller_->GetEntryAtIndex(index);
  }

  void Restore(int selected_entry,
               std::vector<std::unique_ptr<content::NavigationEntry>>* entries)
      override {
    controller_->Restore(selected_entry, content::RestoreType::kRestored,
                         entries);
    controller_->LoadIfNecessary();
  }

 private:
  const raw_ptr<content::NavigationController> controller_;
};

bool RestoreFromPickleLegacy_VersionDataUrl(
    uint32_t state_version,
    base::PickleIterator* iterator,
    internal::NavigationHistorySink& sink) {
  int entry_count = -1;
  int selected_entry = -2;  // -1 is a valid value

  if (!iterator->ReadInt(&entry_count)) {
    return false;
  }

  if (!iterator->ReadInt(&selected_entry)) {
    return false;
  }

  if (entry_count < 0) {
    return false;
  }
  if (selected_entry < -1) {
    return false;
  }
  if (selected_entry >= entry_count) {
    return false;
  }

  std::unique_ptr<content::NavigationEntryRestoreContext> context =
      content::NavigationEntryRestoreContext::Create();
  std::vector<std::unique_ptr<content::NavigationEntry>> entries;
  entries.reserve(entry_count);
  for (int i = 0; i < entry_count; ++i) {
    entries.push_back(content::NavigationEntry::Create());
    if (!internal::RestoreNavigationEntryFromPickle(
            state_version, iterator, entries[i].get(), context.get())) {
      return false;
    }
  }

  // |web_contents| takes ownership of these entries after this call.
  sink.Restore(selected_entry, &entries);
  DCHECK_EQ(0u, entries.size());

  return true;
}

}  // namespace

std::optional<base::Pickle> WriteToPickle(content::WebContents& web_contents,
                                          size_t max_size,
                                          bool include_forward_state) {
  NavigationControllerWrapper wrapper(&web_contents.GetController());
  return internal::WriteToPickle(wrapper, max_size, include_forward_state);
}

bool RestoreFromPickle(base::PickleIterator* iterator,
                       content::WebContents* web_contents) {
  DCHECK(web_contents);
  NavigationControllerWrapper wrapper(&web_contents->GetController());
  return RestoreFromPickle(iterator, wrapper);
}

namespace internal {

std::optional<base::Pickle> WriteToPickle(NavigationHistory& history,
                                          size_t max_size,
                                          bool save_forward_history) {
  base::Pickle pickle;

  internal::WriteHeaderToPickle(AW_STATE_VERSION, &pickle);

  const int entry_count = history.GetEntryCount();
  const int selected_entry = history.GetCurrentEntry();
  // A NavigationEntry will always exist, so there will always be at least 1
  // entry.
  DCHECK_GE(entry_count, 1);
  DCHECK_GE(selected_entry, 0);
  DCHECK_LT(selected_entry, entry_count);

  // Navigations are stored in reverse order, allowing us to prioritise the more
  // recent history entries and stop writing once we exceed the size limit. To
  // know the size of a navigation entry we've got to serialize it, so to avoid
  // doing unnecessary work we write the entry, then check if we've exceeded the
  // limit
  bool selected_entry_was_saved = false;

  int start_entry = save_forward_history ? entry_count - 1 : selected_entry;
  for (int i = start_entry; i >= 0; --i) {
    // Note the difference between |payload_size|, used here and |size| used in
    // the conditional below. |size| gives the total size of the Pickle, so is
    // relevant for the size limit, |payload_size| gives the size of the data
    // we've written (without the Pickle's internal header), so is relevant when
    // we're copying the payload.
    size_t payload_size_before_adding_entry = pickle.payload_size();

    pickle.WriteBool(i == selected_entry);
    internal::WriteNavigationEntryToPickle(*history.GetEntryAtIndex(i),
                                           &pickle);

    if (pickle.size() > max_size) {
      if (i == start_entry) {
        // If not even a single entry can fit into the max size, return nullopt.
        return std::nullopt;
      }

      // This should happen rarely, but it's possible that the selected entry
      // was far enough back in history that it was cut off. In this case, rerun
      // with save_forward_history = false, ensuring that the current entry is
      // the first one written.
      if (!selected_entry_was_saved) {
        return WriteToPickle(history, max_size,
                             /* save_forward_history= */ false);
      }

      base::Pickle new_pickle;
      new_pickle.WriteBytes(pickle.payload_bytes().subspan(
          (size_t)0, payload_size_before_adding_entry));
      return new_pickle;
    }

    if (i == selected_entry) {
      selected_entry_was_saved = true;
    }
  }

  // Please update AW_STATE_VERSION and IsSupportedVersion() if serialization
  // format is changed.
  // Make sure the serialization format is updated in a backwards compatible
  // way.

  return pickle;
}

void WriteHeaderToPickle(base::Pickle* pickle) {
  WriteHeaderToPickle(AW_STATE_VERSION, pickle);
}

void WriteHeaderToPickle(uint32_t state_version, base::Pickle* pickle) {
  pickle->WriteUInt32(state_version);
}

uint32_t RestoreHeaderFromPickle(base::PickleIterator* iterator) {
  uint32_t state_version = -1;
  if (!iterator->ReadUInt32(&state_version)) {
    return 0;
  }

  if (IsSupportedVersion(state_version)) {
    return state_version;
  }

  return 0;
}

bool IsSupportedVersion(uint32_t state_version) {
  return state_version == internal::AW_STATE_VERSION_INITIAL ||
         state_version == internal::AW_STATE_VERSION_DATA_URL ||
         state_version == internal::AW_STATE_VERSION_MOST_RECENT_FIRST;
}

void WriteNavigationEntryToPickle(content::NavigationEntry& entry,
                                  base::Pickle* pickle) {
  WriteNavigationEntryToPickle(AW_STATE_VERSION, entry, pickle);
}

void WriteNavigationEntryToPickle(uint32_t state_version,
                                  content::NavigationEntry& entry,
                                  base::Pickle* pickle) {
  DCHECK(IsSupportedVersion(state_version));
  pickle->WriteString(entry.GetURL().spec());
  pickle->WriteString(entry.GetVirtualURL().spec());

  const content::Referrer& referrer = entry.GetReferrer();
  pickle->WriteString(referrer.url.spec());
  pickle->WriteInt(static_cast<int>(referrer.policy));

  pickle->WriteString16(entry.GetTitle());
  pickle->WriteString(entry.GetPageState().ToEncodedData());
  pickle->WriteBool(static_cast<int>(entry.GetHasPostData()));
  pickle->WriteString(entry.GetOriginalRequestURL().spec());
  pickle->WriteString(entry.GetBaseURLForDataURL().spec());

  if (state_version >= internal::AW_STATE_VERSION_DATA_URL) {
    std::string_view view;
    const scoped_refptr<const base::RefCountedString>& s =
        entry.GetDataURLAsString();
    if (s) {
      view = base::as_string_view(*s);
    }
    // Even when |entry.GetDataForDataURL()| is null we still need to write a
    // zero-length entry to ensure the fields all line up when read back in.
    pickle->WriteData(view.data(), view.size());
  }

  pickle->WriteBool(static_cast<int>(entry.GetIsOverridingUserAgent()));
  pickle->WriteInt64(entry.GetTimestamp().ToInternalValue());
  pickle->WriteInt(entry.GetHttpStatusCode());

  // Please update AW_STATE_VERSION and IsSupportedVersion() if serialization
  // format is changed.
  // Make sure the serialization format is updated in a backwards compatible
  // way.
}

bool RestoreFromPickle(base::PickleIterator* iterator,
                       NavigationHistorySink& sink) {
  DCHECK(iterator);

  uint32_t state_version = internal::RestoreHeaderFromPickle(iterator);
  if (!state_version) {
    return false;
  }

  if (state_version < AW_STATE_VERSION_MOST_RECENT_FIRST) {
    return RestoreFromPickleLegacy_VersionDataUrl(state_version, iterator,
                                                  sink);
  }

  std::unique_ptr<content::NavigationEntryRestoreContext> context =
      content::NavigationEntryRestoreContext::Create();
  std::vector<std::unique_ptr<content::NavigationEntry>> entries;

  std::optional<int> selected_entry;

  while (!iterator->ReachedEnd()) {
    bool selected = false;
    if (!iterator->ReadBool(&selected)) {
      return false;
    }

    entries.push_back(content::NavigationEntry::Create());

    if (!internal::RestoreNavigationEntryFromPickle(
            state_version, iterator, entries.back().get(), context.get())) {
      return false;
    }

    if (selected) {
      selected_entry = entries.size() - 1;
    }
  }

  if (!selected_entry.has_value()) {
    return false;
  }

  // The list was stored in reverse order, so flip it back (and update selected
  // index).
  std::ranges::reverse(entries);
  selected_entry = entries.size() - selected_entry.value() - 1;

  sink.Restore(selected_entry.value(), &entries);
  return true;
}

bool RestoreNavigationEntryFromPickle(
    base::PickleIterator* iterator,
    content::NavigationEntry* entry,
    content::NavigationEntryRestoreContext* context) {
  return RestoreNavigationEntryFromPickle(AW_STATE_VERSION, iterator, entry,
                                          context);
}

bool RestoreNavigationEntryFromPickle(
    uint32_t state_version,
    base::PickleIterator* iterator,
    content::NavigationEntry* entry,
    content::NavigationEntryRestoreContext* context) {
  DCHECK(IsSupportedVersion(state_version));
  DCHECK(iterator);
  DCHECK(entry);
  DCHECK(context);

  GURL deserialized_url;
  {
    string url;
    if (!iterator->ReadString(&url))
      return false;
    deserialized_url = GURL(url);

    // Note: The url will be cloberred by the SetPageState call below (see how
    // RecursivelyGenerateFrameEntries uses PageState data to create
    // FrameNavigationEntries).
    //
    // Nevertheless, we call SetURL here to temporarily set the URL, because it
    // modifies the state that might be depended on in some calls below (e.g.
    // the SetVirtualURL call).
    entry->SetURL(deserialized_url);
  }

  {
    string virtual_url;
    if (!iterator->ReadString(&virtual_url))
      return false;
    entry->SetVirtualURL(GURL(virtual_url));
  }

  content::Referrer deserialized_referrer;
  {
    // Note: The referrer will be cloberred by the SetPageState call below (see
    // how RecursivelyGenerateFrameEntries uses PageState data to create
    // FrameNavigationEntries).
    string referrer_url;
    int policy;

    if (!iterator->ReadString(&referrer_url))
      return false;
    if (!iterator->ReadInt(&policy))
      return false;

    deserialized_referrer.url = GURL(referrer_url);
    deserialized_referrer.policy = content::Referrer::ConvertToPolicy(policy);
  }

  {
    std::u16string title;
    if (!iterator->ReadString16(&title))
      return false;
    entry->SetTitle(std::move(title));
  }

  {
    string content_state;
    if (!iterator->ReadString(&content_state))
      return false;

    // In legacy output of WebViewProvider.saveState, the |content_state|
    // might be empty - we need to gracefully handle such data when
    // it is deserialized via WebViewProvider.restoreState.
    if (content_state.empty()) {
      // Ensure that the deserialized/restored content::NavigationEntry (and
      // the content::FrameNavigationEntry underneath) has a valid PageState.
      entry->SetPageState(blink::PageState::CreateFromURL(deserialized_url),
                          context);

      // The |deserialized_referrer| might be inconsistent with the referrer
      // embedded inside the PageState set above.  Nevertheless, to minimize
      // changes to behavior of old session restore entries, we restore the
      // deserialized referrer here.
      //
      // TODO(lukasza): Consider including the |deserialized_referrer| in the
      // PageState set above + drop the SetReferrer call below.  This will
      // slightly change the legacy behavior, but will make PageState and
      // Referrer consistent.
      entry->SetReferrer(deserialized_referrer);
    } else {
      // Note that PageState covers and will clobber some of the values covered
      // by data within |iterator| (e.g. URL and referrer).
      entry->SetPageState(
          blink::PageState::CreateFromEncodedData(content_state), context);

      // |deserialized_url| and |deserialized_referrer| are redundant wrt
      // PageState, but they should be consistent / in-sync.
      DCHECK_EQ(deserialized_url, entry->GetURL());
      DCHECK_EQ(deserialized_referrer.url, entry->GetReferrer().url);
      DCHECK_EQ(deserialized_referrer.policy, entry->GetReferrer().policy);
    }
  }

  {
    bool has_post_data;
    if (!iterator->ReadBool(&has_post_data))
      return false;
    entry->SetHasPostData(has_post_data);
  }

  {
    string original_request_url;
    if (!iterator->ReadString(&original_request_url))
      return false;
    entry->SetOriginalRequestURL(GURL(original_request_url));
  }

  {
    string base_url_for_data_url;
    if (!iterator->ReadString(&base_url_for_data_url))
      return false;
    entry->SetBaseURLForDataURL(GURL(base_url_for_data_url));
  }

  if (state_version >= internal::AW_STATE_VERSION_DATA_URL) {
    std::optional<base::span<const uint8_t>> data = iterator->ReadData();
    if (!data) {
      return false;
    }
    if (!data->empty()) {
      entry->SetDataURLAsString(base::MakeRefCounted<base::RefCountedString>(
          std::string(base::as_string_view(*data))));
    }
  }

  {
    bool is_overriding_user_agent;
    if (!iterator->ReadBool(&is_overriding_user_agent))
      return false;
    entry->SetIsOverridingUserAgent(is_overriding_user_agent);
  }

  {
    int64_t timestamp;
    if (!iterator->ReadInt64(&timestamp))
      return false;
    entry->SetTimestamp(base::Time::FromInternalValue(timestamp));
  }

  {
    int http_status_code;
    if (!iterator->ReadInt(&http_status_code))
      return false;
    entry->SetHttpStatusCode(http_status_code);
  }

  return true;
}

}  // namespace internal

}  // namespace android_webview
