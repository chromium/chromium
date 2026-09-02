// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/context_hub_service.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/check_deref.h"
#include "base/containers/adapters.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/context_hub/auto_todos/auto_todo_entry.h"
#include "chrome/browser/context_hub/auto_todos/auto_todos_store.h"
#include "chrome/browser/context_hub/features.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank.h"
#include "chrome/browser/context_hub/storage/context_hub_backend.h"
#include "chrome/browser/context_hub/tab_group_store/tab_group_entry.h"
#include "chrome/browser/context_hub/tab_group_store/tab_group_entry_conversions.h"
#include "chrome/browser/context_hub/tab_group_store/tab_group_store.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_tab_visit_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/context_hub.pb.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/core/page_content_extraction_types.h"
#include "components/personal_context/core/personal_context_service.h"
#include "components/personal_context/proto/features/auto_todos.pb.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/signin/public/base/persistent_repeating_timer.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/tab_list/tab_removed_reason.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

namespace context_hub {

namespace {

// Maximum number of parallel MES requests for tab-based todos generation.
// Matches the execution limit for ModelBasedCapabilityKey::kContextHub in
// components/optimization_guide/core/model_execution/model_execution_manager.cc.
constexpr int kMaxConcurrentMesRequests = 10;

optimization_guide::proto::MemoryBankEntry ToMemoryBankEntryProto(
    const MemoryBankEntry& entry) {
  optimization_guide::proto::MemoryBankEntry mb_proto;
  mb_proto.set_id(entry.id);
  switch (entry.type) {
    case MemoryBankType::kTab:
      mb_proto.set_type(optimization_guide::proto::MEMORY_BANK_TYPE_TAB);
      break;
    case MemoryBankType::kTextSelection:
      mb_proto.set_type(
          optimization_guide::proto::MEMORY_BANK_TYPE_TEXT_SELECTION);
      break;
  }
  mb_proto.set_timestamp_ms(entry.timestamp.InMillisecondsSinceUnixEpoch());
  mb_proto.set_url(entry.url.spec());
  mb_proto.set_tab_title(entry.tab_title);
  if (entry.selected_text.has_value()) {
    mb_proto.set_selected_text(*entry.selected_text);
  }
  for (const auto& tag : entry.tags) {
    mb_proto.add_tags(tag);
  }
  return mb_proto;
}

base::TimeDelta GetDurationOfCurrentOrLastVisit(
    content::WebContents* web_contents) {
  if (tabs::TabInterface* tab =
          tabs::TabInterface::MaybeGetFromContents(web_contents)) {
    if (contextual_tasks::ContextualTasksTabVisitTracker* tracker =
            contextual_tasks::ContextualTasksTabVisitTracker::From(tab)) {
      return tracker->GetDurationOfCurrentOrLastVisit();
    }
  }
  return base::TimeDelta();
}

using TabContextBarrierCallback = base::RepeatingCallback<void(
    std::pair<TabData, std::optional<optimization_guide::proto::PageContext>>)>;

void OnPageContentExtracted(
    TabContextBarrierCallback barrier_callback,
    base::WeakPtr<content::WebContents> web_contents,
    base::WeakPtr<content::Page> page,
    std::optional<page_content_annotations::ExtractedPageContentResult>
        extracted_result) {
  // Protect against navigations during the async extraction.
  if (!web_contents || !page || !page->IsPrimary()) {
    barrier_callback.Run(std::make_pair(TabData(), std::nullopt));
    return;
  }

  // Populate the tab data to be used in the barrier callback.
  TabData tab;
  SessionID session_id =
      sessions::SessionTabHelper::IdForTab(web_contents.get());
  tab.id = session_id.is_valid() ? session_id.id() : -1;
  tab.title = base::UTF16ToUTF8(web_contents->GetTitle());
  tab.url = web_contents->GetLastCommittedURL();
  tab.last_active_time = web_contents->GetLastActiveTime();
  tab.last_foreground_duration =
      GetDurationOfCurrentOrLastVisit(web_contents.get());

  std::optional<optimization_guide::proto::PageContext> page_context;
  if (extracted_result && extracted_result->page_content) {
    page_context.emplace();
    *page_context->mutable_annotated_page_content() =
        extracted_result->page_content->data;
  }
  barrier_callback.Run(std::make_pair(std::move(tab), std::move(page_context)));
}

ThirdPartyData::GroupType ToThirdPartyGroupType(
    optimization_guide::proto::BrowserBasedTodosResponse::GroupType
        group_type) {
  switch (group_type) {
    case optimization_guide::proto::BrowserBasedTodosResponse::
        GROUP_TYPE_READING_LIST:
      return ThirdPartyData::GroupType::kReadingList;
    case optimization_guide::proto::BrowserBasedTodosResponse::
        GROUP_TYPE_NUDGE_TO_CLOSE:
      return ThirdPartyData::GroupType::kNudgeToClose;
    case optimization_guide::proto::BrowserBasedTodosResponse::
        GROUP_TYPE_UNFINISHED:
      return ThirdPartyData::GroupType::kUnfinishedAction;
    case optimization_guide::proto::BrowserBasedTodosResponse::
        GROUP_TYPE_SHOPPING_CART:
      return ThirdPartyData::GroupType::kShoppingCart;
    case optimization_guide::proto::BrowserBasedTodosResponse::
        GROUP_TYPE_UNSPECIFIED:
    default:
      return ThirdPartyData::GroupType::kNoMatch;
  }
}

personal_context::proto::AutoTodoItem ToAutoTodoItemProto(
    const AutoTodoEntry& entry) {
  personal_context::proto::AutoTodoItem proto;
  proto.set_id(entry.id);
  proto.set_title(entry.title);
  proto.set_description(entry.description);
  proto.set_importance_score(entry.importance_score);
  switch (entry.status) {
    case AutoTodoEntry::Status::kActive:
      proto.set_status(personal_context::proto::AutoTodoItem::STATUS_ACTIVE);
      break;
    case AutoTodoEntry::Status::kCompleted:
      proto.set_status(personal_context::proto::AutoTodoItem::STATUS_COMPLETED);
      break;
    case AutoTodoEntry::Status::kDismissed:
      proto.set_status(personal_context::proto::AutoTodoItem::STATUS_DISMISSED);
      break;
  }
  if (const FirstPartyData* first_party =
          std::get_if<FirstPartyData>(&entry.data)) {
    proto.set_actionable_url(first_party->actionable_url.spec());
    for (const SourceReference& ref : first_party->source_references) {
      personal_context::proto::SourceReference* source_ref =
          proto.add_source_references();
      personal_context::proto::GmailReference* gmail_ref =
          source_ref->mutable_gmail();
      gmail_ref->set_message_url(ref.url.spec());
      gmail_ref->set_subject(ref.subject);
    }
  }
  return proto;
}

}  // namespace

ContextHubService::ContextHubService(
    Profile* profile,
    signin::IdentityManager* identity_manager,
    personal_context::PersonalContextService* personal_context_service,
    optimization_guide::RemoteModelExecutor*
        optimization_guide_remote_model_executor,
    tab_groups::TabGroupSyncService* tab_group_sync_service,
    page_content_annotations::PageContentExtractionService*
        page_content_extraction_service,
    std::unique_ptr<MemoryBank> memory_bank,
    std::unique_ptr<TabGroupStore> tab_group_store,
    std::unique_ptr<ContextHubBackend> context_hub_backend,
    std::unique_ptr<AutoTodosStore> auto_todos_store)
    : profile_(CHECK_DEREF(profile)),
      identity_manager_(CHECK_DEREF(identity_manager)),
      personal_context_service_(CHECK_DEREF(personal_context_service)),
      optimization_guide_remote_model_executor_(
          CHECK_DEREF(optimization_guide_remote_model_executor)),
      tab_group_sync_service_(CHECK_DEREF(tab_group_sync_service)),
      page_content_extraction_service_(
          CHECK_DEREF(page_content_extraction_service)),
      tab_group_chat_history_cache_(
          features::kMaxTabGroupChatHistoryTurns.Get()),
      todo_feedback_cache_(features::kMaxTodoFeedbackCacheSize.Get()),
      context_hub_backend_(std::move(context_hub_backend)),
      memory_bank_(std::move(memory_bank)),
      tab_group_store_(std::move(tab_group_store)),
      auto_todos_store_(std::move(auto_todos_store)) {
  CHECK(memory_bank_);
  identity_manager_observation_.Observe(&identity_manager_.get());
  if (auto_todos_store_) {
    auto_todos_store_->AddObserver(this);
    first_party_auto_todos_timer_.Start(
        FROM_HERE, features::kFirstPartyAutoTodosInterval.Get(),
        base::BindRepeating(
            &ContextHubService::OnFirstPartyAutoTodosTimerTriggered,
            weak_factory_.GetWeakPtr()));
    MaybeTriggerFirstPartyAutoTodosGeneration();

#if !BUILDFLAG(IS_ANDROID)
    browser_tab_strip_tracker_ =
        std::make_unique<BrowserTabStripTracker>(this, this);
    browser_tab_strip_tracker_->Init();
#endif
  }
}

ContextHubService::~ContextHubService() {
  if (auto_todos_store_) {
    auto_todos_store_->RemoveObserver(this);
  }
  if (pending_tab_todos_callback_) {
    observers_.Notify(&Observer::OnThirdPartyAutoTodosGenerationStateChanged,
                      false);
    std::move(pending_tab_todos_callback_).Run(false);
  }
  if (is_generating_first_party_auto_todos_) {
    observers_.Notify(&Observer::OnFirstPartyAutoTodosGenerationStateChanged,
                      false);
    for (auto& cb : std::exchange(pending_first_party_callbacks_, {})) {
      if (cb) {
        std::move(cb).Run(false);
      }
    }
  }
}

#if !BUILDFLAG(IS_ANDROID)
bool ContextHubService::ShouldTrackBrowser(BrowserWindowInterface* browser) {
  return browser->GetProfile() == &profile_.get();
}

void ContextHubService::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // Delete any cached AutoTodos that are associated with a removed tab.
  if (change.type() == TabStripModelChange::kRemoved) {
    const TabStripModelChange::Remove* remove = change.GetRemove();
    for (const auto& removed_tab : remove->contents) {
      if (TabRemoveReasonUtils::WillDeleteWebContents(
              removed_tab.remove_reason)) {
        content::WebContents* contents =
            removed_tab.contents
                ? removed_tab.contents.get()
                : (removed_tab.tab ? removed_tab.tab->GetContents() : nullptr);
        if (contents) {
          SessionID session_id = sessions::SessionTabHelper::IdForTab(contents);
          if (session_id.is_valid()) {
            DeleteAutoTodoByTabId(session_id.id(), base::DoNothing());
          }
        }
      }
    }
  }
}
#endif

