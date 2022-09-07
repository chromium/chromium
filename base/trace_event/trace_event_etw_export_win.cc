// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_event_etw_export_win.h"

#include <evntrace.h>
#include <guiddef.h>
#include <stddef.h>
#include <stdlib.h>
#include <windows.h>

#include "base/at_exit.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_impl.h"
#include "base/trace_event/trace_logging_minimal_win.h"

namespace {

// |kFilteredEventGroupNames| contains the event categories that can be
// exported individually. These categories can be enabled by passing the correct
// keyword when starting the trace. A keyword is a 64-bit flag and we attribute
// one bit per category. We can therefore enable a particular category by
// setting its corresponding bit in the keyword. For events that are not present
// in |kFilteredEventGroupNames|, we have two bits that control their
// behaviour. When bit 46 is enabled, any event that is not disabled by default
// (ie. doesn't start with disabled-by-default-) will be exported. Likewise,
// when bit 47 is enabled, any event that is disabled by default will be
// exported.
//
// Examples of passing keywords to the provider using xperf:
// # This exports "benchmark" and "cc" events
// xperf -start chrome -on Chrome:0x9
//
// # This exports "gpu", "netlog" and all other events that are not disabled by
// # default
// xperf -start chrome -on Chrome:0x4000000000A0
//
// More info about starting a trace and keyword can be obtained by using the
// help section of xperf (xperf -help start). Note that xperf documentation
// refers to keywords as flags and there are two ways to enable them, using
// group names or the hex representation. We only support the latter. Also, we
// ignore the level.
//
// To avoid continually having to bump MSEdge values to next higher bits, we
// are putting MSEdge values at the high end of the bit range and will grow
// 'down' to lower bits for future MSEdge entries.
//
// As the writing of this comment, we have 4 values:
//    "navigation",                                       // 0x40000000000
//    "ServiceWorker",                                    // 0x80000000000
//    "edge_webview",                                     // 0x100000000000
//    "diagnostic_event",                                 // 0x200000000000
//
// This means the next value added should be:
//    "the_next_value",                                   // 0x20000000000
//    "navigation",                                       // 0x40000000000
//    "ServiceWorker",                                    // 0x80000000000
//    "edge_webview",                                     // 0x100000000000
//    "diagnostic_event",                                 // 0x200000000000
//
// The addition of the "unused_bit_nn" entries keeps the existing code execution
// routines working (ex. TraceEventETWExport::UpdateEnabledCategories()) and
// enables others to see which bits are available.
//
// Example: For some new category group...
//   "latency",                                          // 0x8000
//   "blink.user_timing",                                // 0x10000
//   "unused_bit_18",                                    // 0x20000
//   "unused_bit_19",                                    // 0x40000
//   "unused_bit_20",                                    // 0x80000
//    ...
// becomes:
//   "latency",                                          // 0x8000
//   "blink.user_timing",                                // 0x10000
//   "new_upstream_value",                               // 0x20000
//   "unused_bit_19",                                    // 0x40000
//   "unused_bit_20",                                    // 0x80000
//
// The high 16 bits of the keyword have special semantics and should not be
// set for enabling individual categories as they are reserved by winmeta.xml.
const char* const kFilteredEventGroupNames[] = {
    "benchmark",                             // 0x1
    "blink",                                 // 0x2
    "browser",                               // 0x4
    "cc",                                    // 0x8
    "evdev",                                 // 0x10
    "gpu",                                   // 0x20
    "input",                                 // 0x40
    "netlog",                                // 0x80
    "sequence_manager",                      // 0x100
    "toplevel",                              // 0x200
    "v8",                                    // 0x400
    "disabled-by-default-cc.debug",          // 0x800
    "disabled-by-default-cc.debug.picture",  // 0x1000
    "disabled-by-default-toplevel.flow",     // 0x2000
    "startup",                               // 0x4000
    "latency",                               // 0x8000
    "blink.user_timing",                     // 0x10000
    "media",                                 // 0x20000
    "loading",                               // 0x40000
    "base",                                  // 0x80000
    "unused_bit_20",                         // 0x100000
    "unused_bit_21",                         // 0x200000
    "unused_bit_22",                         // 0x400000
    "unused_bit_23",                         // 0x800000
    "unused_bit_24",                         // 0x1000000
    "unused_bit_25",                         // 0x2000000
    "unused_bit_26",                         // 0x4000000
    "unused_bit_27",                         // 0x8000000
    "unused_bit_28",                         // 0x10000000
    "unused_bit_29",                         // 0x20000000
    "unused_bit_30",                         // 0x40000000
    "unused_bit_31",                         // 0x80000000
    "unused_bit_32",                         // 0x100000000
    "unused_bit_33",                         // 0x200000000
    "unused_bit_34",                         // 0x400000000
    "unused_bit_35",                         // 0x800000000
    "unused_bit_36",                         // 0x1000000000
    "unused_bit_37",                         // 0x2000000000
    "unused_bit_38",                         // 0x4000000000
    "unused_bit_39",                         // 0x8000000000
    "unused_bit_40",                         // 0x10000000000
    "unused_bit_41",                         // 0x20000000000
    "navigation",                            // 0x40000000000
    "ServiceWorker",                         // 0x80000000000
    "edge_webview",                          // 0x100000000000
    "diagnostic_event",                      // 0x200000000000
    "__OTHER_EVENTS",                        // 0x400000000000 See below
    "__DISABLED_OTHER_EVENTS",               // 0x800000000000 See below
};

// These must be kept as the last two entries in the above array.
constexpr uint8_t kOtherEventsGroupNameIndex = 46;
constexpr uint8_t kDisabledOtherEventsGroupNameIndex = 47;

// Max number of available keyword bits.
constexpr size_t kMaxNumberOfGroupNames = 48;
uint64_t g_callback_match_any_keyword = 0;

static void __stdcall EtwEnableCallback(LPCGUID SourceId,
                                        ULONG ControlCode,
                                        UCHAR Level,
                                        ULONGLONG MatchAnyKeyword,
                                        ULONGLONG MatchAllKeyword,
                                        PEVENT_FILTER_DESCRIPTOR FilterData,
                                        PVOID CallbackContext) {
  // This callback is called in the context of an ETW OS thread to
  // inform the process of the global state of the level and keyword
  // across all sessions for this provider. We need to update the
  // local keywords so we log the corresponding events. Protect the
  // upper 16 bits reserved by winmeta.xml as they should not be used
  // but older logging code and tools incorrectly used them.
  g_callback_match_any_keyword = MatchAnyKeyword;
  g_callback_match_any_keyword &= ~0xFFFF000000000000;

  DVLOG(1) << "ETW Keyword"
           << " Bits enabled in global context: " << std::hex << MatchAnyKeyword
           << " Bits enabled in our code: " << std::hex
           << g_callback_match_any_keyword;

  base::trace_event::TraceEventETWExport::OnETWEnableUpdate();
}

}  // namespace

