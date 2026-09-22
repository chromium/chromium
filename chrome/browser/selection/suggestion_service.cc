// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/selection/suggestion_service.h"

#include <algorithm>
#include <utility>

#include "base/check_deref.h"
#include "base/containers/extend.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "components/tabs/public/tab_interface.h"

namespace selection {

DEFINE_USER_DATA(SuggestionService);

struct SuggestionService::ActiveRequest
    : public base::RefCounted<ActiveRequest> {
  ActiveRequest(size_t num_endpoints, SuggestionsCallback cb)
      : remaining_endpoints(num_endpoints), callback(std::move(cb)) {}

  size_t remaining_endpoints;
  SuggestionsCallback callback;
  bool in_synchronous_dispatch = true;
  bool has_synchronous_response = false;
  std::vector<std::unique_ptr<Suggestion>> synchronous_suggestions;

 private:
  friend class base::RefCounted<ActiveRequest>;
  ~ActiveRequest() = default;
};

// static
SuggestionService* SuggestionService::From(tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

// static
SuggestionService* SuggestionService::FromTabWebContents(
    content::WebContents* tab_web_contents) {
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(tab_web_contents);
  return From(tab);
}

SuggestionService::SuggestionService(tabs::TabInterface* tab)
    : tab_(CHECK_DEREF(tab)),
      scoped_unowned_user_data_(tab->GetUnownedUserDataHost(), *this) {}

SuggestionService::~SuggestionService() {
  endpoints_.clear();
}

void SuggestionService::RegisterEndpoint(SuggestionEndpoint* endpoint) {
  if (!endpoint) {
    return;
  }
  if (!std::ranges::contains(endpoints_, endpoint)) {
    endpoints_.push_back(endpoint);
  }
}

void SuggestionService::UnregisterEndpoint(SuggestionEndpoint* endpoint) {
  std::erase(endpoints_, endpoint);
}

void SuggestionService::RequestSuggestions(const AreaOfInterest& processed_area,
                                           SuggestionsCallback callback) {
  if (endpoints_.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       std::vector<std::unique_ptr<Suggestion>>(),
                       /*complete=*/true));
    return;
  }

  auto active_request = base::MakeRefCounted<ActiveRequest>(
      endpoints_.size(), std::move(callback));
  std::vector<raw_ptr<SuggestionEndpoint>> endpoints_snapshot = endpoints_;
  for (SuggestionEndpoint* endpoint : endpoints_snapshot) {
    endpoint->RequestSuggestions(
        processed_area,
        base::BindRepeating(&SuggestionService::OnEndpointSuggestions,
                            weak_factory_.GetWeakPtr(), active_request,
                            base::OwnedRef(false)));
  }

  active_request->in_synchronous_dispatch = false;
  if (active_request->has_synchronous_response) {
    bool all_complete = active_request->remaining_endpoints == 0;
    active_request->callback.Run(
        std::move(active_request->synchronous_suggestions), all_complete);
  }
}

void SuggestionService::OnEndpointSuggestions(
    scoped_refptr<ActiveRequest> active_request,
    bool& endpoint_completed,
    std::vector<std::unique_ptr<Suggestion>> suggestions,
    bool complete) {
  if (complete && !endpoint_completed) {
    endpoint_completed = true;
    active_request->remaining_endpoints--;
  }
  if (active_request->in_synchronous_dispatch) {
    active_request->has_synchronous_response = true;
    base::Extend(active_request->synchronous_suggestions,
                 std::move(suggestions));
    return;
  }
  bool all_complete = (active_request->remaining_endpoints == 0);
  active_request->callback.Run(std::move(suggestions), all_complete);
}

}  // namespace selection

