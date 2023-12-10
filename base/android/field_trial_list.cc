// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include <map>
#include <string>

#include "base/android/jni_string.h"
#include "base/base_jni/FieldTrialList_jni.h"
#include "base/lazy_instance.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_list_including_low_anonymity.h"
#include "base/metrics/field_trial_params.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

// Log trials and their groups on activation, for debugging purposes.
class TrialLogger : public base::FieldTrialList::Observer {
 public:
  TrialLogger() {}

  TrialLogger(const TrialLogger&) = delete;
  TrialLogger& operator=(const TrialLogger&) = delete;

  void OnFieldTrialGroupFinalized(const base::FieldTrial& trial,
                                  const std::string& group_name) override {
    Log(trial.trial_name(), group_name);
  }

  static void Log(const std::string& trial_name,
                  const std::string& group_name) {
    // Changes to format of the log message below must be accompanied by
    // changes to finch smoke tests since they look for this log message
    // in the logcat.
    LOG(INFO) << "Active field trial \"" << trial_name
              << "\" in group \"" << group_name<< '"';
  }

 protected:
  ~TrialLogger() override {}
};

base::LazyInstance<TrialLogger>::Leaky g_trial_logger =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

static ScopedJavaLocalRef<jstring> JNI_FieldTrialList_FindFullName(
    JNIEnv* env,
    const JavaParamRef<jstring>& jtrial_name) {
  std::string trial_name(ConvertJavaStringToUTF8(env, jtrial_name));
  return ConvertUTF8ToJavaString(
      env, base::FieldTrialList::FindFullName(trial_name));
}

static jboolean JNI_FieldTrialList_TrialExists(
    JNIEnv* env,
    const JavaParamRef<jstring>& jtrial_name) {
  std::string trial_name(ConvertJavaStringToUTF8(env, jtrial_name));
  return base::FieldTrialList::TrialExists(trial_name);
}

static ScopedJavaLocalRef<jstring> JNI_FieldTrialList_GetVariationParameter(
    JNIEnv* env,
    const JavaParamRef<jstring>& jtrial_name,
    const JavaParamRef<jstring>& jparameter_key) {
  std::map<std::string, std::string> parameters;
  base::GetFieldTrialParams(ConvertJavaStringToUTF8(env, jtrial_name),
                            &parameters);
  return ConvertUTF8ToJavaString(
      env, parameters[ConvertJavaStringToUTF8(env, jparameter_key)]);
}

// JNI_FieldTrialList_LogActiveTrials() is static function, this makes friending
// it a hassle because it must be declared in the file that the friend
// declaration is in, but its declaration can't be included in multiple places
// or things get messy and the linker gets mad. This helper class exists only to
// friend the JNI function and is, in turn, friended by
// FieldTrialListIncludingLowAnonymity which allows for the private
// GetActiveFieldTrialGroups() to be reached.
class AndroidFieldTrialListLogActiveTrialsFriendHelper {
 private:
  friend void ::JNI_FieldTrialList_LogActiveTrials(JNIEnv* env);

  static bool AddObserver(base::FieldTrialList::Observer* observer) {
    return base::FieldTrialListIncludingLowAnonymity::AddObserver(observer);
  }

  static void GetActiveFieldTrialGroups(
      base::FieldTrial::ActiveGroups* active_groups) {
    base::FieldTrialListIncludingLowAnonymity::GetActiveFieldTrialGroups(
        active_groups);
  }
};

static void JNI_FieldTrialList_LogActiveTrials(JNIEnv* env) {
  DCHECK(!g_trial_logger.IsCreated()); // This need only be called once.

  LOG(INFO) << "Logging active field trials...";
  AndroidFieldTrialListLogActiveTrialsFriendHelper::AddObserver(
      &g_trial_logger.Get());

  // Log any trials that were already active before adding the observer.
  std::vector<base::FieldTrial::ActiveGroup> active_groups;
  AndroidFieldTrialListLogActiveTrialsFriendHelper::GetActiveFieldTrialGroups(
      &active_groups);
  for (const base::FieldTrial::ActiveGroup& group : active_groups) {
    TrialLogger::Log(group.trial_name, group.group_name);
  }
}

static jboolean JNI_FieldTrialList_CreateFieldTrial(
    JNIEnv* env,
    const JavaParamRef<jstring>& jtrial_name,
    const JavaParamRef<jstring>& jgroup_name) {
  return base::FieldTrialList::CreateFieldTrial(
             ConvertJavaStringToUTF8(env, jtrial_name),
             ConvertJavaStringToUTF8(env, jgroup_name)) != nullptr;
}
