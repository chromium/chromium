// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/event_dispatcher.h"

#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

#include "base/check_deref.h"
#include "base/containers/circular_deque.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/actor/tool_request_variant.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/ui/actor_ui_metrics.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/actor/ui/ui_event.h"
#include "chrome/browser/actor/ui/ui_event_debugstring.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/action_result.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_context.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace actor::ui {

// Constructs a MouseMove that may have a computed_target.
AsyncUiEvent ComputedMouseMove(tabs::TabInterface::Handle tab,
                               const PageTarget& target);
namespace {

using ::actor::mojom::ActionResultCode;
using ::actor::mojom::ActionResultPtr;

template <typename T>
struct is_variant_t : std::false_type {};

template <typename... Args>
struct is_variant_t<std::variant<Args...>> : std::true_type {};

template <typename T>
inline constexpr bool is_variant = is_variant_t<T>::value;

template <typename T>
using EventSequence = base::circular_deque<T>;

template <typename T>
auto NoUiEvents = [](const T& tr) -> EventSequence<AsyncUiEvent> {
  return EventSequence<AsyncUiEvent>();
};

constexpr absl::Overload PreToolEventsFn{
    [](const ClickToolRequest& tr) {
      return EventSequence<AsyncUiEvent>{
          ComputedMouseMove(tr.GetTabHandle(), tr.GetTarget()),
          MouseClick(tr.GetTabHandle(), tr.GetClickType(), tr.GetClickCount())};
    },
    NoUiEvents<ActivateTabToolRequest>,
    NoUiEvents<ActivateWindowToolRequest>,
    NoUiEvents<CloseTabToolRequest>,
    NoUiEvents<CloseWindowToolRequest>,
    NoUiEvents<CreateTabToolRequest>,
    NoUiEvents<CreateWindowToolRequest>,
    NoUiEvents<DragAndReleaseToolRequest>,
    NoUiEvents<HistoryToolRequest>,
    NoUiEvents<MediaControlToolRequest>,
    [](const MoveMouseToolRequest& tr) {
      return EventSequence<AsyncUiEvent>{
          ComputedMouseMove(tr.GetTabHandle(), tr.GetTarget())};
    },
    NoUiEvents<NavigateToolRequest>,
    NoUiEvents<ScrollToolRequest>,
    NoUiEvents<SelectToolRequest>,
    [](const TypeToolRequest& tr) {
      return EventSequence<AsyncUiEvent>{
          ComputedMouseMove(tr.GetTabHandle(), tr.GetTarget())};
    },
    NoUiEvents<WaitToolRequest>,
    NoUiEvents<AttemptLoginToolRequest>,
    NoUiEvents<AttemptFormFillingToolRequest>,
    NoUiEvents<ScriptToolRequest>,
    NoUiEvents<ScrollToToolRequest>};

constexpr absl::Overload PostToolEventsFn{
    NoUiEvents<ClickToolRequest>,
    NoUiEvents<ActivateTabToolRequest>,
    NoUiEvents<ActivateWindowToolRequest>,
    NoUiEvents<CloseTabToolRequest>,
    NoUiEvents<CloseWindowToolRequest>,
    NoUiEvents<CreateTabToolRequest>,
    NoUiEvents<CreateWindowToolRequest>,
    NoUiEvents<DragAndReleaseToolRequest>,
    NoUiEvents<HistoryToolRequest>,
    NoUiEvents<MediaControlToolRequest>,
    NoUiEvents<MoveMouseToolRequest>,
    NoUiEvents<NavigateToolRequest>,
    NoUiEvents<ScrollToolRequest>,
    NoUiEvents<SelectToolRequest>,
    NoUiEvents<TypeToolRequest>,
    NoUiEvents<WaitToolRequest>,
    NoUiEvents<AttemptLoginToolRequest>,
    NoUiEvents<AttemptFormFillingToolRequest>,
    NoUiEvents<ScriptToolRequest>,
    NoUiEvents<ScrollToToolRequest>};

constexpr absl::Overload ActorTaskAsyncChangeFn{
    [](const UiEventDispatcher::AddTab& c) {
      return EventSequence<AsyncUiEvent>{
          StartingToActOnTab(c.handle, c.task_id)};
    },
};

constexpr absl::Overload ActorTaskSyncChangeFn{
    [](const UiEventDispatcher::ChangeTaskState& c) {
      auto seq = EventSequence<SyncUiEvent>{};
      if (c.old_state == ActorTask::State::kCreated &&
          c.new_state == ActorTask::State::kActing) {
        seq.push_back(StartTask(c.task_id));
      }
      seq.push_back(TaskStateChanged(c.task_id, c.new_state, c.title));
      return seq;
    },
    [](const UiEventDispatcher::RemoveTab& c) {
      return EventSequence<SyncUiEvent>{StoppedActingOnTab(c.handle)};
    },
};

template <absl::Overload V>
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
struct VisitorTraits<ActorTaskAsyncChangeFn> {
  static constexpr const char* phase_name = "ActorTaskAsyncChange";
};

template <>
struct VisitorTraits<ActorTaskSyncChangeFn> {
  static constexpr const char* phase_name = "ActorTaskSyncChange";
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
struct InputTraits<UiEventDispatcher::ActorTaskAsyncChange> {
  static constexpr const char* name = "ActorTaskAsyncChange";
  static constexpr auto convert_fn = std::identity();
  static constexpr auto debug_info =
      [](const UiEventDispatcher::ActorTaskAsyncChange& change) {
        constexpr absl::Overload DebugFn{
            [](const UiEventDispatcher::AddTab& c) {
              return absl::StrFormat("AddTab task_id=%d tab=%d",
                                     c.task_id.GetUnsafeValue(),
                                     c.handle.raw_value());
            }};
        return std::visit(DebugFn, change);
      };
};

template <>
struct InputTraits<UiEventDispatcher::ActorTaskSyncChange> {
  static constexpr const char* name = "ActorTaskSyncChange";
  static constexpr auto convert_fn = std::identity();
  static constexpr auto debug_info =
      [](const UiEventDispatcher::ActorTaskSyncChange& change) {
        constexpr absl::Overload DebugFn{
            [](const UiEventDispatcher::ChangeTaskState& c) {
              return absl::StrFormat(
                  "ChangeTaskState task_id=%d old_state=%s new_state=%s",
                  c.task_id.GetUnsafeValue(), ToString(c.old_state),
                  ToString(c.new_state));
            },
            [](const UiEventDispatcher::RemoveTab& c) {
              return absl::StrFormat("RemoveTab task_id=%d tab=%d",
                                     c.task_id.GetUnsafeValue(),
                                     c.handle.raw_value());
            }};
        return std::visit(DebugFn, change);
      };
};

class UiEventDispatcherImpl : public UiEventDispatcher {
 public:
  explicit UiEventDispatcherImpl(ActorUiStateManagerInterface& ui_state_manager)
      : ui_state_manager_(ui_state_manager) {}
  ~UiEventDispatcherImpl() override = default;

