// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/debug_daemon_log_source.h"

#include <stddef.h>

#include <utility>

#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/os_feedback/chrome_os_feedback_delegate.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "components/feedback/feedback_util.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"

namespace system_logs {

namespace {

constexpr char kEmpty[] = "<empty>";
constexpr char kNotAvailable[] = "<not available>";
constexpr char kRoutesKeyName[] = "routes";
constexpr char kRoutesv6KeyName[] = "routes6";
constexpr char kLogTruncated[] = "<earlier logs truncated>\n";

// List of user log files that Chrome reads directly as these logs are generated
// by Chrome itself.
constexpr struct UserLogs {
  // A string key used as a title for this log in feedback reports.
  const char* log_key;

  // The log file's path relative to the user's profile directory.
  const char* log_file_relative_path;

  // If true, |log_file_relative_path| is a pattern for which files to match.
  // This works like shell globbing. If there are multiple matches files, it
  // chooses the newest file, with the latest modified time.
  bool pattern = false;
} kUserLogs[] = {
    {"chrome_user_log", "log/chrome"},
    {"chrome_user_log.PREVIOUS", "log/chrome_????????-??????", true},
    {"libassistant_user_log", "google-assistant-library/log/libassistant.log"},
    {"login-times", "login-times"},
    {"logout-times", "logout-times"},
};

// List of debugd entries to exclude from the results.
constexpr std::array<const char*, 2> kExcludeList = {
    // Shill device and service properties are retrieved by ShillLogSource.
    "network-devices",
    "network-services",
};

// Buffer size for user logs in bytes. Given that maximum feedback report size
// is ~7M and that majority of log files are under 1M, we set a per-file limit
// of 1MiB.
const int64_t kMaxLogSize = 1024 * 1024;

std::vector<debugd::FeedbackLogType> GetLogTypesForUser(
    const user_manager::User* user) {
  // The default list of log types that we request from debugd.
  std::vector<debugd::FeedbackLogType> included_log_types = {
      debugd::FeedbackLogType::ARC_BUG_REPORT,
      debugd::FeedbackLogType::CONNECTIVITY_REPORT,
      debugd::FeedbackLogType::VERBOSE_COMMAND_LOGS,
      debugd::FeedbackLogType::COMMAND_LOGS,
      debugd::FeedbackLogType::FEEDBACK_LOGS,
      debugd::FeedbackLogType::BLUETOOTH_BQR,
      debugd::FeedbackLogType::LSB_RELEASE_INFO,
      debugd::FeedbackLogType::PERF_DATA,
      debugd::FeedbackLogType::OS_RELEASE_INFO,
      debugd::FeedbackLogType::VAR_LOG_FILES};
  if (user && ash::ChromeOsFeedbackDelegate::IsWifiDebugLogsAllowed(
                  user->GetProfilePrefs())) {
    // Include WIFI_FIRMWARE_DUMPS since it is allowed for the user.
    included_log_types.push_back(debugd::FeedbackLogType::WIFI_FIRMWARE_DUMPS);
  }

  return included_log_types;
}

}  // namespace

std::string ReadUserLogFile(const base::FilePath& log_file_path) {
  std::optional<std::string> maybe_value =
      feedback_util::ReadEndOfFile(log_file_path, kMaxLogSize);

  if (maybe_value.has_value() && maybe_value.value().size() == kMaxLogSize) {
    maybe_value.value().replace(0, strlen(kLogTruncated), kLogTruncated);

    LOG(WARNING) << "Large log file was likely truncated: " << log_file_path;
  }
  return (maybe_value.has_value() && !maybe_value.value().empty())
             ? maybe_value.value()
             : std::string(kNotAvailable);
}

std::string ReadUserLogFilePattern(
    const base::FilePath& log_file_path_pattern) {
  base::FilePath log_file_dir = log_file_path_pattern.DirName();
  base::FileEnumerator file_enumerator(
      log_file_dir, /*recursive=*/false, base::FileEnumerator::FILES,
      log_file_path_pattern.BaseName().value());

  base::Time newest_file_mtime;
  base::FilePath newest_file_path;
  for (base::FilePath path = file_enumerator.Next(); !path.empty();
       path = file_enumerator.Next()) {
    const base::FileEnumerator::FileInfo info = file_enumerator.GetInfo();
    if (newest_file_mtime.is_null() ||
        info.GetLastModifiedTime() >= newest_file_mtime) {
      newest_file_mtime = info.GetLastModifiedTime();
      newest_file_path = path;
    }
  }

  return newest_file_mtime.is_null() ? std::string(kNotAvailable)
                                     : ReadUserLogFile(newest_file_path);
}

// Reads the contents of the user log files listed in |kUserLogs| and adds them
// to the |response| parameter.
void ReadUserLogFiles(const std::vector<base::FilePath>& profile_dirs,
                      SystemLogsResponse* response) {
  for (size_t i = 0; i < profile_dirs.size(); ++i) {
    std::string profile_prefix = "Profile[" + base::NumberToString(i) + "] ";
    const base::FilePath profile_dir = profile_dirs[i];
    for (const auto& log : kUserLogs) {
      const base::FilePath log_file_path_or_pattern =
          profile_dir.AppendASCII(log.log_file_relative_path);
      const std::string content =
          log.pattern ? ReadUserLogFilePattern(log_file_path_or_pattern)
                      : ReadUserLogFile(log_file_path_or_pattern);
      response->emplace(profile_prefix + log.log_key, content);
    }
  }
}

DebugDaemonLogSource::DebugDaemonLogSource(bool scrub)
    : SystemLogsSource("DebugDemon"),
      response_(new SystemLogsResponse()),
      num_pending_requests_(0),
      scrub_(scrub) {}

DebugDaemonLogSource::~DebugDaemonLogSource() {}

void DebugDaemonLogSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());
  DCHECK(callback_.is_null());

  callback_ = std::move(callback);
  ash::DebugDaemonClient* client = ash::DebugDaemonClient::Get();

  client->GetRoutes(true,   // Numeric
                    false,  // No IPv6
                    false,  // All tables option disabled
                    base::BindOnce(&DebugDaemonLogSource::OnGetRoutes,
                                   weak_ptr_factory_.GetWeakPtr(), false));
  ++num_pending_requests_;
  client->GetRoutes(true,   // Numeric
                    true,   // with IPv6
                    false,  // All tables option disabled
                    base::BindOnce(&DebugDaemonLogSource::OnGetRoutes,
                                   weak_ptr_factory_.GetWeakPtr(), true));
  ++num_pending_requests_;

  const auto start_time = base::TimeTicks::Now();
  if (scrub_) {
    const user_manager::User* user =
        user_manager::UserManager::Get()->GetActiveUser();
    const auto account_identifier =
        cryptohome::CreateAccountIdentifierFromAccountId(
            user ? user->GetAccountId() : EmptyAccountId());

    client->GetFeedbackLogs(
        account_identifier, GetLogTypesForUser(user),
        base::BindOnce(&DebugDaemonLogSource::OnGetLogs,
                       weak_ptr_factory_.GetWeakPtr(), start_time));

  } else {
    client->GetAllLogs(base::BindOnce(&DebugDaemonLogSource::OnGetLogs,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      start_time));
  }
  ++num_pending_requests_;
}

