// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tool_request_variant.h"

namespace actor {

ConvertToVariantFn::ConvertToVariantFn() = default;
ConvertToVariantFn::~ConvertToVariantFn() = default;

void ConvertToVariantFn::Apply(const ActivateTabToolRequest& tr) {
  var_ = ToolRequestVariant(tr);
}
void ConvertToVariantFn::Apply(const ActivateWindowToolRequest& tr) {
  var_ = ToolRequestVariant(tr);
}
void ConvertToVariantFn::Apply(const AttemptLoginToolRequest& tr) {
  var_ = ToolRequestVariant(tr);
}
void ConvertToVariantFn::Apply(const AttemptFormFillingToolRequest& tr) {
  var_ = ToolRequestVariant(tr);
}
void ConvertToVariantFn::Apply(const ClickToolRequest& tr) {
  var_ = ToolRequestVariant(tr);
}
void ConvertToVariantFn::Apply(const CloseTabToolRequest& tr) {
  var_ = ToolRequestVariant(tr);
}
void ConvertToVariantFn::Apply(const CloseWindowToolRequest& tr) {
  var_ = ToolRequestVariant(tr);
}
void ConvertToVariantFn::Apply(const CreateTabToolRequest& tr) {
  var_ = ToolRequestVariant(tr);
}
void ConvertToVariantFn::Apply(const CreateWindowToolRequest& tr) {
  var_ = ToolRequestVariant(tr);
}
void ConvertToVariantFn::Apply(const DragAndReleaseToolRequest& tr) {
  var_ = ToolRequestVariant(tr);
}
void ConvertToVariantFn::Apply(const HistoryToolRequest& tr) {
  var_ = ToolRequestVariant(tr);
}
void ConvertToVariantFn::Apply(const MediaControlToolRequest& tr) {
  var_ = ToolRequestVariant(tr);
}
void ConvertToVariantFn::Apply(const MoveMouseToolRequest& tr) {
  var_ = ToolRequestVariant(tr);
}
void ConvertToVariantFn::Apply(const NavigateToolRequest& tr) {
  var_ = ToolRequestVariant(tr);
}
void ConvertToVariantFn::Apply(const ScriptToolRequest& tr) {
  var_ = ToolRequestVariant(tr);
}
void ConvertToVariantFn::Apply(const ScrollToolRequest& tr) {
  var_ = ToolRequestVariant(tr);
}
void ConvertToVariantFn::Apply(const ScrollToToolRequest& tr) {
  var_ = ToolRequestVariant(tr);
}
void ConvertToVariantFn::Apply(const SelectToolRequest& tr) {
  var_ = ToolRequestVariant(tr);
}
void ConvertToVariantFn::Apply(const TypeToolRequest& tr) {
  var_ = ToolRequestVariant(tr);
}
void ConvertToVariantFn::Apply(const WaitToolRequest& tr) {
  var_ = ToolRequestVariant(tr);
}

}  // namespace actor
