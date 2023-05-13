// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/session_flags_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/constants/ash_switches.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "components/user_manager/user_names.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"

namespace ash::test {
namespace {

// Keys for values in dictionary used to preserve session manager state.
constexpr char kUserIdKey[] = "active_user_id";
constexpr char kUserHashKey[] = "active_user_hash";
constexpr char kRestartJobKey[] = "restart_job";
constexpr char kUserFlagsKey[] = "user_flags";
constexpr char kFlagNameKey[] = "name";
constexpr char kFlagValueKey[] = "value";

constexpr char kCachedSessionStateFile[] = "test_session_manager_state.json";

}  // namespace

SessionFlagsManager::SessionFlagsManager() = default;

SessionFlagsManager::~SessionFlagsManager() {
  Finalize();
}

void SessionFlagsManager::SetUpSessionRestore() {
  DCHECK_EQ(mode_, Mode::LOGIN_SCREEN);
  mode_ = Mode::LOGIN_SCREEN_WITH_SESSION_RESTORE;

  base::FilePath user_data_path;
  CHECK(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_path))
      << "Unable to get used data dir";

  backing_file_ = user_data_path.AppendASCII(kCachedSessionStateFile);
  LoadStateFromBackingFile();
}

void SessionFlagsManager::SetDefaultLoginSwitches(
    const std::vector<Switch>& switches) {
  default_switches_ = {{chromeos::switches::kPolicySwitchesBegin, ""}};
  default_switches_.insert(default_switches_.end(), switches.begin(),
                           switches.end());
  default_switches_.emplace_back(
      std::make_pair(chromeos::switches::kPolicySwitchesEnd, ""));
}

void SessionFlagsManager::AppendSwitchesToCommandLine(
    base::CommandLine* command_line) {
  if (restart_job_.has_value()) {
    DCHECK_EQ(mode_, Mode::LOGIN_SCREEN_WITH_SESSION_RESTORE);
    for (const auto& item : *restart_job_) {
      // Do not override flags added to test command line by default.
      if (command_line->HasSwitch(item.first)) {
        continue;
      }
      command_line->AppendSwitchASCII(item.first, item.second);
    }
  }

  if (mode_ == Mode::LOGIN_SCREEN ||
      (user_id_.empty() && !restart_job_.has_value())) {
    command_line->AppendSwitch(switches::kLoginManager);
    command_line->AppendSwitch(switches::kForceLoginManagerInTests);
    command_line->AppendSwitchASCII(switches::kLoginProfile, "user");
  } else if (!user_id_.empty()) {
    DCHECK_EQ(mode_, Mode::LOGIN_SCREEN_WITH_SESSION_RESTORE);
    command_line->AppendSwitchASCII(switches::kLoginUser, user_id_);
    command_line->AppendSwitchASCII(switches::kLoginProfile, user_hash_);
  }

  // Session manager uses extra args to pass both default, login policy
  // switches and user flag switches. If extra args are not explicitly set for
  // current user before resarting chrome (which is represented by user_flags_
  // not being set here), session manager will keep using extra args set by
  // default login switches - simulate  this behavior.
  const std::vector<Switch>& active_switches =
      user_flags_.has_value() ? *user_flags_ : default_switches_;
  for (const auto& item : active_switches) {
    command_line->AppendSwitchASCII(item.first, item.second);
  }
}

void SessionFlagsManager::Finalize() {
  if (finalized_ || mode_ != Mode::LOGIN_SCREEN_WITH_SESSION_RESTORE) {
    return;
  }

  finalized_ = true;
  StoreStateToBackingFile();
}

