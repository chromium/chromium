// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/sw_reporter_invocation_win.h"

#include <utility>

namespace safe_browsing {

SwReporterInvocation::SwReporterInvocation(
    const base::CommandLine& command_line)
    : command_line_(command_line) {}

SwReporterInvocation::SwReporterInvocation(const SwReporterInvocation& other)
    : command_line_(other.command_line_),
      supported_behaviours_(other.supported_behaviours_),
      suffix_(other.suffix_),
      reporter_logs_upload_enabled_(other.reporter_logs_upload_enabled_),
      cleaner_logs_upload_enabled_(other.cleaner_logs_upload_enabled_),
      chrome_prompt_(other.chrome_prompt_) {}

void SwReporterInvocation::operator=(const SwReporterInvocation& invocation) {
  command_line_ = invocation.command_line_;
  supported_behaviours_ = invocation.supported_behaviours_;
  suffix_ = invocation.suffix_;
  reporter_logs_upload_enabled_ = invocation.reporter_logs_upload_enabled_;
  cleaner_logs_upload_enabled_ = invocation.cleaner_logs_upload_enabled_;
  chrome_prompt_ = invocation.chrome_prompt_;
}

SwReporterInvocation& SwReporterInvocation::WithSuffix(
    const std::string& suffix) {
  suffix_ = suffix;
  return *this;
}

SwReporterInvocation& SwReporterInvocation::WithSupportedBehaviours(
    Behaviours supported_behaviours) {
  supported_behaviours_ = supported_behaviours;
  return *this;
}

bool SwReporterInvocation::operator==(const SwReporterInvocation& other) const {
  return command_line_.argv() == other.command_line_.argv() &&
         supported_behaviours_ == other.supported_behaviours_ &&
         suffix_ == other.suffix_ &&
         reporter_logs_upload_enabled_ == other.reporter_logs_upload_enabled_ &&
         cleaner_logs_upload_enabled_ == other.cleaner_logs_upload_enabled_ &&
         chrome_prompt_ == other.chrome_prompt_;
}

const base::CommandLine& SwReporterInvocation::command_line() const {
  return command_line_;
}

base::CommandLine& SwReporterInvocation::mutable_command_line() {
  return command_line_;
}

SwReporterInvocation::Behaviours SwReporterInvocation::supported_behaviours()
    const {
  return supported_behaviours_;
}

bool SwReporterInvocation::BehaviourIsSupported(
    SwReporterInvocation::Behaviours intended_behaviour) const {
  return (supported_behaviours_ & intended_behaviour) != 0;
}

std::string SwReporterInvocation::suffix() const {
  return suffix_;
}

bool SwReporterInvocation::reporter_logs_upload_enabled() const {
  return reporter_logs_upload_enabled_;
}

void SwReporterInvocation::set_reporter_logs_upload_enabled(
    bool reporter_logs_upload_enabled) {
  reporter_logs_upload_enabled_ = reporter_logs_upload_enabled;
}

bool SwReporterInvocation::cleaner_logs_upload_enabled() const {
  return cleaner_logs_upload_enabled_;
}

void SwReporterInvocation::set_cleaner_logs_upload_enabled(
    bool cleaner_logs_upload_enabled) {
  cleaner_logs_upload_enabled_ = cleaner_logs_upload_enabled;
}

chrome_cleaner::ChromePromptValue SwReporterInvocation::chrome_prompt() const {
  return chrome_prompt_;
}

void SwReporterInvocation::set_chrome_prompt(
    chrome_cleaner::ChromePromptValue chrome_prompt) {
  chrome_prompt_ = chrome_prompt;
}

SwReporterInvocationSequence::SwReporterInvocationSequence(
    const base::Version& version)
    : version_(version) {}

SwReporterInvocationSequence::SwReporterInvocationSequence(
    SwReporterInvocationSequence&& invocations_sequence)
    : version_(std::move(invocations_sequence.version_)),
      container_(std::move(invocations_sequence.container_)) {}

SwReporterInvocationSequence::SwReporterInvocationSequence(
    const SwReporterInvocationSequence& invocations_sequence)
    : version_(invocations_sequence.version_),
      container_(invocations_sequence.container_) {}

SwReporterInvocationSequence::~SwReporterInvocationSequence() = default;

void SwReporterInvocationSequence::operator=(
    SwReporterInvocationSequence&& invocations_sequence) {
  version_ = std::move(invocations_sequence.version_);
  container_ = std::move(invocations_sequence.container_);
}

void SwReporterInvocationSequence::PushInvocation(
    const SwReporterInvocation& invocation) {
  container_.push(invocation);
}

base::Version SwReporterInvocationSequence::version() const {
  return version_;
}

const SwReporterInvocationSequence::Queue&
SwReporterInvocationSequence::container() const {
  return container_;
}

SwReporterInvocationSequence::Queue&
SwReporterInvocationSequence::mutable_container() {
  return container_;
}

}  // namespace safe_browsing
