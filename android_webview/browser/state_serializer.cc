// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/state_serializer.h"

#include <memory>
#include <string>
#include <vector>

#include "base/pickle.h"
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

const uint32_t AW_STATE_VERSION = internal::AW_STATE_VERSION_DATA_URL;

}  // namespace

void WriteToPickle(content::WebContents& web_contents, base::Pickle* pickle) {
  DCHECK(pickle);

  internal::WriteHeaderToPickle(pickle);

  content::NavigationController& controller = web_contents.GetController();
  const int entry_count = controller.GetEntryCount();
  const int selected_entry = controller.GetLastCommittedEntryIndex();
  // A NavigationEntry will always exist, so there will always be at least 1
  // entry.
  DCHECK_GE(entry_count, 1);
  DCHECK_GE(selected_entry, 0);
  DCHECK_LT(selected_entry, entry_count);

  pickle->WriteInt(entry_count);
  pickle->WriteInt(selected_entry);
  for (int i = 0; i < entry_count; ++i) {
    internal::WriteNavigationEntryToPickle(*controller.GetEntryAtIndex(i),
                                           pickle);
  }

  // Please update AW_STATE_VERSION and IsSupportedVersion() if serialization
  // format is changed.
  // Make sure the serialization format is updated in a backwards compatible
  // way.
}

bool RestoreFromPickle(base::PickleIterator* iterator,
                       content::WebContents* web_contents) {
  DCHECK(iterator);
  DCHECK(web_contents);

  uint32_t state_version = internal::RestoreHeaderFromPickle(iterator);
  if (!state_version)
    return false;

  int entry_count = -1;
  int selected_entry = -2;  // -1 is a valid value

  if (!iterator->ReadInt(&entry_count))
    return false;

  if (!iterator->ReadInt(&selected_entry))
    return false;

  if (entry_count < 0)
    return false;
  if (selected_entry < -1)
    return false;
  if (selected_entry >= entry_count)
    return false;

  std::unique_ptr<content::NavigationEntryRestoreContext> context =
      content::NavigationEntryRestoreContext::Create();
  std::vector<std::unique_ptr<content::NavigationEntry>> entries;
  entries.reserve(entry_count);
  for (int i = 0; i < entry_count; ++i) {
    entries.push_back(content::NavigationEntry::Create());
    if (!internal::RestoreNavigationEntryFromPickle(
            state_version, iterator, entries[i].get(), context.get()))
      return false;
  }

  // |web_contents| takes ownership of these entries after this call.
  content::NavigationController& controller = web_contents->GetController();
  controller.Restore(selected_entry, content::RestoreType::kRestored, &entries);
  DCHECK_EQ(0u, entries.size());
  controller.LoadIfNecessary();

  return true;
}

namespace internal {

void WriteHeaderToPickle(base::Pickle* pickle) {
  WriteHeaderToPickle(AW_STATE_VERSION, pickle);
}

void WriteHeaderToPickle(uint32_t state_version, base::Pickle* pickle) {
  pickle->WriteUInt32(state_version);
}

uint32_t RestoreHeaderFromPickle(base::PickleIterator* iterator) {
  uint32_t state_version = -1;
  if (!iterator->ReadUInt32(&state_version))
    return 0;

  if (IsSupportedVersion(state_version)) {
    return state_version;
  }

  return 0;
}

bool IsSupportedVersion(uint32_t state_version) {
  return state_version == internal::AW_STATE_VERSION_INITIAL ||
         state_version == internal::AW_STATE_VERSION_DATA_URL;
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
    const char* data;
    size_t size;
    if (!iterator->ReadData(&data, &size))
      return false;
    if (size > 0) {
      scoped_refptr<base::RefCountedString> ref = new base::RefCountedString();
      ref->as_string().assign(data, size);
      entry->SetDataURLAsString(ref);
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