void ContextHubService::MaybeTriggerFirstPartyAutoTodosGeneration() {
  if (!identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin) ||
      !identity_manager_->AreRefreshTokensLoaded()) {
    return;
  }
  const base::TimeDelta time_since_last_generation =
      base::Time::Now() - last_first_party_generation_time_;
  if (!last_first_party_generation_time_.is_null() &&
      time_since_last_generation >= base::TimeDelta() &&
      time_since_last_generation <
          features::kFirstPartyAutoTodosInterval.Get()) {
    return;
  }
  GenerateFirstPartyAutoTodos(base::DoNothing());
}

void ContextHubService::OnFirstPartyAutoTodosTimerTriggered() {
  if (auto_todos_store_) {
    auto_todos_store_->DeleteExpiredEntries(base::DoNothing());
  }
  MaybeTriggerFirstPartyAutoTodosGeneration();
}

void ContextHubService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  // Trigger generation when a primary account signs in to ensure 1P AutoTodos
  // are fetched as soon as the user is authenticated.
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
      signin::PrimaryAccountChangeEvent::Type::kSet) {
    MaybeTriggerFirstPartyAutoTodosGeneration();
  }
}

void ContextHubService::OnRefreshTokensLoaded() {
  // Trigger generation once refresh tokens finish loading after startup to
  // ensure authenticated network requests can succeed.
  MaybeTriggerFirstPartyAutoTodosGeneration();
}

