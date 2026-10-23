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
using ToolRequestVariant = std::variant<ActivateTabToolRequest,
#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
                                        ActivateWindowToolRequest,
#endif
                                        AddBookmarkToolRequest,
                                        AttemptLoginToolRequest,
                                        AttemptFormFillingToolRequest,
                                        AttemptOtpFillingToolRequest,
                                        ChangePasswordToolRequest,
                                        ClickToolRequest,
                                        CloseGeminiPanelToolRequest,
                                        CloseTabToolRequest,
#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
                                        CloseWindowToolRequest,
#endif
                                        CreateTabToolRequest,
#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
                                        CreateWindowToolRequest,
#endif
                                        DragAndReleaseToolRequest,
                                        EnterFullscreenToolRequest,
                                        ExitFullscreenToolRequest,
                                        FileUploadToolRequest,
                                        FindAndHighlightToolRequest,
                                        HistoryBackToolRequest,
                                        HistoryForwardToolRequest,
#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
                                        LoadAndExtractContentToolRequest,
#endif
                                        MoveMouseToolRequest,
                                        NavigateToolRequest,
                                        OpenKnownPageToolRequest,
                                        OpenUrlToolRequest,
                                        PauseMediaToolRequest,
                                        PerformSearchToolRequest,
                                        PlayMediaToolRequest,
                                        PromptGeminiPanelToolRequest,
                                        ReloadPageToolRequest,
                                        RemoveBookmarkToolRequest,
                                        ScriptToolRequest,
                                        ScrollPageToolRequest,
                                        ScrollToolRequest,
                                        ScrollToToolRequest,
                                        SeekMediaToolRequest,
                                        SelectToolRequest,
                                        SiteBasedSearchToolRequest,
                                        SwitchTabToolRequest,
                                        TranslatePageToolRequest,
                                        TypeToolRequest,
                                        WaitToolRequest>;
// LINT.ThenChange(//tools/metrics/histograms/metadata/actor/histograms.xml:ToolRequest)

// Converts a polymorphic ToolRequest object to the proper ToolRequestVariant
// type.
ToolRequestVariant ConvertToVariant(const ToolRequest& tr);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOL_REQUEST_VARIANT_H_