  void OnPreTool(const ToolRequest& tr, UiCompleteCallback callback) override {
    On<PreToolEventsFn>(tr, std::move(callback));
  }

  void OnPostTool(const ToolRequest& tr, UiCompleteCallback callback) override {
    On<PostToolEventsFn>(tr, std::move(callback));
  }

  void OnActorTaskAsyncChange(const ActorTaskAsyncChange& change,
                              UiCompleteCallback callback) override {
    On<ActorTaskAsyncChangeFn>(change, std::move(callback));
  }

  void OnActorTaskSyncChange(const ActorTaskSyncChange& change) override {
    On<ActorTaskSyncChangeFn>(change);
  }

 private:
  raw_ref<ActorUiStateManagerInterface> ui_state_manager_;
  std::variant<EventSequence<AsyncUiEvent>, EventSequence<SyncUiEvent>> events_;
  UiCompleteCallback overall_callback_;
  base::WeakPtrFactory<UiEventDispatcherImpl> weak_ptr_factory_{this};

  void ResetAndComplete(ActionResultPtr result) {
    TRACE_EVENT_END("actor");
    weak_ptr_factory_.InvalidateWeakPtrs();
    std::visit([]<typename T>(EventSequence<T>& e) { return e.clear(); },
               events_);
    if (!overall_callback_.is_null()) {
      std::move(overall_callback_).Run(std::move(result));
    } else {
      if (result->code != ActionResultCode::kOk) {
        LOG(DFATAL) << ToDebugString(*result);
      }
    }
  }

