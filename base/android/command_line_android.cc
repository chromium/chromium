// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"

#include "base/android/jni_string.h"
#include "build/robolectric_buildflags.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#if BUILDFLAG(IS_ROBOLECTRIC)
#include "base/base_robolectric_jni/CommandLine_jni.h"  // nogncheck
#else
#include "base/command_line_jni/CommandLine_jni.h"
#endif

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::CommandLine;

namespace {

void AppendToCommandLine(std::vector<std::string>& vec, bool includes_program) {
  if (!includes_program)
    vec.insert(vec.begin(), std::string());
  CommandLine extra_command_line(vec);
  CommandLine::ForCurrentProcess()->AppendArguments(extra_command_line,
                                                    includes_program);
}

}  // namespace

static jboolean JNI_CommandLine_HasSwitch(JNIEnv* env,
                                          std::string& switch_string) {
  return CommandLine::ForCurrentProcess()->HasSwitch(switch_string);
}

static std::string JNI_CommandLine_GetSwitchValue(JNIEnv* env,
                                                  std::string& switch_string) {
  return CommandLine::ForCurrentProcess()->GetSwitchValueNative(switch_string);
}

static std::vector<std::string> JNI_CommandLine_GetSwitchesFlattened(
    JNIEnv* env) {
  // JNI doesn't support returning Maps. Instead, express this map as a 1
  // dimensional array: [ key1, value1, key2, value2, ... ]
  std::vector<std::string> keys_and_values;
  for (const auto& entry : CommandLine::ForCurrentProcess()->GetSwitches()) {
    keys_and_values.push_back(entry.first);
    keys_and_values.push_back(entry.second);
  }
  return keys_and_values;
}

static void JNI_CommandLine_AppendSwitch(JNIEnv* env,
                                         std::string& switch_string) {
  CommandLine::ForCurrentProcess()->AppendSwitch(switch_string);
}

static void JNI_CommandLine_AppendSwitchWithValue(JNIEnv* env,
                                                  std::string& switch_string,
                                                  std::string& value_string) {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(switch_string,
                                                      value_string);
}

static void JNI_CommandLine_AppendSwitchesAndArguments(
    JNIEnv* env,
    std::vector<std::string>& vec) {
  AppendToCommandLine(vec, false);
}

static void JNI_CommandLine_RemoveSwitch(JNIEnv* env,
                                         std::string& switch_string) {
  CommandLine::ForCurrentProcess()->RemoveSwitch(switch_string);
}

static void JNI_CommandLine_Init(JNIEnv* env,
                                 std::vector<std::string>& init_command_line) {
  // TODO(port): Make an overload of Init() that takes StringVector rather than
  // have to round-trip via AppendArguments.
  CommandLine::Init(0, nullptr);
  AppendToCommandLine(init_command_line, true);
}
