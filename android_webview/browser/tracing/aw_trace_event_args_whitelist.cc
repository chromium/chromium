// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/tracing/aw_trace_event_args_whitelist.h"

#include "base/bind.h"
#include "base/strings/pattern.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"

namespace {

struct WhitelistEntry {
  const char* category_name;
  const char* event_name;
  const char* const* arg_name_filter;
};

const char* const kMemoryDumpAllowedArgs[] = {"dumps", nullptr};

const WhitelistEntry kEventArgsWhitelist[] = {
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
// with chrome/common/trace_event_args_whitelist.cc, see crbug.com/805045.
bool IsTraceArgumentNameWhitelisted(const char* const* granular_filter,
                                    const char* arg_name) {
  for (int i = 0; granular_filter[i] != nullptr; ++i) {
    if (base::MatchPattern(arg_name, granular_filter[i]))
      return true;
  }

  return false;
}

bool IsTraceEventArgsWhitelisted(
    const char* category_group_name,
    const char* event_name,
    base::trace_event::ArgumentNameFilterPredicate* arg_name_filter) {
  DCHECK(arg_name_filter);
  base::CStringTokenizer category_group_tokens(
      category_group_name, category_group_name + strlen(category_group_name),
      ",");
  while (category_group_tokens.GetNext()) {
    const std::string& category_group_token = category_group_tokens.token();
    for (int i = 0; kEventArgsWhitelist[i].category_name != nullptr; ++i) {
      const WhitelistEntry& whitelist_entry = kEventArgsWhitelist[i];
      DCHECK(whitelist_entry.event_name);

      if (base::MatchPattern(category_group_token,
                             whitelist_entry.category_name) &&
          base::MatchPattern(event_name, whitelist_entry.event_name)) {
        if (whitelist_entry.arg_name_filter) {
          *arg_name_filter = base::BindRepeating(
              &IsTraceArgumentNameWhitelisted, whitelist_entry.arg_name_filter);
        }
        return true;
      }
    }
  }

  return false;
}

bool IsTraceMetadataWhitelisted(const std::string& name) {
  return false;
}

}  // namespace android_webview
