// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/tracing/aw_trace_event_args_allowlist.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/pattern.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"

namespace {

struct AllowlistEntry {
  const char* category_name;
  const char* event_name;
  raw_ptr<const char* const> arg_name_filter;
};

const char* const kMemoryDumpAllowedArgs[] = {"dumps", nullptr};

const AllowlistEntry kEventArgsAllowlist[] = {
    {"__metadata", "thread_name", nullptr},
    {"__metadata", "process_name", nullptr},
    {"__metadata", "process_uptime_seconds", nullptr},
    {"__metadata", "stackFrames", nullptr},
    {"__metadata", "typeNames", nullptr},
    // Redefined the string since MemoryDumpManager::kTraceCategory causes
    // static initialization of this struct.
    {TRACE_DISABLED_BY_DEFAULT("memory-infra"), "*", kMemoryDumpAllowedArgs},
    {nullptr, nullptr, nullptr}};

}  // namespace

namespace android_webview {

// TODO(timvolodine): refactor this into base/ to avoid code duplication
// with chrome/common/trace_event_args_allowlist.cc, see crbug.com/805045.
bool IsTraceArgumentNameAllowlisted(const char* const* granular_filter,
                                    const char* arg_name) {
  for (int i = 0; granular_filter[i] != nullptr; ++i) {
    if (base::MatchPattern(arg_name, granular_filter[i]))
      return true;
  }

  return false;
}

bool IsTraceEventArgsAllowlisted(
    const char* category_group_name,
    const char* event_name,
    base::trace_event::ArgumentNameFilterPredicate* arg_name_filter) {
  DCHECK(arg_name_filter);
  base::CStringTokenizer category_group_tokens(
      category_group_name, category_group_name + strlen(category_group_name),
      ",");
  while (category_group_tokens.GetNext()) {
    const std::string& category_group_token = category_group_tokens.token();
    for (int i = 0; kEventArgsAllowlist[i].category_name != nullptr; ++i) {
      const AllowlistEntry& allowlist_entry = kEventArgsAllowlist[i];
      DCHECK(allowlist_entry.event_name);

      if (base::MatchPattern(category_group_token,
                             allowlist_entry.category_name) &&
          base::MatchPattern(event_name, allowlist_entry.event_name)) {
        if (allowlist_entry.arg_name_filter) {
          *arg_name_filter = base::BindRepeating(
              &IsTraceArgumentNameAllowlisted, allowlist_entry.arg_name_filter);
        }
        return true;
      }
    }
  }

  return false;
}

bool IsTraceMetadataAllowlisted(const std::string& name) {
  return false;
}

}  // namespace android_webview
