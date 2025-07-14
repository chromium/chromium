// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/event_dispatcher.h"

#include <memory>
#include <type_traits>
#include <variant>

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/actor/ui/tool_request_variant.h"
#include "chrome/browser/actor/ui/ui_event.h"
#include "chrome/browser/actor/ui/ui_event_debugstring.h"
#include "chrome/browser/actor/variant_visitor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor/action_result.h"
#include "content/public/browser/browser_context.h"

namespace actor::ui {
namespace {
template <typename T>
struct is_variant_t : std::false_type {};

template <typename... Args>
struct is_variant_t<std::variant<Args...>> : std::true_type {};

template <typename T>
inline constexpr bool is_variant = is_variant_t<T>::value;

template <typename T>
auto NoUiEvents = [](const T& tr) -> std::deque<AsyncUiEvent> {
  return std::deque<AsyncUiEvent>();
};

constexpr Visitor PreToolEventsFn{
    [](const ClickToolRequest& tr) {
      return std::deque<AsyncUiEvent>{
          MouseMove(tr.GetTabHandle(), tr.GetTarget()),
          MouseClick(tr.GetTabHandle(), tr.GetClickType(), tr.GetClickCount())};
    },
    NoUiEvents<ActivateTabToolRequest>,
    NoUiEvents<CloseTabToolRequest>,
    NoUiEvents<CreateTabToolRequest>,
    NoUiEvents<DragAndReleaseToolRequest>,
    NoUiEvents<HistoryToolRequest>,
    [](const MoveMouseToolRequest& tr) {
      return std::deque<AsyncUiEvent>{
          MouseMove(tr.GetTabHandle(), tr.GetTarget())};
    },
    NoUiEvents<NavigateToolRequest>,
    NoUiEvents<ScrollToolRequest>,
    NoUiEvents<SelectToolRequest>,
    NoUiEvents<TypeToolRequest>,
    NoUiEvents<WaitToolRequest>,
    NoUiEvents<AttemptLoginToolRequest>};

constexpr Visitor PostToolEventsFn{
    NoUiEvents<ClickToolRequest>,          NoUiEvents<ActivateTabToolRequest>,
    NoUiEvents<CloseTabToolRequest>,       NoUiEvents<CreateTabToolRequest>,
    NoUiEvents<DragAndReleaseToolRequest>, NoUiEvents<HistoryToolRequest>,
    NoUiEvents<MoveMouseToolRequest>,      NoUiEvents<NavigateToolRequest>,
    NoUiEvents<ScrollToolRequest>,         NoUiEvents<SelectToolRequest>,
    NoUiEvents<TypeToolRequest>,           NoUiEvents<WaitToolRequest>,
    NoUiEvents<AttemptLoginToolRequest>};

// TODO(crbug.com/425784083): Remove FirstActEventsFn once functionality moves
// to ActorTaskChangeFn.
constexpr Visitor FirstActEventsFn{
    [](const UiEventDispatcher::FirstActInfo& info) {
      auto events = std::deque<AsyncUiEvent>{StartTask(info.task_id)};
      if (info.tab_handle.has_value()) {
        events.emplace_back(
            StartingToActOnTab(info.tab_handle.value(), info.task_id));
      }
      return events;
    },
};

constexpr Visitor ActorTaskChangeFn{
    // TODO(crbug.com/425784083): Add tab changes from ActorTask.
    [](const UiEventDispatcher::ChangeTaskState& c) {
      // TODO(crbug.com/425784083): Dispatch StartTask if state transition is
      // Created -> Acting.
      return std::deque<SyncUiEvent>{TaskStateChanged(c.task_id, c.new_state)};
    },
};

template <Visitor V>
struct VisitorTraits {};

template <>
struct VisitorTraits<PreToolEventsFn> {
  static constexpr const char* phase_name = "PreTool";
};

template <>
struct VisitorTraits<PostToolEventsFn> {
  static constexpr const char* phase_name = "PostTool";
};

template <>
struct VisitorTraits<FirstActEventsFn> {
  static constexpr const char* phase_name = "FirstAct";
};

template <>
struct VisitorTraits<ActorTaskChangeFn> {
  static constexpr const char* phase_name = "ActorTaskChange";
};

template <typename T>
struct InputTraits {};

template <>
struct InputTraits<ToolRequest> {
  static constexpr const char* name = "ToolRequest";
  static constexpr auto convert_fn =
      [](const ToolRequest& tr) -> ToolRequestVariant {
    ConvertToVariantFn fn;
    tr.Apply(fn);
    return fn.GetVariant().value();
  };
  static constexpr auto debug_info = [](const ToolRequest& tr) {
    return tr.JournalEvent();
  };
};

template <>
struct InputTraits<UiEventDispatcher::FirstActInfo> {
  static constexpr const char* name = "FirstActInfo";
  static constexpr auto convert_fn = std::identity();
  static constexpr auto debug_info =
      [](const UiEventDispatcher::FirstActInfo& info) {
        return absl::StrFormat("task_id=%d tab? %s",
                               info.task_id.GetUnsafeValue(),
                               info.tab_handle.has_value() ? "yes" : "no");
      };
};

template <>
struct InputTraits<UiEventDispatcher::ActorTaskChange> {
  static constexpr const char* name = "ActorTaskChange";
  static constexpr auto convert_fn = std::identity();
  static constexpr auto debug_info =
      [](const UiEventDispatcher::ActorTaskChange& change) {
        constexpr Visitor DebugFn{
            [](const UiEventDispatcher::ChangeTaskState& c) {
              return absl::StrFormat(
                  "ChangeTaskState task_id=%d old_state=%s new_state=%s",
                  c.task_id.GetUnsafeValue(), ToString(c.old_state),
                  ToString(c.new_state));
            }};
        return std::visit(DebugFn, change);
      };
};

std::variant<mojom::ActionResultPtr, ActorUiStateManagerInterface*>
GetUiStateManager(Profile* profile) {
  ActorKeyedService* actor_service = ActorKeyedService::Get(profile);
  if (!actor_service) {
    return MakeResult(mojom::ActionResultCode::kError,
                      base::StrCat({"No ActorKeyedService found for profile ",
                                    profile->GetDebugName()}));
  }

  ActorUiStateManagerInterface* state_manager =
      actor_service->GetActorUiStateManager();
  if (!state_manager) {
    return MakeResult(mojom::ActionResultCode::kError,
                      base::StrCat({"No ActorUiStateManager found for profile ",
                                    profile->GetDebugName()}));
  }
  return state_manager;
}

class UiEventDispatcherImpl : public UiEventDispatcher {
 public:
  explicit UiEventDispatcherImpl(Profile* profile) : profile_(profile) {}
  ~UiEventDispatcherImpl() override = default;

