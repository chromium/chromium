// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/activity_log/activity_log_policy.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/files/file_path.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/extensions/activity_log/activity_action_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

namespace constants = activity_log_constants;

namespace extensions {

ActivityLogPolicy::ActivityLogPolicy(Profile* profile) {}

ActivityLogPolicy::~ActivityLogPolicy() {}

void ActivityLogPolicy::SetClockForTesting(base::Clock* clock) {
  testing_clock_ = clock;
}

base::Time ActivityLogPolicy::Now() const {
  if (testing_clock_)
    return testing_clock_->Now();
  return base::Time::Now();
}

ActivityLogDatabasePolicy::ActivityLogDatabasePolicy(
    Profile* profile,
    const base::FilePath& database_name)
    : ActivityLogPolicy(profile) {
  CHECK(profile);
  base::FilePath profile_base_path = profile->GetPath();
  db_ = new ActivityDatabase(this);
  database_path_ = profile_base_path.Append(database_name);
}

void ActivityLogDatabasePolicy::Init() {
  LOG(WARNING) << "Scheduling init";
  ScheduleAndForget(db_.get(), &ActivityDatabase::Init, database_path_);
}

void ActivityLogDatabasePolicy::Flush() {
  ScheduleAndForget(activity_database(),
                    &ActivityDatabase::AdviseFlush,
                    ActivityDatabase::kFlushImmediately);
}

sql::Database* ActivityLogDatabasePolicy::GetDatabaseConnection() const {
  return db_->GetSqlConnection();
}

// static
std::string ActivityLogPolicy::Util::Serialize(
    std::optional<base::ValueView> value) {
  std::string value_as_text;
  if (value) {
    JSONStringValueSerializer serializer(&value_as_text);
    serializer.SerializeAndOmitBinaryValues(*value);
  }
  return value_as_text;
}

// static
void ActivityLogPolicy::Util::StripPrivacySensitiveFields(
    scoped_refptr<Action> action) {
  // Clear incognito URLs/titles.
  if (action->page_incognito()) {
    action->set_page_url(GURL());
    action->set_page_title("");
    action->set_page_incognito(false);
  }
  if (action->arg_incognito()) {
    action->set_arg_url(GURL());
    action->set_arg_incognito(false);
  }

  // Strip query parameters, username/password, etc., from URLs.
  if (action->page_url().is_valid() || action->arg_url().is_valid()) {
    GURL::Replacements url_sanitizer;
    url_sanitizer.ClearUsername();
    url_sanitizer.ClearPassword();
    url_sanitizer.ClearQuery();
    url_sanitizer.ClearRef();

    if (action->page_url().is_valid())
      action->set_page_url(action->page_url().ReplaceComponents(url_sanitizer));
    if (action->arg_url().is_valid())
      action->set_arg_url(action->arg_url().ReplaceComponents(url_sanitizer));
  }

  // Clear WebRequest details; only keep a record of which types of
  // modifications were performed.
  if (action->action_type() == Action::ACTION_WEB_REQUEST) {
    if (base::Value::Dict* details =
            action->mutable_other().FindDict(constants::kActionWebRequest)) {
      for (auto detail : *details) {
        details->SetByDottedPath(detail.first, true);
      }
    }
  }
}

// static
void ActivityLogPolicy::Util::StripArguments(const ApiSet& api_allowlist,
                                             scoped_refptr<Action> action) {
  if (api_allowlist.find(std::make_pair(
          action->action_type(), action->api_name())) == api_allowlist.end()) {
    action->set_args(std::nullopt);
  }
}

// static
base::Time ActivityLogPolicy::Util::AddDays(const base::Time& base_date,
                                            int days) {
  // To allow for time zone changes, add an additional partial day then round
  // down to midnight.
  return (base_date + base::Days(days) + base::Hours(4)).LocalMidnight();
}

// static
void ActivityLogPolicy::Util::ComputeDatabaseTimeBounds(const base::Time& now,
                                                        int days_ago,
                                                        int64_t* early_bound,
                                                        int64_t* late_bound) {
  base::Time morning_midnight = now.LocalMidnight();
  if (days_ago == 0) {
    *early_bound = morning_midnight.ToInternalValue();
    *late_bound = base::Time::Max().ToInternalValue();
  } else {
    base::Time early_time = Util::AddDays(morning_midnight, -days_ago);
    base::Time late_time = Util::AddDays(early_time, 1);
    *early_bound = early_time.ToInternalValue();
    *late_bound = late_time.ToInternalValue();
  }
}

}  // namespace extensions
