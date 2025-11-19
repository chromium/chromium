// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/caching_zero_state_suggestions_manager.h"

#include "base/types/id_type.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace contextual_cueing {

std::vector<std::string> EmptySuggestions() {
  return {};
}

class CacheIdClass;
using CacheId = base::IdType32<CacheIdClass>;
constexpr size_t kCacheSize = 10;

class CachingContextualCueingServiceImpl
    : public CachingZeroStateSuggestionsManager {
 public:
  explicit CachingContextualCueingServiceImpl(
      contextual_cueing::ContextualCueingService* service)
      : contextual_queueing_service_(service) {}

  void GetContextualGlicZeroStateSuggestionsForFocusedTab(
      content::WebContents* focused_tab,
      bool is_fre,
      std::optional<std::vector<std::string>> supported_tools,
      GlicSuggestionsCallback callback) override {
    PageReference focused_page;
    std::vector<GURL> urls;
    if (focused_tab) {
      urls.push_back(focused_tab->GetLastCommittedURL());
      focused_page.page = focused_tab->GetPrimaryPage().GetWeakPtr();
    }
    Entry* new_entry = TryCachedRequest(
        CacheKey{
            .is_focused_tab_request = true,
            .urls = std::move(urls),
            .is_fre = is_fre,
            .supported_tools = supported_tools,
            .focused_page = focused_page,
        },
        std::move(callback));
    if (!new_entry) {
      return;
    }

    contextual_queueing_service_
        ->GetContextualGlicZeroStateSuggestionsForFocusedTab(
            focused_tab, is_fre, supported_tools,
            mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                base::BindOnce(
                    &CachingContextualCueingServiceImpl::OnResultReceived,
                    GetWeakPtr(), new_entry->id),
                EmptySuggestions()));
  }

  void GetContextualGlicZeroStateSuggestionsForPinnedTabs(
      std::vector<content::WebContents*> pinned_web_contents,
      bool is_fre,
      std::optional<std::vector<std::string>> supported_tools,
      content::WebContents* focused_tab,
      GlicSuggestionsCallback callback) override {
    PageReference focused_page;
    std::vector<GURL> urls;
    if (focused_tab) {
      urls.push_back(focused_tab->GetLastCommittedURL());
      focused_page.page = focused_tab->GetPrimaryPage().GetWeakPtr();
    }
    for (auto* web_contents : pinned_web_contents) {
      if (web_contents != focused_tab) {
        urls.push_back(web_contents->GetLastCommittedURL());
      }
    }

    Entry* new_entry = TryCachedRequest(
        CacheKey{
            .is_focused_tab_request = false,
            .urls = std::move(urls),
            .is_fre = is_fre,
            .supported_tools = supported_tools,
            .focused_page = focused_page,
        },
        std::move(callback));
    if (!new_entry) {
      return;
    }

    contextual_queueing_service_
        ->GetContextualGlicZeroStateSuggestionsForPinnedTabs(
            pinned_web_contents, is_fre, supported_tools, focused_tab,
            mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                base::BindOnce(
                    &CachingContextualCueingServiceImpl::OnResultReceived,
                    GetWeakPtr(), new_entry->id),
                EmptySuggestions()));
  }

  base::WeakPtr<CachingContextualCueingServiceImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void OnResultReceived(CacheId id, std::vector<std::string> result) {
    for (auto& e : entries_) {
      if (e.id == id) {
        auto callbacks =
            std::exchange(*std::get_if<1>(&e.result_or_callbacks), {});
        for (auto& callback : callbacks) {
          std::move(callback).Run(result);
        }
        e.result_or_callbacks = std::move(result);
      }
    }
  }

  struct PageReference {
    bool operator==(const PageReference& rhs) const {
      if (page.WasInvalidated() || rhs.page.WasInvalidated()) {
        return false;
      }
      if (!page != !rhs.page) {
        return false;
      }
      if (!page) {
        return true;
      }
      return page.get() == rhs.page.get();
    }

    base::WeakPtr<content::Page> page;
  };

  struct CacheKey {
    bool is_focused_tab_request = false;
    std::vector<GURL> urls;
    bool is_fre = false;
    std::optional<std::vector<std::string>> supported_tools;
    PageReference focused_page;

    bool operator==(const CacheKey& other) const = default;
  };

  struct Entry {
    CacheId id;
    CacheKey request;
    std::variant<std::vector<std::string>, std::vector<GlicSuggestionsCallback>>
        result_or_callbacks;
  };

  // Either promises to call `callback` with a cached response, or returns
  // a new cache entry.
  Entry* TryCachedRequest(CacheKey request, GlicSuggestionsCallback callback) {
    if (!cache_disabled_) {
      for (auto& entry : entries_) {
        if (entry.request == request) {
          std::visit(absl::Overload(
                         [&](std::vector<std::string>& result) {
                           std::move(callback).Run(result);
                         },
                         [&](std::vector<GlicSuggestionsCallback>& callbacks) {
                           callbacks.push_back(std::move(callback));
                         }),
                     entry.result_or_callbacks);
          return nullptr;
        }
      }
    }

    std::vector<GlicSuggestionsCallback> callbacks;
    callbacks.push_back(std::move(callback));
    entries_.push_front(Entry{.id = id_generator_.GenerateNextId(),
                              .request = std::move(request),
                              .result_or_callbacks = std::move(callbacks)});
    if (entries_.size() > kCacheSize) {
      auto* dropped_callbacks =
          std::get_if<1>(&entries_.back().result_or_callbacks);
      if (dropped_callbacks) {
        for (auto& cb : std::exchange(*dropped_callbacks, {})) {
          std::move(cb).Run(EmptySuggestions());
        }
      }
      entries_.pop_back();
    }
    return &entries_.front();
  }

  bool cache_disabled_ = false;
  CacheId::Generator id_generator_;
  raw_ptr<contextual_cueing::ContextualCueingService>
      contextual_queueing_service_;
  std::deque<Entry> entries_;
  base::WeakPtrFactory<CachingContextualCueingServiceImpl> weak_ptr_factory_{
      this};
};

std::unique_ptr<CachingZeroStateSuggestionsManager>
CreateCachingZeroStateSuggestionsManager(ContextualCueingService* service) {
  return std::make_unique<CachingContextualCueingServiceImpl>(service);
}

}  // namespace contextual_cueing
