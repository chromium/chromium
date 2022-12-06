// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_ARC_INTENT_H_
#define CHROME_BROWSER_ASH_APP_LIST_ARC_INTENT_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

namespace base {
class TimeTicks;
}

namespace arc {

extern const char kInitialStartParam[];
extern const char kRequestStartTimeParamKey[];
extern const char kRequestDeferredStartTimeParamKey[];

extern const char kAction[];
extern const char kCategory[];
extern const char kComponent[];
extern const char kEndSuffix[];
extern const char kIntentPrefix[];
extern const char kLaunchFlags[];
extern const char kCategoryLauncher[];

// Represents parsed intent.
class Intent {
 public:
  Intent(const Intent&) = delete;
  Intent& operator=(const Intent&) = delete;

  ~Intent();

  enum LaunchFlags : uint32_t {
    FLAG_ACTIVITY_NEW_TASK = 0x10000000,
    FLAG_RECEIVER_NO_ABORT = 0x08000000,
    FLAG_ACTIVITY_RESET_TASK_IF_NEEDED = 0x00200000,
    FLAG_ACTIVITY_LAUNCH_ADJACENT = 0x00001000,
  };

  // Parses provided |intent_as_string|. Returns nullptr if |intent_as_string|
  // cannot be parsed.
  static std::unique_ptr<Intent> Get(const std::string& intent_as_string);

  void AddExtraParam(const std::string& extra_param);
  bool HasExtraParam(const std::string& extra_param) const;

  // Extra param mighgt be key=value pair. Enumetates |extra_params_| and finds
  // one that starts with requested |extra_param_key|. If found value is stored
  // in |out| and true is returned.
  bool GetExtraParamValue(const std::string& extra_param_key,
                          std::string* out) const;

  const std::string& action() const { return action_; }
  void set_action(const std::string& action) { action_ = action; }

  const std::string& category() const { return category_; }
  void set_category(const std::string& category) { category_ = category; }

  const std::string& package_name() const { return package_name_; }
  void set_package_name(const std::string& package_name) {
    package_name_ = package_name;
  }

  const std::string& activity() const { return activity_; }
  void set_activity(const std::string& activity) { activity_ = activity; }

  uint32_t launch_flags() const { return launch_flags_; }
  void set_launch_flags(uint32_t launch_flags) { launch_flags_ = launch_flags; }

  const std::vector<std::string>& extra_params() const { return extra_params_; }

 private:
  Intent();

  std::string action_;                     // Extracted from action.
  std::string category_;                   // Extracted from category.
  std::string package_name_;               // Extracted from component.
  std::string activity_;                   // Extracted from component.
  uint32_t launch_flags_ = 0;              // Extracted from launchFlags;
  std::vector<std::string> extra_params_;  // Other parameters not listed above.
};

// Returns intent that can be used to launch an activity specified by
// |package_name| and |activity|. |extra_params| is the list of optional
// parameters encoded to intent.
std::string GetLaunchIntent(const std::string& package_name,
                            const std::string& activity,
                            const std::vector<std::string>& extra_params);

// This takes existing |launch_intent| and appends it with |extra_params|.
std::string AppendLaunchIntent(const std::string& launch_intent,
                               const std::vector<std::string>& extra_params);

// Helper function that creates pair key=value where value represents |ticks|.
// |ticks| is converted to ms.
std::string CreateIntentTicksExtraParam(const std::string& param_key,
                                        const base::TimeTicks& ticks);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_APP_LIST_ARC_INTENT_H_
