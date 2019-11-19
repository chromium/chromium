// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_TRACING_EVENT_MATCHER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_TRACING_EVENT_MATCHER_H_

#include <stddef.h>

#include <map>
#include <string>

#include "base/macros.h"
#include "base/optional.h"

namespace arc {

class ArcTracingEvent;

// Helper that allows to match events based on provided criteria.
class ArcTracingEventMatcher {
 public:
  ArcTracingEventMatcher();
  // Format category:name[*]?(arg_name=arg_value;..)
  // For example:
  // exo:Surface::Attach
  // exo:Surface::Attach(buffer_id=0x7f9f5110690)
  // android:HW_VSYNC_0|*
  explicit ArcTracingEventMatcher(const std::string& data);

  // Returns true in case |event| matches criteria set.
  bool Match(const ArcTracingEvent& event) const;

  base::Optional<int64_t> ReadAndroidEventInt64(
      const ArcTracingEvent& event) const;

  // Sets the expected phase. Tested event does not match if its phase does not
  // match |phase|. This is an optional criteria.
  ArcTracingEventMatcher& SetPhase(char phase);
  // Sets the expected category. Tested event does not match if its category
  // does not match |category|. This is an optional criteria.
  ArcTracingEventMatcher& SetCategory(const std::string& category);
  // Sets the expected name. Tested event does not match if its name does not
  // match |name|. This is an optional criteria.
  ArcTracingEventMatcher& SetName(const std::string& name);
  // Adds the expected argument. Tested event does not match if it does not
  // contains the argument specified by |key| or argument does not match
  // |value|.
  ArcTracingEventMatcher& AddArgument(const std::string& key,
                                      const std::string& value);

 private:
  // Defines the phase to match.
  char phase_ = 0;
  // Defines the category to match.
  std::string category_;
  // Defines the name to match.
  std::string name_;
  // If true, name_ is a prefix to match instead of the entire string.
  bool name_prefix_match_ = false;
  // Defines set of arguments to match if needed.
  std::map<std::string, std::string> args_;

  DISALLOW_COPY_AND_ASSIGN(ArcTracingEventMatcher);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_TRACING_EVENT_MATCHER_H_
