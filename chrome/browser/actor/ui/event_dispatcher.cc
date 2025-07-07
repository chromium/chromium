// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/event_dispatcher.h"

#include <memory>
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
auto NoUiEvents =
    [](const T& tr) -> std::deque<UiEvent> { return std::deque<UiEvent>(); };

constexpr Visitor PreToolEventsFn{
    [](const ClickToolRequest& tr) {
      return std::deque<UiEvent>{
          MouseMove(tr.GetTabHandle(), tr.GetTarget()),
          MouseClick(tr.GetTabHandle(), tr.GetClickType(), tr.GetClickCount())};
    },
    NoUiEvents<ActivateTabToolRequest>,
    NoUiEvents<CloseTabToolRequest>,
    NoUiEvents<CreateTabToolRequest>,
    NoUiEvents<DragAndReleaseToolRequest>,
    NoUiEvents<HistoryToolRequest>,
    [](const MoveMouseToolRequest& tr) {
      return std::deque<UiEvent>{MouseMove(tr.GetTabHandle(), tr.GetTarget())};
    },
    NoUiEvents<NavigateToolRequest>,
    NoUiEvents<ScrollToolRequest>,
    NoUiEvents<SelectToolRequest>,
    NoUiEvents<TypeToolRequest>,
    NoUiEvents<WaitToolRequest>};

constexpr Visitor PostToolEventsFn{
    NoUiEvents<ClickToolRequest>,          NoUiEvents<ActivateTabToolRequest>,
    NoUiEvents<CloseTabToolRequest>,       NoUiEvents<CreateTabToolRequest>,
    NoUiEvents<DragAndReleaseToolRequest>, NoUiEvents<HistoryToolRequest>,
    NoUiEvents<MoveMouseToolRequest>,      NoUiEvents<NavigateToolRequest>,
    NoUiEvents<ScrollToolRequest>,         NoUiEvents<SelectToolRequest>,
    NoUiEvents<TypeToolRequest>,           NoUiEvents<WaitToolRequest>};

constexpr Visitor FirstActEventsFn{
    [](const UiEventDispatcher::FirstActInfo& info) {
      auto events = std::deque<UiEvent>{StartTask(info.task_id)};
      if (info.tab_handle.has_value()) {
        events.emplace_back(
            StartingToActOnTab(info.tab_handle.value(), info.task_id));
      }
      return events;
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
  UiEventDispatcherImpl() = default;
  ~UiEventDispatcherImpl() override = default;

  void OnPreTool(Profile* profile,
                 const ToolRequest& tr,
                 UiCompleteCallback callback) override {
    On<PreToolEventsFn>(profile, tr, std::move(callback));
  }

  void OnPostTool(Profile* profile,
                  const ToolRequest& tr,
                  UiCompleteCallback callback) override {
    On<PostToolEventsFn>(profile, tr, std::move(callback));
  }

  void OnPreFirstAct(Profile* profile,
                     const FirstActInfo& first_act_info,
                     UiCompleteCallback callback) override {
    On<FirstActEventsFn>(profile, first_act_info, std::move(callback));
  }

 private:
  std::deque<UiEvent> events_;
  UiCompleteCallback overall_callback_;
  raw_ptr<ActorUiStateManagerInterface> ui_state_manager_;
  base::WeakPtrFactory<UiEventDispatcherImpl> weak_ptr_factory_{this};

  void ResetAndComplete(mojom::ActionResultPtr result) {
    weak_ptr_factory_.InvalidateWeakPtrs();
    ui_state_manager_ = nullptr;
    events_.clear();
    std::move(overall_callback_).Run(std::move(result));
  }

  template <Visitor V, typename InputT>
  void On(Profile* profile, const InputT& in, UiCompleteCallback callback) {
    VLOG(4) << VisitorTraits<V>::phase_name << "(" << InputTraits<InputT>::name
            << "): " << InputTraits<InputT>::debug_info(in);
    GenerateAndSend<V>(profile, InputTraits<InputT>::convert_fn(in),
                       std::move(callback));
  }

  template <Visitor V, typename ConvertedInputT>
  void GenerateAndSend(Profile* profile,
                       const ConvertedInputT& converted,
                       UiCompleteCallback callback) {
    CHECK(events_.empty()) << "Unexpected: unprocessed UiEvents remaining";
    auto result = GetUiStateManager(profile);
    if (std::holds_alternative<mojom::ActionResultPtr>(result)) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback),
                         std::move(std::get<mojom::ActionResultPtr>(result))));
      return;
    }
    ui_state_manager_ = std::get<ActorUiStateManagerInterface*>(result);

    // Visit converted type to generate UiEvent sequence.
    if constexpr (is_variant<ConvertedInputT>) {
      events_ = std::visit(V, converted);
    } else {
      events_ = V(converted);
    }
    overall_callback_ = std::move(callback);
    MaybeSendNextEvent<V>(MakeOkResult());
  }

  // Callback after each event is processed by ActorUiStateManager.
  template <Visitor V>
  void MaybeSendNextEvent(mojom::ActionResultPtr result) {
    if (result->code != mojom::ActionResultCode::kOk) {
      VLOG(4) << VisitorTraits<V>::phase_name
              << " UI actuation failed: " << ToDebugString(*result);
      ResetAndComplete(std::move(result));
      return;
    }
    if (events_.empty()) {
      ResetAndComplete(MakeOkResult());
      return;
    }

    const UiEvent event = std::move(events_.front());
    events_.pop_front();
    VLOG(4) << VisitorTraits<V>::phase_name
            << "(UiEvent): " << DebugString(event);
    ui_state_manager_->OnUiEvent(
        std::move(event),
        base::BindOnce(&UiEventDispatcherImpl::MaybeSendNextEvent<V>,
                       weak_ptr_factory_.GetWeakPtr()));
  }
};
}  // namespace

std::unique_ptr<UiEventDispatcher> NewUiEventDispatcher() {
  return std::make_unique<UiEventDispatcherImpl>();
}
}  // namespace actor::ui
