// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_LOGGING_UTIL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_LOGGING_UTIL_H_

#include <string>

#include "components/autofill/core/common/save_password_progress_logger.h"

namespace password_manager {
class PasswordManagerClient;
}


namespace optimization_guide::proto {
class PasswordChangeResponse;
}


namespace password_change {

void LogMessage(password_manager::PasswordManagerClient* client,
                autofill::SavePasswordProgressLogger::StringID message_id);

void LogBoolean(password_manager::PasswordManagerClient* client,
                autofill::SavePasswordProgressLogger::StringID message_id,
                bool value);

void LogNumber(password_manager::PasswordManagerClient* client,
               autofill::SavePasswordProgressLogger::StringID message_id,
               int value);

void LogString(password_manager::PasswordManagerClient* client,
               autofill::SavePasswordProgressLogger::StringID message_id,
               const std::string& value);

void LogResponse(
    password_manager::PasswordManagerClient* client,
    autofill::SavePasswordProgressLogger::StringID message_id,
    const optimization_guide::proto::PasswordChangeResponse& response);

}  // namespace password_change

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_LOGGING_UTIL_H_
