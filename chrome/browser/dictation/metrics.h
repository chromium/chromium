// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_METRICS_H_
#define CHROME_BROWSER_DICTATION_METRICS_H_

#include <string_view>

namespace dictation {

inline constexpr std::string_view kIsEnabledOnProfileInitHistogramName =
    "VoiceTyping.IsEnabledOnProfileInit";
inline constexpr std::string_view kSessionStartSourceHistogramName =
    "VoiceTyping.SessionStartSource";
inline constexpr std::string_view kStreamStartTriggerHistogramName =
    "VoiceTyping.StreamStartTrigger";

// Entry points for starting a Dictation session.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(DictationSessionEntryPoint)
enum class DictationSessionEntryPoint {
  kContextMenu = 0,
  kHotkeyToggle = 1,
  kMaxValue = kHotkeyToggle,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/voice_typing/enums.xml:DictationSessionEntryPoint)

// Triggers for starting a Dictation stream.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(DictationStreamStartTrigger)
enum class DictationStreamStartTrigger {
  // The initial stream created when a session is first started.
  kSessionStart = 0,

  // The stream was started by clicking the "Start" button in the bubble UI.
  kStartButton = 1,

  // The stream was started by the user moving focus into a new input box.
  kFocusChange = 2,

  // The stream was started from the context menu while a session was already
  // open.
  kContextMenuExistingSession = 3,

  // The stream was started by the user pressing the hotkey while a session was
  // already open.
  // TODO (b/540938709): Add testing for this metric
  kHotkeyToggleExistingSession = 4,

  kMaxValue = kHotkeyToggleExistingSession,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/voice_typing/enums.xml:DictationStreamStartTrigger)

// Records whether Dictation is overall enabled (feature and policies enabled)
// when DictationKeyedService is initialized for a profile.
void RecordDictationIsEnabledOnProfileInit(bool is_enabled);

// Records the entry point for starting a Dictation session.
void RecordDictationSessionStartSource(DictationSessionEntryPoint entry_point);

// Records the trigger for starting a Dictation stream.
void RecordDictationStreamStartTrigger(DictationStreamStartTrigger trigger);

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_METRICS_H_
