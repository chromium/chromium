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
#include "chrome/browser/actor/ui/variant_visitor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor/action_result.h"
#include "content/public/browser/browser_context.h"

namespace actor::ui {
namespace {
// TODO(crbug.com/425784083): evaluate sharing these types between ToolRequests
// and the UI code.
PageTarget ConvertTarget(const PageToolRequest::Target& t) {
  if (t.is_coordinate()) {
    return t.coordinate();
  } else if (t.is_node()) {
    return DomNode{
        .node_id = t.node().dom_node_id,
        .document_identifier = t.node().document_identifier,
    };
  }
  NOTREACHED();
}

MouseClickType ConvertClickType(const ClickToolRequest::ClickType& ct) {
  switch (ct) {
    case ClickToolRequest::ClickType::kLeft:
      return MouseClickType::kLeft;
    case ClickToolRequest::ClickType::kRight:
      return MouseClickType::kRight;
  }
  NOTREACHED();
}

MouseClickCount ConvertClickCount(const ClickToolRequest::ClickCount& ct) {
  switch (ct) {
    case ClickToolRequest::ClickCount::kSingle:
      return MouseClickCount::kSingle;
    case ClickToolRequest::ClickCount::kDouble:
      return MouseClickCount::kDouble;
  }
  NOTREACHED();
}

template <typename T>
auto NoUiEvents =
    [](const T& tr) -> std::deque<UiEvent> { return std::deque<UiEvent>(); };

constexpr Visitor PreToolEventsFn{
    [](const ClickToolRequest& tr) {
      return std::deque<UiEvent>{
          MouseMove(tr.GetTabHandle(), ConvertTarget(tr.GetTarget())),
          MouseClick(tr.GetTabHandle(), ConvertClickType(tr.GetClickType()),
                     ConvertClickCount(tr.GetClickCount()))};
    },
    NoUiEvents<ActivateTabToolRequest>,
    NoUiEvents<CloseTabToolRequest>,
    NoUiEvents<CreateTabToolRequest>,
    NoUiEvents<DragAndReleaseToolRequest>,
    NoUiEvents<HistoryToolRequest>,
    [](const MoveMouseToolRequest& tr) {
      return std::deque<UiEvent>{
          MouseMove(tr.GetTabHandle(), ConvertTarget(tr.GetTarget()))};
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

template <Visitor V>
struct VisitorTraits {
  static constexpr const char* name = "";
};

template <>
struct VisitorTraits<PreToolEventsFn> {
  static constexpr const char* phase_name = "PreTool";
};

template <>
struct VisitorTraits<PostToolEventsFn> {
  static constexpr const char* phase_name = "PostTool";
};

ToolRequestVariant ConvertToolRequestToVariant(const ToolRequest& tr) {
  ConvertToVariantFn fn;
  tr.Apply(fn);
  return fn.GetVariant().value();
}

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

  // TODO(crbug.com/425784083): Add hooks to send StartTask events.

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

  template <Visitor V>
  void On(Profile* profile,
          const ToolRequest& tr,
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

    VLOG(4) << VisitorTraits<V>::phase_name
            << "(ToolRequest): " << tr.JournalEvent();
    auto tool_request = ConvertToolRequestToVariant(tr);
    events_ = std::visit(V, tool_request);
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