  void OnPreTool(const ToolRequest& tr, UiCompleteCallback callback) override {
    On<PreToolEventsFn>(tr, std::move(callback));
  }

  void OnPostTool(const ToolRequest& tr, UiCompleteCallback callback) override {
    On<PostToolEventsFn>(tr, std::move(callback));
  }

  void OnPreFirstAct(const FirstActInfo& first_act_info,
                     UiCompleteCallback callback) override {
    On<FirstActEventsFn>(first_act_info, std::move(callback));
  }

  void OnActorTaskChange(const ActorTaskChange& change) override {
    On<ActorTaskChangeFn>(change);
  }

 private:
  raw_ptr<Profile> profile_;
  std::variant<std::deque<AsyncUiEvent>, std::deque<SyncUiEvent>> events_;
  UiCompleteCallback overall_callback_;
  raw_ptr<ActorUiStateManagerInterface> ui_state_manager_;
  base::WeakPtrFactory<UiEventDispatcherImpl> weak_ptr_factory_{this};

  void ResetAndComplete(mojom::ActionResultPtr result) {
    weak_ptr_factory_.InvalidateWeakPtrs();
    ui_state_manager_ = nullptr;
    std::visit([]<typename T>(std::deque<T>& e) { return e.clear(); }, events_);
    if (!overall_callback_.is_null()) {
      std::move(overall_callback_).Run(std::move(result));
    } else {
      if (result->code != mojom::ActionResultCode::kOk) {
        LOG(DFATAL) << ToDebugString(*result);
      }
    }
  }