namespace base {
namespace trace_event {

bool TraceEventETWExport::is_registration_complete_ = false;

TraceEventETWExport::TraceEventETWExport() {
  // Construct the ETW provider. If construction fails then the event logging
  // calls will fail. We're passing a callback function as part of registration.
  // This allows us to detect changes to enable/disable/keyword changes.

  // This GUID is the used to identify the Chrome provider and is used whenever
  // ETW is enabled via tracing tools and cannot change without updating tools
  // that collect Chrome ETW data.
  static const GUID Chrome_GUID = {
      0xD2D578D9,
      0x2936,
      0x45B6,
      {0xA0, 0x9F, 0x30, 0xE3, 0x27, 0x15, 0xF4, 0x2D}};

  etw_provider_ = std::make_unique<TlmProvider>("Google.Chrome", Chrome_GUID,
                                                &EtwEnableCallback);
  TraceEventETWExport::is_registration_complete_ = true;

  // Make sure to initialize the map with all the group names. Subsequent
  // modifications will be made by the background thread and only affect the
  // values of the keys (no key addition/deletion). Therefore, the map does not
  // require a lock for access.
  // Also set up the map from category name to keyword.
  for (size_t i = 0; i < ARRAYSIZE(kFilteredEventGroupNames); i++) {
    uint64_t keyword = 1ULL << i;
    categories_status_[kFilteredEventGroupNames[i]] = false;
    categories_keyword_[kFilteredEventGroupNames[i]] = keyword;
  }
  // Make sure we stay at 48 entries, the maximum number of bits available
  // for keyword use.
  static_assert(ARRAYSIZE(kFilteredEventGroupNames) <= kMaxNumberOfGroupNames,
                "Exceeded max ETW keyword bits");
}

TraceEventETWExport::~TraceEventETWExport() {
  is_registration_complete_ = false;
}

// static
void TraceEventETWExport::EnableETWExport() {
  auto* instance = GetInstance();
  if (instance) {
    // Sync the enabled categories with ETW by calling UpdateEnabledCategories()
    // that checks the keyword. We'll stay in sync via the EtwEnableCallback
    // we register in TraceEventETWExport's constructor.
    instance->UpdateEnabledCategories();
  }
}

// static
uint64_t TraceEventETWExport::CategoryGroupToKeyword(
    const uint8_t* category_state) {
  uint64_t keyword = 0;

  // To enable multiple sessions with this provider enabled we need to log the
  // level and keyword with the event so that if the sessions differ in the
  // level or keywords enabled we log the right events and allow ETW to
  // route the data to the appropriate session.
  // TODO(joel@microsoft.com) Explore better methods in future integration
  // with perfetto.

  auto* instance = GetInstance();
  if (!instance)
    return keyword;

  // Add in the keyword for the special bits if they are set.
  if (instance->categories_status_
          [kFilteredEventGroupNames[kOtherEventsGroupNameIndex]]) {
    keyword |= instance->categories_keyword_
                   [kFilteredEventGroupNames[kOtherEventsGroupNameIndex]];
  }
  if (instance->categories_status_
          [kFilteredEventGroupNames[kDisabledOtherEventsGroupNameIndex]]) {
    keyword |=
        instance->categories_keyword_
            [kFilteredEventGroupNames[kDisabledOtherEventsGroupNameIndex]];
  }
  // Add in the keyword for the categories specified at the logging site.
  const TraceCategory* category = TraceCategory::FromStatePtr(category_state);
  StringPiece category_group_name = category->name();

  CStringTokenizer category_group_tokens(category_group_name.begin(),
                                         category_group_name.end(), ",");
  while (category_group_tokens.GetNext()) {
    StringPiece category_group_token = category_group_tokens.token_piece();

    // Lookup the keyword for this part of the category_group_name
    // and or in the keyword.
    auto it = instance->categories_keyword_.find(category_group_token);
    if (it != instance->categories_keyword_.end())
      keyword |= it->second;
  }
  return keyword;
}

// static
void TraceEventETWExport::AddEvent(char phase,
                                   const unsigned char* category_group_enabled,
                                   const char* name,
                                   unsigned long long id,
                                   const TraceArguments* args) {
  // We bail early in case exporting is disabled or no consumer is listening.
  auto* instance = GetInstance();
  uint64_t keyword = CategoryGroupToKeyword(category_group_enabled);
  if (!instance ||
      !instance->etw_provider_->IsEnabled(TRACE_LEVEL_NONE, keyword)) {
    return;
  }

  const char* phase_string = nullptr;

  // Space to store the phase identifier and null-terminator, when needed.
  char phase_buffer[2];
  switch (phase) {
    case TRACE_EVENT_PHASE_BEGIN:
      phase_string = "Begin";
      break;
    case TRACE_EVENT_PHASE_END:
      phase_string = "End";
      break;
    case TRACE_EVENT_PHASE_COMPLETE:
      phase_string = "Complete";
      break;
    case TRACE_EVENT_PHASE_INSTANT:
      phase_string = "Instant";
      break;
    case TRACE_EVENT_PHASE_ASYNC_BEGIN:
      phase_string = "Async Begin";
      break;
    case TRACE_EVENT_PHASE_ASYNC_STEP_INTO:
      phase_string = "Async Step Into";
      break;
    case TRACE_EVENT_PHASE_ASYNC_STEP_PAST:
      phase_string = "Async Step Past";
      break;
    case TRACE_EVENT_PHASE_ASYNC_END:
      phase_string = "Async End";
      break;
    case TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN:
      phase_string = "Nestable Async Begin";
      break;
    case TRACE_EVENT_PHASE_NESTABLE_ASYNC_END:
      phase_string = "Nestable Async End";
      break;
    case TRACE_EVENT_PHASE_NESTABLE_ASYNC_INSTANT:
      phase_string = "Nestable Async Instant";
      break;
    case TRACE_EVENT_PHASE_FLOW_BEGIN:
      phase_string = "Phase Flow Begin";
      break;
    case TRACE_EVENT_PHASE_FLOW_STEP:
      phase_string = "Phase Flow Step";
      break;
    case TRACE_EVENT_PHASE_FLOW_END:
      phase_string = "Phase Flow End";
      break;
    case TRACE_EVENT_PHASE_METADATA:
      phase_string = "Phase Metadata";
      break;
    case TRACE_EVENT_PHASE_COUNTER:
      phase_string = "Phase Counter";
      break;
    case TRACE_EVENT_PHASE_SAMPLE:
      phase_string = "Phase Sample";
      break;
    case TRACE_EVENT_PHASE_CREATE_OBJECT:
      phase_string = "Phase Create Object";
      break;
    case TRACE_EVENT_PHASE_SNAPSHOT_OBJECT:
      phase_string = "Phase Snapshot Object";
      break;
    case TRACE_EVENT_PHASE_DELETE_OBJECT:
      phase_string = "Phase Delete Object";
      break;
    default:
      phase_buffer[0] = phase;
      phase_buffer[1] = 0;
      phase_string = phase_buffer;
      break;
  }

  std::string arg_values_string[3];
  size_t num_args = args ? args->size() : 0;
  for (size_t i = 0; i < num_args; i++) {
    const auto type = args->types()[i];
    if (type == TRACE_VALUE_TYPE_CONVERTABLE ||
        type == TRACE_VALUE_TYPE_PROTO) {
      // For convertable types, temporarily do nothing here. This function
      // consumes 1/3 to 1/2 of *total* process CPU time when ETW tracing, and
      // many of the strings created exceed WPA's 4094 byte limit and are shown
      // as "Unable to parse data". See crbug.com/488257.
      //
      // For protobuf-based values, there is no string serialization so skip
      // those as well.
    } else {
      args->values()[i].AppendAsString(type, arg_values_string + i);
    }
  }

  // Log the event and include the info needed to decode it via TraceLogging
  if (num_args == 0) {
    instance->etw_provider_->WriteEvent(
        name, TlmEventDescriptor(0, keyword),
        TlmMbcsStringField("Phase", phase_string));
  } else if (num_args == 1) {
    instance->etw_provider_->WriteEvent(
        name, TlmEventDescriptor(0, keyword),
        TlmMbcsStringField("Phase", phase_string),
        TlmMbcsStringField((args->names()[0]), (arg_values_string[0].c_str())));
  } else if (num_args == 2) {
    instance->etw_provider_->WriteEvent(
        name, TlmEventDescriptor(0, keyword),
        TlmMbcsStringField("Phase", phase_string),
        TlmMbcsStringField((args->names()[0]), (arg_values_string[0].c_str())),
        TlmMbcsStringField((args->names()[1]), (arg_values_string[1].c_str())));
  } else {
    NOTREACHED();
  }
}

// static
void TraceEventETWExport::AddCompleteEndEvent(
    const unsigned char* category_group_enabled,
    const char* name) {
  auto* instance = GetInstance();
  uint64_t keyword = CategoryGroupToKeyword(category_group_enabled);
  if (!instance ||
      !instance->etw_provider_->IsEnabled(TRACE_LEVEL_NONE, keyword)) {
    return;
  }

  // Log the event and include the info needed to decode it via TraceLogging
  instance->etw_provider_->WriteEvent(
      name, TlmEventDescriptor(0, keyword),
      TlmMbcsStringField("Phase", "Complete End"));
}

// static
bool TraceEventETWExport::IsCategoryGroupEnabled(
    StringPiece category_group_name) {
  DCHECK(!category_group_name.empty());

  auto* instance = GetInstanceIfExists();
  if (instance == nullptr)
    return false;

  if (!instance->etw_provider_->IsEnabled())
    return false;

  CStringTokenizer category_group_tokens(category_group_name.begin(),
                                         category_group_name.end(), ",");
  while (category_group_tokens.GetNext()) {
    StringPiece category_group_token = category_group_tokens.token_piece();
    if (instance->IsCategoryEnabled(category_group_token)) {
      return true;
    }
  }
  return false;
}

bool TraceEventETWExport::UpdateEnabledCategories() {
  if (etw_match_any_keyword_ == g_callback_match_any_keyword)
    return false;

  // If the global keyword has changed, update each category. The global
  // context is set by UIforETW (or other ETW trace recording tools)
  // using the ETW infrastructure. When the global context changes the
  // callback will be called to set the updated keyword bits in each
  // browser process that has registered their ETW provider.
  etw_match_any_keyword_ = g_callback_match_any_keyword;
  for (size_t i = 0; i < ARRAYSIZE(kFilteredEventGroupNames); i++) {
    if (etw_match_any_keyword_ & (1ULL << i)) {
      categories_status_[kFilteredEventGroupNames[i]] = true;
    } else {
      categories_status_[kFilteredEventGroupNames[i]] = false;
    }
  }

  // Update the categories in TraceLog.
  TraceLog::GetInstance()->UpdateETWCategoryGroupEnabledFlags();

  return true;
}

bool TraceEventETWExport::IsCategoryEnabled(StringPiece category_name) const {
  // Try to find the category and return its status if found
  auto it = categories_status_.find(category_name);
  if (it != categories_status_.end())
    return it->second;

  // Otherwise return the corresponding default status by first checking if the
  // category is disabled by default.
  if (StartsWith(category_name, "disabled-by-default")) {
    DCHECK(categories_status_.find(
               kFilteredEventGroupNames[kDisabledOtherEventsGroupNameIndex]) !=
           categories_status_.end());
    return categories_status_
        .find(kFilteredEventGroupNames[kDisabledOtherEventsGroupNameIndex])
        ->second;
  } else {
    DCHECK(categories_status_.find(
               kFilteredEventGroupNames[kOtherEventsGroupNameIndex]) !=
           categories_status_.end());
    return categories_status_
        .find(kFilteredEventGroupNames[kOtherEventsGroupNameIndex])
        ->second;
  }
}

// static
void TraceEventETWExport::OnETWEnableUpdate() {
  // During construction, if tracing is already enabled, we'll get
  // a callback synchronously on the same thread. Calling GetInstance
  // in that case will hang since we're in the process of creating the
  // singleton.
  if (is_registration_complete_) {
    auto* instance = GetInstance();
    if (instance)
      instance->UpdateEnabledCategories();
  }
}

// static
TraceEventETWExport* TraceEventETWExport::GetInstance() {
  return Singleton<TraceEventETWExport,
                   StaticMemorySingletonTraits<TraceEventETWExport>>::get();
}

// static
TraceEventETWExport* TraceEventETWExport::GetInstanceIfExists() {
  return Singleton<
      TraceEventETWExport,
      StaticMemorySingletonTraits<TraceEventETWExport>>::GetIfExists();
}

}  // namespace trace_event
}  // namespace base