void ContextHubService::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error,
    signin_metrics::SourceForRefreshTokenOperation token_operation_source) {
  // Trigger generation if an authentication error has been resolved for the
  // primary account (e.g. after the user re-authenticates).
  if (account_info.account_id == identity_manager_->GetPrimaryAccountId(
                                     signin::ConsentLevel::kSignin) &&
      error.state() == GoogleServiceAuthError::NONE) {
    MaybeTriggerFirstPartyAutoTodosGeneration();
  }
}

void ContextHubService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ContextHubService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ContextHubService::OnAutoTodosChanged(
    base::span<const AutoTodoEntry> entries) {
  observers_.Notify(&Observer::OnAutoTodosChanged, entries);
}

void ContextHubService::GenerateFirstPartyAutoTodos(
    AutoTodosStore::OperationCallback callback) {
  if (!auto_todos_store_) {
    if (callback) {
      std::move(callback).Run(false);
    }
    return;
  }

  if (callback) {
    pending_first_party_callbacks_.push_back(std::move(callback));
  }

  // If a request is already in flight, simply attach the callback and wait for
  // completion instead of rejecting the call or sending duplicate requests.
  if (is_generating_first_party_auto_todos_) {
    return;
  }

  is_generating_first_party_auto_todos_ = true;
  observers_.Notify(&Observer::OnFirstPartyAutoTodosGenerationStateChanged,
                    true);

  // Fetch all existing items from the store to use as deduplication input to
  // the server.
  auto_todos_store_->GetAllItems(
      base::BindOnce(&ContextHubService::OnCachedFirstPartyAutoTodosFetched,
                     weak_factory_.GetWeakPtr()));
}

void ContextHubService::OnCachedFirstPartyAutoTodosFetched(
    std::vector<AutoTodoEntry> stored_todos) {
  if (!auto_todos_store_ || !is_generating_first_party_auto_todos_) {
    FinishFirstPartyAutoTodosGeneration(/*success=*/false);
    return;
  }

  personal_context::proto::AutoTodosRequest request_metadata;
  for (const AutoTodoEntry& entry : stored_todos) {
    if (entry.is_first_party()) {
      *request_metadata.add_existing_todos() = ToAutoTodoItemProto(entry);
    }
  }

  personal_context::ContextMemoryRequestOptions options;
  options.request_timeout = features::kAutoTodosTimeoutSeconds.Get();

  personal_context_service_->FetchContext(
      personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
      request_metadata, options,
      base::BindOnce(&ContextHubService::OnFirstPartyAutoTodosFetched,
                     weak_factory_.GetWeakPtr()));
}

bool ContextHubService::IsGeneratingFirstPartyAutoTodos() const {
  return is_generating_first_party_auto_todos_;
}

base::Time ContextHubService::GetLastFirstPartyGenerationTime() const {
  return last_first_party_generation_time_;
}

base::Time ContextHubService::GetLastThirdPartyGenerationTime() const {
  return last_third_party_generation_time_;
}

void ContextHubService::GenerateTabBasedTodos(
    std::vector<base::WeakPtr<content::WebContents>> tabs,
    AutoTodosStore::OperationCallback callback) {
  if (!auto_todos_store_ || pending_tab_todos_callback_) {
    if (callback) {
      std::move(callback).Run(false);
    }
    return;
  }

  auto_todos_store_->GetAllItems(base::BindOnce(
      &ContextHubService::OnAllAutoTodosFetchedForTabBasedTodos,
      weak_factory_.GetWeakPtr(), std::move(tabs), std::move(callback)));
}