void SessionFlagsManager::LoadStateFromBackingFile() {
  DCHECK_EQ(mode_, Mode::LOGIN_SCREEN_WITH_SESSION_RESTORE);

  JSONFileValueDeserializer deserializer(backing_file_);

  int error_code = 0;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(&error_code, nullptr);
  if (error_code != JSONFileValueDeserializer::JSON_NO_ERROR) {
    return;
  }

  base::Value::Dict& value_dict = value->GetDict();
  const std::string* user_id = value_dict.FindString(kUserIdKey);
  if (user_id && !user_id->empty()) {
    user_id_ = *user_id;
  }

  const std::string* user_hash = value_dict.FindString(kUserHashKey);
  if (user_hash && !user_hash->empty()) {
    user_hash_ = *user_hash;
  }

  base::Value::List* user_flags = value_dict.FindList(kUserFlagsKey);
  if (user_flags) {
    user_flags_ = std::vector<Switch>();
    for (const base::Value& flag : *user_flags) {
      const auto& flag_dict = flag.GetDict();
      user_flags_->emplace_back(
          std::make_pair(*flag_dict.FindString(kFlagNameKey),
                         *flag_dict.FindString(kFlagValueKey)));
    }
  }

  base::Value::List* restart_job = value_dict.FindList(kRestartJobKey);
  if (restart_job) {
    restart_job_ = std::vector<Switch>();
    for (const base::Value& job_switch : *restart_job) {
      const auto& job_switch_dict = job_switch.GetDict();
      restart_job_->emplace_back(
          std::make_pair(*job_switch_dict.FindString(kFlagNameKey),
                         *job_switch_dict.FindString(kFlagValueKey)));
    }
  }
}

void SessionFlagsManager::StoreStateToBackingFile() {
  FakeSessionManagerClient* session_manager = FakeSessionManagerClient::Get();
  const SessionManagerClient::ActiveSessionsMap& sessions =
      session_manager->user_sessions();
  const bool session_active =
      !sessions.empty() && !session_manager->session_stopped();
  const bool has_restart_job = session_manager->restart_job_argv().has_value();
  // If a user session is not active, clear the backing file so default flags
  // are used next time.
  if (!session_active && !has_restart_job) {
    base::DeleteFile(backing_file_);
    return;
  }

  std::string user_id;
  std::string user_profile;
  if (has_restart_job) {
    // Set guest user ID, so it can be used to retrieve user flags.
    // Restart job is only used for guest login, and the command line it sets
    // up is expected to already contain switches::kLoginUser and
    // switches::kLoginProfile. There is a DCHECK below to ensure that restart
    // job sets these flags to expected guest user values.
    user_id = user_manager::kGuestUserName;
  } else {
    // Only the primary user's switches/flags are preserved. This is the same
    // behavior of session_manager daemon.
    DCHECK(session_manager->primary_user_id().has_value());
    const auto it = sessions.find(*session_manager->primary_user_id());
    user_id = it->first;
    user_profile = it->second;
  }

  base::Value::Dict cached_state;

  // Restart job command line should already contain login user and profile
  // switches, no reason to store it separately.
  if (!has_restart_job && !user_id.empty()) {
    DCHECK(!user_profile.empty());
    cached_state.Set(kUserIdKey, user_id);
    cached_state.Set(kUserHashKey, user_profile);
  }

  std::vector<Switch> user_flag_args;
  std::vector<std::string> raw_flags;
  const bool has_user_flags = FakeSessionManagerClient::Get()->GetFlagsForUser(
      cryptohome::CreateAccountIdentifierFromIdentification(
          cryptohome::Identification::FromString(user_id)),
      &raw_flags);
  if (has_user_flags) {
    std::vector<std::string> argv = {"" /* Empty program */};
    argv.insert(argv.end(), raw_flags.begin(), raw_flags.end());
    cached_state.Set(kUserFlagsKey, GetSwitchesValueFromArgv(argv));
  }

  if (has_restart_job) {
    const std::vector<std::string>& argv =
        FakeSessionManagerClient::Get()->restart_job_argv().value();
    DCHECK(
        base::Contains(argv, base::StringPrintf("--%s=%s", switches::kLoginUser,
                                                user_manager::kGuestUserName)));
    DCHECK(base::Contains(
        argv, base::StringPrintf("--%s=%s", switches::kLoginProfile, "user")));

    cached_state.Set(kRestartJobKey, GetSwitchesValueFromArgv(argv));
  }

  JSONFileValueSerializer serializer(backing_file_);
  serializer.Serialize(cached_state);
}

base::Value::List SessionFlagsManager::GetSwitchesValueFromArgv(
    const std::vector<std::string>& argv) {
  // Parse flag name-value pairs using command line initialization.
  base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
  cmd_line.InitFromArgv(argv);

  base::Value::List flag_list;
  for (const auto& flag : cmd_line.GetSwitches()) {
    auto flag_value = base::Value::Dict()
                          .Set(kFlagNameKey, flag.first)
                          .Set(kFlagValueKey, flag.second);
    flag_list.Append(std::move(flag_value));
  }
  return flag_list;
}

}  // namespace ash::test
