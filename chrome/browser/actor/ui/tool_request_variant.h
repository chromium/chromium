// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_TOOL_REQUEST_VARIANT_H_
#define CHROME_BROWSER_ACTOR_UI_TOOL_REQUEST_VARIANT_H_

#include <variant>

#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"

namespace actor::ui {

// Type safe union of ToolRequest types.
using ToolRequestVariant = std::variant<ActivateTabToolRequest,
                                        AttemptLoginToolRequest,
                                        ClickToolRequest,
                                        CloseTabToolRequest,
                                        CreateTabToolRequest,
                                        DragAndReleaseToolRequest,
                                        HistoryToolRequest,
                                        MoveMouseToolRequest,
                                        NavigateToolRequest,
                                        ScriptToolRequest,
                                        ScrollToolRequest,
                                        SelectToolRequest,
                                        TypeToolRequest,
                                        WaitToolRequest>;

// Functor for converting a polymorphic ToolRequest object to the proper
// ToolRequestVariant type.
class ConvertToVariantFn : public ToolRequestVisitorFunctor {
 public:
  ConvertToVariantFn();
  ~ConvertToVariantFn();
  void Apply(const ActivateTabToolRequest&) override;
  void Apply(const AttemptLoginToolRequest&) override;
  void Apply(const ClickToolRequest&) override;
  void Apply(const CloseTabToolRequest&) override;
  void Apply(const CreateTabToolRequest&) override;
  void Apply(const DragAndReleaseToolRequest&) override;
  void Apply(const HistoryToolRequest&) override;
  void Apply(const MoveMouseToolRequest&) override;
  void Apply(const NavigateToolRequest&) override;
  void Apply(const ScriptToolRequest&) override;
  void Apply(const ScrollToolRequest&) override;
  void Apply(const SelectToolRequest&) override;
  void Apply(const TypeToolRequest&) override;
  void Apply(const WaitToolRequest&) override;

  const std::optional<ToolRequestVariant>& GetVariant() const { return var_; }

 private:
  std::optional<ToolRequestVariant> var_;
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_TOOL_REQUEST_VARIANT_H_