void ContextHubService::OnAllAutoTodosFetchedForTabBasedTodos(
    std::vector<base::WeakPtr<content::WebContents>> tabs,
    AutoTodosStore::OperationCallback callback,
    std::vector<AutoTodoEntry> stored_todos) {
  if (!auto_todos_store_ || pending_tab_todos_callback_) {
    if (callback) {
      std::move(callback).Run(false);
    }
    return;
  }

  base::flat_set<int64_t> cached_tab_ids;
  for (const auto& entry : stored_todos) {
    if (entry.is_third_party()) {
      if (auto tab_id = entry.tab_id()) {
        cached_tab_ids.insert(*tab_id);
      }
    }
  }

  std::vector<base::WeakPtr<content::WebContents>> eligible_tabs;
  for (auto& tab : tabs) {
    if (!tab) {
      continue;
    }
    // Only consider tabs that are not actively visible.
    if (tab->GetVisibility() == content::Visibility::VISIBLE) {
      continue;
    }
    // Only consider unpinned tabs.
    if (tabs::TabInterface* tab_interface =
            tabs::TabInterface::MaybeGetFromContents(tab.get())) {
      if (tab_interface->IsPinned()) {
        continue;
      }
    }
    // Only consider tabs that have a valid last active time.
    // All tabs are sent to the model and filtered out by varying time
    // thresholds based on group type. This will be simplified in the future to
    // a lightweight pre-classifier.
    if (tab->GetLastActiveTime() <= base::Time::UnixEpoch()) {
      continue;
    }
    SessionID session_id = sessions::SessionTabHelper::IdForTab(tab.get());
    int64_t tab_id = session_id.is_valid() ? session_id.id() : -1;
    if (tab_id != -1 && cached_tab_ids.contains(tab_id)) {
      continue;
    }
    eligible_tabs.push_back(std::move(tab));
  }

  if (eligible_tabs.empty()) {
    last_third_party_generation_time_ = base::Time::Now();
    if (callback) {
      // Return early if there are no eligible tabs to process. Return true to
      // indicate that the operation was successful, just with no results.
      std::move(callback).Run(true);
    }
    return;
  }

  // Store the callback to be invoked when page context extraction and model
  // execution are complete.
  pending_tab_todos_callback_ = std::move(callback);
  observers_.Notify(&Observer::OnThirdPartyAutoTodosGenerationStateChanged,
                    true);

  // Collects the asynchronous page content extraction results across all
  // eligible tabs. Once all tab extractions have completed, `barrier_callback`
  // aggregates the results and invokes `OnTabContextsFetched`.
  TabContextBarrierCallback barrier_callback = base::BarrierCallback<std::pair<
      TabData, std::optional<optimization_guide::proto::PageContext>>>(
      eligible_tabs.size(),
      base::BindOnce(&ContextHubService::OnTabContextsFetched,
                     weak_factory_.GetWeakPtr()));

  for (auto& tab : eligible_tabs) {
    // See if the tab needs to be loaded to get its WebContents to extract the
    // page content from.
    tab->GetController().LoadIfNecessary();

    // Get the page content from the primary page.
    content::Page& primary_page = tab->GetPrimaryPage();
    base::WeakPtr<content::Page> page_weak_ptr = primary_page.GetWeakPtr();
    page_content_extraction_service_
        ->GetExtractedPageContentAndEligibilityForPageAsync(
            primary_page,
            base::BindOnce(&OnPageContentExtracted, barrier_callback,
                           std::move(tab), std::move(page_weak_ptr)),
            /*trigger_if_not_cached=*/true);
  }
}

void ContextHubService::OnTabContextsFetched(
    std::vector<
        std::pair<TabData,
                  std::optional<optimization_guide::proto::PageContext>>>
        tab_contexts) {
  if (!pending_tab_todos_callback_) {
    return;
  }

  // Add all eligible tabs to the pending MES requests queue.
  for (auto& [tab, page_context] : tab_contexts) {
    if (tab.id != -1 && tab.url.is_valid() &&
        tab.last_active_time > base::Time::UnixEpoch() && page_context) {
      pending_tab_todos_requests_.emplace(std::move(tab),
                                          std::move(*page_context));
    }
  }

  // Start processing the MES requests.
  ProcessNextTabBasedTodosMesBatch();
}

void ContextHubService::ProcessNextTabBasedTodosMesBatch() {
  // If all pending requests have been dispatched and no active model executions
  // remain, commit all generated todos to the store in a single batch.
  if (pending_tab_todos_requests_.empty() && active_tab_todos_requests_ == 0) {
    if (!generated_tab_todos_.empty()) {
      auto_todos_store_->AddAllTodos(
          generated_tab_todos_,
          base::BindOnce(&ContextHubService::FinishTabBasedTodosGeneration,
                         weak_factory_.GetWeakPtr()));
    } else {
      FinishTabBasedTodosGeneration(/*success=*/true);
    }
    return;
  }

  // Dispatch requests up to the maximum concurrency limit.
  while (active_tab_todos_requests_ < kMaxConcurrentMesRequests &&
         !pending_tab_todos_requests_.empty()) {
    auto [tab, page_context] = std::move(pending_tab_todos_requests_.front());
    pending_tab_todos_requests_.pop();
    active_tab_todos_requests_++;

    // Construct the ContextHubRequest proto with the tab and its page context.
    optimization_guide::proto::ContextHubRequest request;
    request.set_request_type(optimization_guide::proto::
                                 CONTEXT_HUB_REQUEST_TYPE_BROWSER_BASED_TODOS);
    optimization_guide::proto::EntryItem* entry_item =
        request.add_entry_items();
    optimization_guide::proto::Tab* tab_proto = entry_item->mutable_tab();
    tab_proto->set_tab_id(tab.id);
    tab_proto->set_title(tab.title);
    tab_proto->set_url(tab.url.spec());
    tab_proto->set_last_active_timestamp_ms(
        tab.last_active_time.InMillisecondsSinceUnixEpoch());
    if (tab.last_foreground_duration.is_positive()) {
      tab_proto->set_last_foreground_duration_ms(
          tab.last_foreground_duration.InMilliseconds());
    }
    *tab_proto->mutable_page_context() = std::move(page_context);

    int64_t tab_id = tab.id;
    base::Time last_active_time = tab.last_active_time;

    optimization_guide_remote_model_executor_->ExecuteModel(
        optimization_guide::ModelBasedCapabilityKey::kContextHub, request,
        optimization_guide::ModelExecutionOptions(),
        base::BindOnce(&ContextHubService::OnTabBasedTodosMesResponseReceived,
                       weak_factory_.GetWeakPtr(), tab_id, last_active_time));
  }
}

