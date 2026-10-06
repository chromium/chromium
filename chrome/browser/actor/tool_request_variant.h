// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOL_REQUEST_VARIANT_H_
#define CHROME_BROWSER_ACTOR_TOOL_REQUEST_VARIANT_H_

#include <variant>

#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"

namespace actor {

// LINT.IfChange(ToolRequestVariant)
// Type safe union of ToolRequest types.
using ToolRequestVariant = std::variant<
#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
    ActivateTabToolRequest,
    ActivateWindowToolRequest,
#endif
    AddBookmarkToolRequest,
    AttemptLoginToolRequest,
    AttemptFormFillingToolRequest,
    AttemptOtpFillingToolRequest,
    ChangePasswordToolRequest,
    ClickToolRequest,
#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
    CloseTabToolRequest,
    CloseWindowToolRequest,
    CreateTabToolRequest,
    CreateWindowToolRequest,
#endif
    DragAndReleaseToolRequest,
#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
    EnterFullscreenToolRequest,
    ExitFullscreenToolRequest,
#endif
    FileUploadToolRequest,
    FindAndHighlightToolRequest,
    HistoryBackToolRequest,
    HistoryForwardToolRequest,
#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
    LoadAndExtractContentToolRequest,
#endif
    MoveMouseToolRequest,
    NavigateToolRequest,
#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
    OpenKnownPageToolRequest,
#endif
    PauseMediaToolRequest,
    PerformSearchToolRequest,
    PlayMediaToolRequest,
    ReloadPageToolRequest,
    RemoveBookmarkToolRequest,
    ScriptToolRequest,
    ScrollToolRequest,
    ScrollToToolRequest,
    SeekMediaToolRequest,
    SelectToolRequest,
#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
    SwitchTabToolRequest,
#endif
    TranslatePageToolRequest,
    TypeToolRequest,
    WaitToolRequest>;
// LINT.ThenChange(//tools/metrics/histograms/metadata/actor/histograms.xml:ToolRequest)

// Converts a polymorphic ToolRequest object to the proper ToolRequestVariant
// type.
ToolRequestVariant ConvertToVariant(const ToolRequest& tr);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOL_REQUEST_VARIANT_H_
