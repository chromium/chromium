// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BACKUP_DICT_PREF_BACKUP_SERIALIZER_H_
#define CHROME_BROWSER_ANDROID_BACKUP_DICT_PREF_BACKUP_SERIALIZER_H_

#include <string>

class PrefService;

namespace dict_pref_backup_serializer {

// Underlying implementation of the corresponding JNI_DictPrefBackupSerializer_*
// functions, exposed here for testing, because the JNI functions have internal
// linkage and can't be called from the test.
std::string GetSerializedDict(PrefService* pref_service,
                              const std::string& pref_name);
void SetDict(PrefService* pref_service,
             const std::string& pref_name,
             const std::string& serialized_dict);

}  // namespace dict_pref_backup_serializer

#endif  // CHROME_BROWSER_ANDROID_BACKUP_DICT_PREF_BACKUP_SERIALIZER_H_