void ContextHubService::OnTabBasedTodosMesResponseReceived(
    int64_t tab_id,
    base::Time last_active_time,
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  // Decrement active request count to free up a concurrency slot.
  active_tab_todos_requests_--;

  // Parse the model execution response and extract any generated todo.
  if (result.response.has_value()) {
    std::optional<optimization_guide::proto::ContextHubResponse> response =
        optimization_guide::ParsedAnyMetadata<
            optimization_guide::proto::ContextHubResponse>(*result.response);
    if (response && response->has_browser_based_todos_response()) {
      const auto& todo_proto = response->browser_based_todos_response();
      ThirdPartyData::GroupType group_type =
          ToThirdPartyGroupType(todo_proto.group_type());
      // Only record the todo if a valid non-empty title was generated and the
      // group type matches a known actionable category.
      if (!todo_proto.todo_title().empty() &&
          group_type != ThirdPartyData::GroupType::kNoMatch) {
        AutoTodoEntry entry;
        entry.title = todo_proto.todo_title();
        entry.description = todo_proto.todo_description();
        entry.status = AutoTodoEntry::Status::kActive;

        ThirdPartyData third_party;
        third_party.tab_id = tab_id;
        third_party.last_active_timestamp = last_active_time;
        third_party.group_type = group_type;
        entry.data = std::move(third_party);

        generated_tab_todos_.push_back(std::move(entry));
      }
    }
  }

  // Continue processing remaining queued requests or finalize generation.
  ProcessNextTabBasedTodosMesBatch();
}

void ContextHubService::FinishTabBasedTodosGeneration(bool success) {
  active_tab_todos_requests_ = 0;
  pending_tab_todos_requests_ = {};
  generated_tab_todos_.clear();

  if (success) {
    last_third_party_generation_time_ = base::Time::Now();
  }

  observers_.Notify(&Observer::OnThirdPartyAutoTodosGenerationStateChanged,
                    false);

  if (pending_tab_todos_callback_) {
    std::move(pending_tab_todos_callback_).Run(success);
  }
}

void ContextHubService::OnFirstPartyAutoTodosFetched(
    personal_context::FetchContextResult result) {
  if (!result.response.has_value()) {
    FinishFirstPartyAutoTodosGeneration(/*success=*/false);
    return;
  }

  personal_context::proto::AutoTodosResponse response;
  if (!response.ParseFromString(result.response.value().value())) {
    FinishFirstPartyAutoTodosGeneration(/*success=*/false);
    return;
  }

  std::vector<AutoTodoEntry> entries;
  entries.reserve(response.todos_size());
  for (const personal_context::proto::AutoTodoItem& todo : response.todos()) {
    AutoTodoEntry entry;
    if (!todo.id().empty()) {
      entry.id = todo.id();
    }
    entry.title = todo.title();
    entry.description = todo.description();
    entry.importance_score = todo.importance_score();
    entry.status = AutoTodoEntry::Status::kActive;

    FirstPartyData first_party;
    first_party.actionable_url = GURL(todo.actionable_url());
    for (const auto& ref : todo.source_references()) {
      if (ref.has_gmail()) {
        first_party.source_references.push_back(
            SourceReference{.url = GURL(ref.gmail().message_url()),
                            .subject = std::string(ref.gmail().subject())});
      }
    }
    entry.data = std::move(first_party);
    entries.push_back(std::move(entry));
  }

  auto_todos_store_->AddAllTodos(
      std::move(entries),
      base::BindOnce(&ContextHubService::FinishFirstPartyAutoTodosGeneration,
                     weak_factory_.GetWeakPtr()));
}

void ContextHubService::FinishFirstPartyAutoTodosGeneration(bool success) {
  is_generating_first_party_auto_todos_ = false;
  if (success) {
    last_first_party_generation_time_ = base::Time::Now();
    first_party_auto_todos_timer_.Reset();
  }
  observers_.Notify(&Observer::OnFirstPartyAutoTodosGenerationStateChanged,
                    false);
  for (auto& cb : std::exchange(pending_first_party_callbacks_, {})) {
    if (cb) {
      std::move(cb).Run(success);
    }
  }
}

void ContextHubService::GetAutoTodos(GetAutoTodosCallback callback) const {
  if (!auto_todos_store_) {
    std::move(callback).Run({});
    return;
  }
  auto_todos_store_->GetAllItems(std::move(callback));
}

void ContextHubService::UpdateAutoTodo(
    AutoTodoEntry item,
    AutoTodosStore::OperationCallback callback) {
  if (!auto_todos_store_) {
    std::move(callback).Run(false);
    return;
  }
  auto_todos_store_->AddOrUpdateItem(std::move(item), std::move(callback));
}

void ContextHubService::DeleteAutoTodoByTabId(
    int64_t tab_id,
    AutoTodosStore::OperationCallback callback) {
  if (!auto_todos_store_) {
    std::move(callback).Run(false);
    return;
  }
  auto_todos_store_->DeleteItemByTabId(tab_id, std::move(callback));
}

void ContextHubService::ClearFirstPartyAutoTodos(
    AutoTodosStore::OperationCallback callback) {
  if (!auto_todos_store_) {
    std::move(callback).Run(false);
    return;
  }
  last_first_party_generation_time_ = base::Time();
  auto_todos_store_->ClearFirstPartyTodos(std::move(callback));
}

