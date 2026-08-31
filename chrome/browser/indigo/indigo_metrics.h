// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INDIGO_INDIGO_METRICS_H_
#define CHROME_BROWSER_INDIGO_INDIGO_METRICS_H_

#include <optional>

#include "chrome/browser/indigo/indigo_page_action_controller.h"

namespace page_actions {
enum class PageActionPriorityCategory;
}  // namespace page_actions

namespace indigo {

inline constexpr char kShownEntryPointHistogram[] =
    "Indigo.PageAction.ShownEntryPoint";
inline constexpr char kClickedEntryPointHistogram[] =
    "Indigo.PageAction.ClickedEntryPoint";

inline constexpr char kSuggestionChipShowAction[] =
    "Indigo.PageAction.SuggestionChip.Show";
inline constexpr char kProactiveAnchoredMessageShowAction[] =
    "Indigo.PageAction.AnchoredMessage.Proactive.Show";
inline constexpr char kReactiveAnchoredMessageShowAction[] =
    "Indigo.PageAction.AnchoredMessage.Reactive.Show";

inline constexpr char kSuggestionChipClickAction[] =
    "Indigo.PageAction.SuggestionChip.Click";
inline constexpr char kAnchoredMessageClickAction[] =
    "Indigo.PageAction.AnchoredMessage.Click";
inline constexpr char kErrorToastRetryClickAction[] =
    "Indigo.ErrorToast.Retry.Click";

// Records UMA histogram and UserAction for shown entry points.
void RecordShownEntryPoint(IndigoPageActionEntryPoint entry_point);

// Records UMA histogram and UserAction for clicked entry points.
void RecordClickedEntryPoint(
    EntryPoint entry_point,
    std::optional<page_actions::PageActionPriorityCategory>
        last_anchored_message_priority);

}  // namespace indigo

#endif  // CHROME_BROWSER_INDIGO_INDIGO_METRICS_H_