  // Takes async path.
  template <absl::Overload V, typename InputT>
  void On(const InputT& in, UiCompleteCallback callback) {
    VLOG(4) << VisitorTraits<V>::phase_name << "(" << InputTraits<InputT>::name
            << "): " << InputTraits<InputT>::debug_info(in);
    GenerateAndSend<V, AsyncUiEvent>(InputTraits<InputT>::convert_fn(in),
                                     std::move(callback));
  }

  // Takes synchronous path.
  template <absl::Overload V, typename InputT>
  void On(const InputT& in) {
    VLOG(4) << VisitorTraits<V>::phase_name << "(" << InputTraits<InputT>::name
            << "): " << InputTraits<InputT>::debug_info(in);
    GenerateAndSend<V, SyncUiEvent>(InputTraits<InputT>::convert_fn(in),
                                    UiCompleteCallback() /*=null*/);
  }

  template <absl::Overload V, typename EventT, typename ConvertedInputT>
  void GenerateAndSend(const ConvertedInputT& converted,
                       UiCompleteCallback callback) {
    TRACE_EVENT_BEGIN("actor", "UiEventDispatch");
    CHECK(std::visit([]<typename T>(EventSequence<T>& e) { return e.empty(); },
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
  template <absl::Overload V>
  void MaybeSendNextEvent(ActionResultPtr result) {
    TRACE_EVENT_BEGIN("actor", "MaybeSendNextEvent");
    if (result->code != ActionResultCode::kOk) {
      VLOG(4) << VisitorTraits<V>::phase_name
              << " UI actuation failed: " << ToDebugString(*result);
      TRACE_EVENT_END("actor");
      ResetAndComplete(std::move(result));
      return;
    }
    auto& events = std::get<EventSequence<AsyncUiEvent>>(events_);
    if (events.empty()) {
      TRACE_EVENT_END("actor");
      ResetAndComplete(MakeOkResult());
      return;
    }

    const AsyncUiEvent event = std::move(events.front());
    events.pop_front();
    VLOG(4) << VisitorTraits<V>::phase_name
            << "(AsyncUiEvent): " << DebugString(event);

    // Create a callback that records metrics based on the result.
    base::OnceCallback<ActionResultPtr(ActionResultPtr)> record_metrics =
        base::BindOnce(
            [](std::string_view event_name, base::TimeTicks start_time,
               ActionResultPtr result) {
              if (result->code == ActionResultCode::kOk) {
                RecordUiEventDuration(event_name,
                                      base::TimeTicks::Now() - start_time);
              } else {
                RecordUiEventFailure(event_name);
              }
              // Pass the result along to the next step.
              return result;
            },
            GetUiEventName(event), base::TimeTicks::Now());

    TRACE_EVENT_END("actor");
    ui_state_manager_->OnUiEvent(
        std::move(event),
        std::move(record_metrics)
            .Then(base::BindOnce(&UiEventDispatcherImpl::MaybeSendNextEvent<V>,
                                 weak_ptr_factory_.GetWeakPtr())));
  }

  // Synchronously send events.
  template <absl::Overload V>
  void SendAllEvents() {
    TRACE_EVENT("actor", "SendAllEvents");
    auto& events = std::get<EventSequence<SyncUiEvent>>(events_);
    while (!events.empty()) {
      const SyncUiEvent event = std::move(events.front());
      events.pop_front();
      VLOG(4) << VisitorTraits<V>::phase_name
              << "(SyncUiEvent): " << DebugString(event);

      base::ScopedUmaHistogramTimer timer(
          GetUiEventDurationHistogramName(GetUiEventName(event)),
          base::ScopedUmaHistogramTimer::ScopedHistogramTiming::
              kMicrosecondTimes);
      ui_state_manager_->OnUiEvent(std::move(event));
    }
    ResetAndComplete(MakeOkResult());
  }
};
}  // namespace

std::unique_ptr<UiEventDispatcher> NewUiEventDispatcher(
    ActorUiStateManagerInterface* ui_state_manager) {
  return std::make_unique<UiEventDispatcherImpl>(CHECK_DEREF(ui_state_manager));
}
}  // namespace actor::ui