void ContextHubService::ClearThirdPartyAutoTodos(
    AutoTodosStore::OperationCallback callback) {
  if (!auto_todos_store_) {
    std::move(callback).Run(false);
    return;
  }
  last_third_party_generation_time_ = base::Time();
  auto_todos_store_->ClearThirdPartyTodos(std::move(callback));
}

void ContextHubService::SetTodoFeedback(
    browser::context_hub::mojom::AutoTodoItemFeedbackPtr feedback) {
  if (!feedback) {
    return;
  }
  todo_feedback_cache_.Put(feedback->todo_id, feedback->liked);
}

void ContextHubService::DeleteTodoFeedback(const std::string& id) {
  auto it = todo_feedback_cache_.Peek(id);
  if (it != todo_feedback_cache_.end()) {
    todo_feedback_cache_.Erase(it);
  }
}

void ContextHubService::ClearTodoFeedbacks() {
  todo_feedback_cache_.Clear();
}

std::vector<browser::context_hub::mojom::AutoTodoItemFeedbackPtr>
ContextHubService::GetTodoFeedbacks() const {
  std::vector<browser::context_hub::mojom::AutoTodoItemFeedbackPtr> feedbacks;
  feedbacks.reserve(todo_feedback_cache_.size());
  for (const auto& [todo_id, liked] : todo_feedback_cache_) {
    auto feedback = browser::context_hub::mojom::AutoTodoItemFeedback::New();
    feedback->todo_id = todo_id;
    feedback->liked = liked;
    feedbacks.push_back(std::move(feedback));
  }
  return feedbacks;
}

void ContextHubService::AddTabGroupChatHistoryTurn(
    optimization_guide::proto::ChatHistoryTurn::Role role,
    std::string_view message_content) {
  optimization_guide::proto::ChatHistoryTurn turn;
  turn.set_role(role);
  turn.set_message_content(message_content);
  turn.set_timestamp_ms(base::Time::Now().InMillisecondsSinceUnixEpoch());
  TabGroupChatHistoryTurnId id =
      TabGroupChatHistoryTurnId::FromUnsafeValue(turn.timestamp_ms());
  tab_group_chat_history_cache_.Put(id, std::move(turn));
}

std::vector<optimization_guide::proto::ChatHistoryTurn>
ContextHubService::GetTabGroupChatHistory() const {
  std::vector<optimization_guide::proto::ChatHistoryTurn> history;
  history.reserve(tab_group_chat_history_cache_.size());
  for (const auto& [id, turn] : base::Reversed(tab_group_chat_history_cache_)) {
    history.push_back(turn);
  }
  return history;
}

void ContextHubService::ClearTabGroupChatHistory() {
  tab_group_chat_history_cache_.Clear();
}

void ContextHubService::SetPendingMemoryBankEntry(MemoryBankEntry entry) {
  pending_memory_bank_entry_ = std::move(entry);
}

std::optional<MemoryBankEntry> ContextHubService::GetPendingMemoryBankEntry()
    const {
  return pending_memory_bank_entry_;
}

bool ContextHubService::SavePendingMemoryBankEntry(
    std::vector<std::string> tags,
    std::optional<std::string> note,
    std::optional<std::string> collection) {
  if (!pending_memory_bank_entry_.has_value()) {
    return false;
  }
  MemoryBankEntry entry = std::move(*pending_memory_bank_entry_);
  pending_memory_bank_entry_.reset();

  entry.tags = std::move(tags);
  entry.note = std::move(note);
  entry.collection = std::move(collection);
  SaveMemoryBankEntry(std::move(entry), base::DoNothing());
  return true;
}

void ContextHubService::SaveMemoryBankEntry(
    MemoryBankEntry entry,
    MemoryBank::OperationCompleteCallback callback) {
  memory_bank_->SaveMemoryBankEntry(std::move(entry), std::move(callback));
}

void ContextHubService::UpdateMemoryBankEntryAnnotations(
    int64_t id,
    std::vector<std::string> tags,
    std::optional<std::string> note,
    std::optional<std::string> collection,
    MemoryBank::OperationCompleteCallback callback) {
  memory_bank_->UpdateEntryAnnotations(id, std::move(tags), std::move(note),
                                       std::move(collection),
                                       std::move(callback));
}

void ContextHubService::DeleteEntries(
    base::span<const int64_t> ids,
    MemoryBank::OperationCompleteCallback callback) {
  memory_bank_->DeleteEntries(ids, std::move(callback));
}

void ContextHubService::GetAllEntries(
    MemoryBank::GetEntriesCallback callback) const {
  memory_bank_->GetAllEntries(std::move(callback));
}

void ContextHubService::GetEntriesByIds(
    base::span<const int64_t> ids,
    MemoryBank::GetEntriesCallback callback) const {
  memory_bank_->GetEntriesByIds(ids, std::move(callback));
}

void ContextHubService::GetAllMemoryBankTags(
    MemoryBank::GetStringsCallback callback) const {
  memory_bank_->GetAllTags(std::move(callback));
}

void ContextHubService::GetAllMemoryBankCollections(
    MemoryBank::GetStringsCallback callback) const {
  memory_bank_->GetAllCollections(std::move(callback));
}

void ContextHubService::GetTabGroups(GetTabGroupsCallback callback) const {
  if (tab_group_store_) {
    tab_group_store_->GetAllGroups(std::move(callback));
  } else {
    std::move(callback).Run({});
  }
}