void DebugDaemonLogSource::OnGetRoutes(
    bool is_ipv6,
    std::optional<std::vector<std::string>> routes) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::string key = is_ipv6 ? kRoutesv6KeyName : kRoutesKeyName;
  (*response_)[key] = routes.has_value()
                          ? base::JoinString(routes.value(), "\n")
                          : kNotAvailable;
  RequestCompleted();
}

void DebugDaemonLogSource::OnGetOneLog(std::string key,
                                       std::optional<std::string> status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  (*response_)[std::move(key)] = std::move(status).value_or(kNotAvailable);
  RequestCompleted();
}

void DebugDaemonLogSource::OnGetLogs(const base::TimeTicks get_start_time,
                                     bool succeeded,
                                     const KeyValueMap& logs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // We are interested in the performance of gathering the logs for feedback
  // reports only where the logs will always be scrubbed. GetBigFeedbackLogs is
  // the dbus method used.
  if (scrub_) {
    base::UmaHistogramBoolean("Feedback.ChromeOSApp.GetBigFeedbackLogs.Success",
                              succeeded);
    base::UmaHistogramMediumTimes(
        "Feedback.ChromeOSApp.Duration.GetBigFeedbackLogs",
        base::TimeTicks::Now() - get_start_time);
  }
  int empty_log_count = 0;
  int not_available_log_count = 0;
  int other_log_count = 0;

  // We ignore 'succeeded' for this callback - we want to display as much of the
  // debug info as we can even if we failed partway through parsing, and if we
  // couldn't fetch any of it, none of the fields will even appear.
  for (const auto& log : logs) {
    if (base::Contains(kExcludeList, log.first)) {
      continue;
    }
    response_->insert(log);
    if (log.second == kEmpty) {
      ++empty_log_count;
    } else if (log.second == kNotAvailable) {
      ++not_available_log_count;
    } else {
      ++other_log_count;
    }
  }
  if (scrub_) {
    // Record stats for the logs received from debugd.
    // As of today, the total logs are about 211.
    base::UmaHistogramCounts1000(
        "Feedback.ChromeOSApp.GetBigFeedbackLogs.EmptyCount", empty_log_count);
    base::UmaHistogramCounts1000(
        "Feedback.ChromeOSApp.GetBigFeedbackLogs.NotAvailableCount",
        not_available_log_count);
    base::UmaHistogramCounts1000(
        "Feedback.ChromeOSApp.GetBigFeedbackLogs.OtherCount", other_log_count);
  }
  RequestCompleted();
}

void DebugDaemonLogSource::GetLoggedInUsersLogFiles() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // List all logged-in users' profile directories.
  std::vector<base::FilePath> profile_dirs;
  const user_manager::UserList& users =
      user_manager::UserManager::Get()->GetLoggedInUsers();
  for (const user_manager::User* user : users) {
    if (user->username_hash().empty()) {
      continue;
    }

    profile_dirs.emplace_back(
        ash::ProfileHelper::GetProfilePathByUserIdHash(user->username_hash()));
  }

  auto response = std::make_unique<SystemLogsResponse>();
  SystemLogsResponse* response_ptr = response.get();
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ReadUserLogFiles, profile_dirs, response_ptr),
      base::BindOnce(&DebugDaemonLogSource::MergeUserLogFilesResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(response)));
}

void DebugDaemonLogSource::MergeUserLogFilesResponse(
    std::unique_ptr<SystemLogsResponse> response) {
  for (auto& pair : *response) {
    response_->emplace(pair.first, std::move(pair.second));
  }

  auto response_to_return = std::make_unique<SystemLogsResponse>();
  std::swap(response_to_return, response_);
  DCHECK(!callback_.is_null());
  std::move(callback_).Run(std::move(response_to_return));
}

void DebugDaemonLogSource::RequestCompleted() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback_.is_null());

  --num_pending_requests_;
  if (num_pending_requests_ > 0) {
    return;
  }

  // When all other logs are collected, fetch the user logs, because any errors
  // fetching the other logs is reported in the user logs.
  GetLoggedInUsersLogFiles();
}

}  // namespace system_logs