  // Takes async path.
  template <Visitor V, typename InputT>
  void On(const InputT& in, UiCompleteCallback callback) {
    VLOG(4) << VisitorTraits<V>::phase_name << "(" << InputTraits<InputT>::name
            << "): " << InputTraits<InputT>::debug_info(in);
    GenerateAndSend<V, AsyncUiEvent>(InputTraits<InputT>::convert_fn(in),
                                     std::move(callback));
  }

  // Takes synchronous path.
  template <Visitor V, typename InputT>
  void On(const InputT& in) {
    VLOG(4) << VisitorTraits<V>::phase_name << "(" << InputTraits<InputT>::name
            << "): " << InputTraits<InputT>::debug_info(in);
    GenerateAndSend<V, SyncUiEvent>(InputTraits<InputT>::convert_fn(in),
                                    UiCompleteCallback() /*=null*/);
  }

  template <Visitor V, typename EventT, typename ConvertedInputT>
  void GenerateAndSend(const ConvertedInputT& converted,
                       UiCompleteCallback callback) {
    CHECK(std::visit([]<typename T>(std::deque<T>& e) { return e.empty(); },
                     events_))
        << "Unexpected: unprocessed UiEvents remaining";
    if constexpr (std::is_same_v<EventT, AsyncUiEvent>) {
      CHECK(!callback.is_null()) << "Callback not defined for AsyncUiEvent";
      overall_callback_ = std::move(callback);
    } else if constexpr (std::is_same_v<EventT, SyncUiEvent>) {
      CHECK(callback.is_null()) << "Callback defined for SyncUiEvent";
    } else {
      static_assert(false, "Unknown type!");
    }

    auto result = GetUiStateManager(profile_);
    if (std::holds_alternative<mojom::ActionResultPtr>(result)) {
      auto& result_v = std::get<mojom::ActionResultPtr>(result);
      ResetAndComplete(std::move(result_v));
      return;
    }
    ui_state_manager_ = std::get<ActorUiStateManagerInterface*>(result);

    // Visit converted type to generate UiEvent sequence.
    if constexpr (is_variant<ConvertedInputT>) {
      events_ = std::visit(V, converted);
    } else {
      events_ = V(converted);
    }
    // Send events either asynchronously or synchronously.
    if constexpr (std::is_same_v<EventT, AsyncUiEvent>) {
      MaybeSendNextEvent<V>(MakeOkResult());
    } else if constexpr (std::is_same_v<EventT, SyncUiEvent>) {
      SendAllEvents<V>();
    }
  }

  // Asynchronously send events.  Called back after each event is processed
  // by ActorUiStateManager.
  template <Visitor V>
  void MaybeSendNextEvent(mojom::ActionResultPtr result) {
    if (result->code != mojom::ActionResultCode::kOk) {
      VLOG(4) << VisitorTraits<V>::phase_name
              << " UI actuation failed: " << ToDebugString(*result);
      ResetAndComplete(std::move(result));
      return;
    }
    auto& events = std::get<std::deque<AsyncUiEvent>>(events_);
    if (events.empty()) {
      ResetAndComplete(MakeOkResult());
      return;
    }

    const AsyncUiEvent event = std::move(events.front());
    events.pop_front();
    VLOG(4) << VisitorTraits<V>::phase_name
            << "(AsyncUiEvent): " << DebugString(event);
    ui_state_manager_->OnUiEvent(
        std::move(event),
        base::BindOnce(&UiEventDispatcherImpl::MaybeSendNextEvent<V>,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Synchronously send events.
  template <Visitor V>
  void SendAllEvents() {
    auto& events = std::get<std::deque<SyncUiEvent>>(events_);
    while (!events.empty()) {
      const SyncUiEvent event = std::move(events.front());
      events.pop_front();
      VLOG(4) << VisitorTraits<V>::phase_name
              << "(SyncUiEvent): " << DebugString(event);
      ui_state_manager_->OnUiEvent(std::move(event));
    }
    ResetAndComplete(MakeOkResult());
  }
};
}  // namespace

std::unique_ptr<UiEventDispatcher> NewUiEventDispatcher(Profile* profile) {
  return std::make_unique<UiEventDispatcherImpl>(profile);
}
}  // namespace actor::ui
