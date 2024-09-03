// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CHROME_BACKUP_AGENT_H_
#define CHROME_BROWSER_ANDROID_CHROME_BACKUP_AGENT_H_

#include <string>

class PrefService;

namespace chrome_backup_agent {

// Underlying implementation of the corresponding JNI_ChromeBackupAgentImpl_*
// functions, exposed here for testing, because the JNI functions have internal
// linkage and can't be called from the test.
std::string GetSerializedDict(PrefService* pref_service,
                              const std::string& pref_name);
void SetDict(PrefService* pref_service,
             const std::string& pref_name,
             const std::string& serialized_dict);

}  // namespace chrome_backup_agent

#endif  // CHROME_BROWSER_ANDROID_CHROME_BACKUP_AGENT_H_
