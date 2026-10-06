// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tool_request_variant.h"

#include <optional>
#include <utility>

#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"

namespace actor {

namespace {

// Functor for converting a polymorphic ToolRequest object to the proper
// ToolRequestVariant type.
class ConvertToVariantFn : public ToolRequestVisitorFunctor {
 public:
  ConvertToVariantFn() = default;
  ~ConvertToVariantFn() = default;
#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
  void Apply(const ActivateTabToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
  void Apply(const ActivateWindowToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
#endif
  void Apply(const AddBookmarkToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
  void Apply(const AttemptLoginToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
  void Apply(const AttemptFormFillingToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
  void Apply(const AttemptOtpFillingToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
  void Apply(const ChangePasswordToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
  void Apply(const ClickToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
  void Apply(const CloseTabToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
  void Apply(const CloseWindowToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
  void Apply(const CreateTabToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
  void Apply(const CreateWindowToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
#endif
  void Apply(const DragAndReleaseToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
  void Apply(const EnterFullscreenToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
  void Apply(const ExitFullscreenToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
#endif
  void Apply(const FileUploadToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
  void Apply(const FindAndHighlightToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
  void Apply(const HistoryBackToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
  void Apply(const HistoryForwardToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
  void Apply(const LoadAndExtractContentToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
#endif
  void Apply(const MoveMouseToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
  void Apply(const NavigateToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
  void Apply(const OpenKnownPageToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
#endif
  void Apply(const PauseMediaToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
  void Apply(const PerformSearchToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
  void Apply(const PlayMediaToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
  void Apply(const ReloadPageToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
  void Apply(const RemoveBookmarkToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
  void Apply(const ScriptToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
  void Apply(const ScrollToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
  void Apply(const ScrollToToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
  void Apply(const SeekMediaToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
  void Apply(const SelectToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
  void Apply(const SwitchTabToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
#endif
  void Apply(const TranslatePageToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
  void Apply(const TypeToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }
  void Apply(const WaitToolRequest& tr) override {
    var_ = ToolRequestVariant(tr);
  }

  std::optional<ToolRequestVariant>& GetVariant() { return var_; }

 private:
  std::optional<ToolRequestVariant> var_;
};

}  // namespace

ToolRequestVariant ConvertToVariant(const ToolRequest& tr) {
  ConvertToVariantFn f;
  tr.Apply(f);
  return std::move(f.GetVariant()).value();
}

}  // namespace actor