void ContextHubService::DeleteAllTabGroups(base::OnceClosure callback) {
  if (tab_group_store_) {
    tab_group_store_->DeleteAllGroups(std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void ContextHubService::ConfirmAllTabGroups(
    ConfirmAllTabGroupsCallback callback) {
  if (!tab_group_store_) {
    std::move(callback).Run(false, {});
    return;
  }
  tab_group_store_->GetAllGroups(
      base::BindOnce(&ContextHubService::OnAllTabGroupsFetchedForConfirmation,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

std::optional<base::Uuid> ContextHubService::AddTabGroupToSyncService(
    const TabGroupEntry& entry) {
  auto saved_group = ToSavedTabGroup(entry);
  if (!saved_group) {
    return std::nullopt;
  }
  base::Uuid guid = saved_group->saved_guid();
  tab_group_sync_service_->AddGroup(*std::move(saved_group));
  return guid;
}

std::vector<base::Uuid> ContextHubService::AddTabGroupsToSyncService(
    base::span<const TabGroupEntry> entries) {
  std::vector<base::Uuid> added_group_guids;
  for (const TabGroupEntry& entry : entries) {
    if (auto guid = AddTabGroupToSyncService(entry)) {
      added_group_guids.push_back(*guid);
    }
  }
  return added_group_guids;
}

void ContextHubService::OnAllTabGroupsFetchedForConfirmation(
    ConfirmAllTabGroupsCallback callback,
    std::vector<TabGroupEntry> groups) {
  std::vector<base::Uuid> added_group_guids =
      AddTabGroupsToSyncService(groups);
  DeleteAllTabGroups(base::BindOnce(
      std::move(callback), true, std::move(added_group_guids)));
}

std::vector<TabGroupEntry>
ContextHubService::GetConfirmedTabGroups() const {
  std::vector<tab_groups::SavedTabGroup> groups =
      tab_group_sync_service_->GetAllGroups();
  // Filter out closed or remotely synced tab groups that do not have active
  // tabs open in any browser window.
  std::erase_if(groups, [](const tab_groups::SavedTabGroup& group) {
    return !group.local_group_id().has_value() ||
           !std::ranges::any_of(group.saved_tabs(), [](const auto& tab) {
             return tab.local_tab_id().has_value();
           });
  });
  return FromSavedTabGroups(groups);
}

std::optional<TabGroupEntry>
ContextHubService::GetConfirmedTabGroup(const base::Uuid& group_guid) const {
  std::optional<tab_groups::SavedTabGroup> group =
      tab_group_sync_service_->GetGroup(group_guid);
  if (!group.has_value()) {
    return std::nullopt;
  }
  return FromSavedTabGroup(*group);
}

std::optional<tab_groups::LocalTabGroupID>
ContextHubService::GetLocalGroupIdForConfirmedGroup(
    const base::Uuid& group_guid) const {
  std::optional<tab_groups::SavedTabGroup> group =
      tab_group_sync_service_->GetGroup(group_guid);
  return group.has_value() ? group->local_group_id() : std::nullopt;
}

bool ContextHubService::RemoveConfirmedTabGroup(const base::Uuid& group_guid) {
  std::optional<tab_groups::SavedTabGroup> group =
      tab_group_sync_service_->GetGroup(group_guid);
  if (!group.has_value()) {
    return false;
  }
  tab_group_sync_service_->RemoveGroup(group_guid);
  return true;
}

bool ContextHubService::RemoveAllConfirmedTabGroups() {
  std::vector<tab_groups::SavedTabGroup> all_groups =
      tab_group_sync_service_->GetAllGroups();
  for (const tab_groups::SavedTabGroup& group : all_groups) {
    tab_group_sync_service_->RemoveGroup(group.saved_guid());
  }
  return true;
}

void ContextHubService::ConnectLocalTabGroup(
    const base::Uuid& group_guid,
    const tab_groups::LocalTabGroupID& local_id) {
  tab_group_sync_service_->ConnectLocalTabGroup(
      group_guid, local_id, tab_groups::OpeningSource::kOpenedFromRevisitUi);
}

// TODO(crbug.com/531938478): Update to handle APC ingestion.
void ContextHubService::GenerateTabGroups(std::vector<TabData> tabs,
                                          const std::string& user_command,
                                          GroupTabsCallback callback) {
  optimization_guide::proto::ContextHubRequest request;
  request.set_request_type(
      optimization_guide::proto::CONTEXT_HUB_REQUEST_TYPE_GROUPING);

  std::vector<TabGroupEntry> confirmed_groups = GetConfirmedTabGroups();
  base::flat_set<int64_t> existing_ids =
      base::MakeFlatSet<int64_t>(tabs, {}, &TabData::id);

  for (const TabGroupEntry& confirmed_group : confirmed_groups) {
    optimization_guide::proto::TabGroupMinimal* group_proto =
        request.add_pre_existing_tab_groups();
    group_proto->set_label(confirmed_group.label);
    group_proto->set_group_id(confirmed_group.id);
    for (const TabData& tab : confirmed_group.tabs) {
      if (tab.id != SessionID::InvalidValue().id()) {
        group_proto->add_tab_ids(tab.id);
        if (!existing_ids.contains(tab.id)) {
          existing_ids.insert(tab.id);
          tabs.push_back(tab);
        }
      }
    }
  }

  if (tabs.size() < 2) {
    std::move(callback).Run({}, std::move(tabs), /*text_response=*/"");
    return;
  }

  for (const TabData& tab : tabs) {
    optimization_guide::proto::EntryItem* entry_item =
        request.add_entry_items();
    optimization_guide::proto::Tab* tab_proto = entry_item->mutable_tab();
    tab_proto->set_tab_id(tab.id);
    tab_proto->set_title(tab.title);
    tab_proto->set_url(tab.url.spec());
  }

  for (const auto& turn : GetTabGroupChatHistory()) {
    *request.add_chat_history() = turn;
  }

  std::string_view trimmed_command =
      base::TrimWhitespaceASCII(user_command, base::TRIM_ALL);
  if (!trimmed_command.empty()) {
    request.set_user_command(std::string(trimmed_command));
    AddTabGroupChatHistoryTurn(
        optimization_guide::proto::ChatHistoryTurn::ROLE_USER, trimmed_command);
  }

  optimization_guide_remote_model_executor_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kContextHub, request,
      optimization_guide::ModelExecutionOptions(),
      base::BindOnce(&ContextHubService::HandleTabGroupModelExecutionResult,
                     weak_factory_.GetWeakPtr(), std::move(tabs),
                     std::move(callback)));
}

void ContextHubService::ExecuteMemoryBankChat(
    base::span<const int64_t> entry_ids,
    const std::string& user_command,
    MemoryBankChatCallback callback) {
  std::string_view trimmed_command =
      base::TrimWhitespaceASCII(user_command, base::TRIM_ALL);
  if (trimmed_command.empty()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  GetEntriesByIds(
      entry_ids,
      base::BindOnce(&ContextHubService::OnMemoryBankEntriesFetched,
                     weak_factory_.GetWeakPtr(), std::string(trimmed_command),
                     std::move(callback)));
}

void ContextHubService::OnMemoryBankEntriesFetched(
    const std::string& user_command,
    MemoryBankChatCallback callback,
    std::vector<MemoryBankEntry> entries) {
  optimization_guide::proto::ContextHubRequest request;
  request.set_request_type(
      optimization_guide::proto::CONTEXT_HUB_REQUEST_TYPE_MEMORY_BANK_CHAT);

  for (const MemoryBankEntry& entry : entries) {
    *request.add_entry_items()->mutable_memory_bank_entry() =
        ToMemoryBankEntryProto(entry);
  }

  request.set_user_command(user_command);

  optimization_guide_remote_model_executor_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kContextHub, request,
      optimization_guide::ModelExecutionOptions(),
      base::BindOnce(
          &ContextHubService::HandleMemoryBankChatModelExecutionResult,
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ContextHubService::HandleMemoryBankChatModelExecutionResult(
    MemoryBankChatCallback callback,
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  std::optional<optimization_guide::proto::ContextHubResponse> response;
  if (result.response.has_value()) {
    response = optimization_guide::ParsedAnyMetadata<
        optimization_guide::proto::ContextHubResponse>(*result.response);
  }
  if (!response || !response->has_memory_bank_chat_response()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::move(callback).Run(
      response->memory_bank_chat_response().text_response());
}

void ContextHubService::HandleTabGroupModelExecutionResult(
    std::vector<TabData> tabs,
    GroupTabsCallback callback,
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  std::optional<optimization_guide::proto::ContextHubResponse> response;
  if (result.response.has_value()) {
    response = optimization_guide::ParsedAnyMetadata<
        optimization_guide::proto::ContextHubResponse>(*result.response);
  }
  if (!response || !response->has_group_response()) {
    std::move(callback).Run({}, std::move(tabs), /*text_response=*/"");
    return;
  }

  std::string text_response = response->group_response().text_response();
  if (!text_response.empty()) {
    AddTabGroupChatHistoryTurn(
        optimization_guide::proto::ChatHistoryTurn::ROLE_ASSISTANT,
        text_response);
  }

  std::vector<TabGroupEntry> groups;

  base::flat_map<int64_t, size_t> tab_index_map;
  for (size_t i = 0; i < tabs.size(); ++i) {
    tab_index_map.emplace(tabs[i].id, i);
  }

  for (const optimization_guide::proto::TabGroupMinimal& group_proto :
       response->group_response().minimal_tab_groups()) {
    std::vector<int64_t> valid_tab_ids;
    for (int64_t tab_id : group_proto.tab_ids()) {
      if (tab_index_map.contains(tab_id) &&
          std::ranges::find(valid_tab_ids, tab_id) == valid_tab_ids.end()) {
        valid_tab_ids.push_back(tab_id);
      }
    }

    if (valid_tab_ids.size() >= 2) {
      TabGroupEntry entry;
      entry.label = group_proto.label();
      entry.created_timestamp = base::Time::Now();
      entry.last_accessed_timestamp = entry.created_timestamp;
      for (int64_t tab_id : valid_tab_ids) {
        entry.tab_ids.push_back(tab_id);
        if (tab_index_map.contains(tab_id)) {
          size_t index = tab_index_map.at(tab_id);
          entry.tabs.push_back(std::move(tabs[index]));
          tab_index_map.erase(tab_id);
        }
      }
      groups.push_back(std::move(entry));
    }
  }

  if (tab_group_store_) {
    tab_group_store_->DeleteAllGroups(base::BindOnce(
        [](base::WeakPtr<ContextHubService> self,
           std::vector<TabGroupEntry> groups) {
          if (self && self->tab_group_store_) {
            self->tab_group_store_->AddAllGroups(std::move(groups),
                                                 base::DoNothing());
          }
        },
        weak_factory_.GetWeakPtr(), groups));
  }

  std::vector<TabData> ungrouped_tabs;
  for (context_hub::TabData& tab : tabs) {
    if (tab_index_map.contains(tab.id)) {
      ungrouped_tabs.push_back(std::move(tab));
    }
  }

  std::move(callback).Run(std::move(groups), std::move(ungrouped_tabs),
                          std::move(text_response));
}

void ContextHubService::GroupTabs(std::vector<TabData> tabs,
                                  const std::string& user_command,
                                  GroupTabsCallback callback) {
  GenerateTabGroups(std::move(tabs), user_command, std::move(callback));
}

}  // namespace context_hub
