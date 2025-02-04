// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"

#include "base/android/jni_string.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/command_line_jni/CommandLine_jni.h"

using base::CommandLine;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

void AppendToCommandLine(std::vector<std::string>& vec, bool includes_program) {
  if (!includes_program) {
    vec.insert(vec.begin(), std::string());
  }
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

static CommandLine::SwitchMap JNI_CommandLine_GetSwitches(JNIEnv* env) {
  return CommandLine::ForCurrentProcess()->GetSwitches();
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
