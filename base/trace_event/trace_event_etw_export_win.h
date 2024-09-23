// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

// This file contains the Windows-specific exporting to ETW.
#ifndef BASE_TRACE_EVENT_TRACE_EVENT_ETW_EXPORT_WIN_H_
#define BASE_TRACE_EVENT_TRACE_EVENT_ETW_EXPORT_WIN_H_

#include <windows.h>

#include <stdint.h>

#include <map>
#include <memory>
#include <string_view>

#include "base/base_export.h"
#include "base/trace_event/trace_event_impl.h"
#include "base/trace_event/trace_logging_minimal_win.h"

namespace base {

template <typename Type>
struct StaticMemorySingletonTraits;

namespace trace_event {

// This GUID is the used to identify the Chrome provider and is used whenever
// ETW is enabled via tracing tools and cannot change without updating tools
// that collect Chrome ETW data.
inline constexpr GUID Chrome_GUID = {
    0xD2D578D9,
    0x2936,
    0x45B6,
    {0xA0, 0x9F, 0x30, 0xE3, 0x27, 0x15, 0xF4, 0x2D}};

class BASE_EXPORT TraceEventETWExport {
 public:
  TraceEventETWExport(const TraceEventETWExport&) = delete;
  TraceEventETWExport& operator=(const TraceEventETWExport&) = delete;
  ~TraceEventETWExport();

  // Retrieves the singleton.
  // Note that this may return NULL post-AtExit processing.
  static TraceEventETWExport* GetInstance();

  // Retrieves the singleton iff it was previously instantiated by a
  // GetInstance() call. Avoids creating the instance only to check that it
  // wasn't disabled. Note that, like GetInstance(), this may also return NULL
  // post-AtExit processing.
  static TraceEventETWExport* GetInstanceIfExists();

  // Enables exporting of events to ETW. If tracing is disabled for the Chrome
  // provider, AddEvent and AddCustomEvent will simply return when called.
  static void EnableETWExport();

  // Exports an event to ETW. This is mainly used in
  // TraceLog::AddTraceEventWithThreadIdAndTimestamp to export internal events.
  static void AddEvent(char phase,
                       const unsigned char* category_group_enabled,
                       const char* name,
                       unsigned long long id,
                       TimeTicks timestamp,
                       const TraceArguments* args);

  // Exports an ETW event that marks the end of a complete event.
  static void AddCompleteEndEvent(const unsigned char* category_group_enabled,
                                  const char* name);

  // Returns true if any category in the group is enabled.
  static bool IsCategoryGroupEnabled(std::string_view category_group_name);

 private:
  // Ensure only the provider can construct us.
  friend struct StaticMemorySingletonTraits<TraceEventETWExport>;
  TraceEventETWExport();

  // Called from the ETW EnableCallback when the state of the provider or
  // keywords has changed.
  void OnETWEnableUpdate(TlmProvider::EventControlCode enabled);

  // Updates the list of enabled categories by consulting the ETW keyword.
  // Returns true if there was a change, false otherwise.
  bool UpdateEnabledCategories();

  // Returns true if the category is enabled.
  bool IsCategoryEnabled(std::string_view category_name) const;

  uint64_t CategoryStateToETWKeyword(const uint8_t* category_state);

  bool is_registration_complete_ = false;

  // The keywords that were enabled last time the callback was made.
  uint64_t etw_match_any_keyword_ = 0;

  // The provider is set based on channel for MSEdge, in other Chromium
  // based browsers all channels use the same GUID/provider.
  std::unique_ptr<TlmProvider> etw_provider_;

  // Maps category names to their status (enabled/disabled).
  std::map<std::string_view, bool> categories_status_;
};

BASE_EXPORT uint64_t
CategoryGroupToETWKeyword(std::string_view category_group_name);

BASE_EXPORT perfetto::protos::gen::TrackEventConfig
ETWKeywordToTrackEventConfig(uint64_t keyword);

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_TRACE_EVENT_ETW_EXPORT_WIN_H_
