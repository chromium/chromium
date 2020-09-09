// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/browser.h"

namespace {

const char kIdParam[] = "id";
const char kMethodParam[] = "method";
const char kParamsParam[] = "params";

}  // namespace

DevToolsProtocolTestBase::DevToolsProtocolTestBase() = default;

DevToolsProtocolTestBase::~DevToolsProtocolTestBase() = default;

void DevToolsProtocolTestBase::TearDownOnMainThread() {
  Detach();
}

void DevToolsProtocolTestBase::DispatchProtocolMessage(
    content::DevToolsAgentHost* agent_host,
    base::span<const uint8_t> message) {
  base::StringPiece message_str(reinterpret_cast<const char*>(message.data()),
                                message.size());
  base::Optional<base::Value> parsed_message =
      base::JSONReader::Read(message_str);
  ASSERT_TRUE(parsed_message.has_value());
  if (auto id = parsed_message->FindIntPath("id")) {
    base::Value* result = parsed_message->FindDictPath("result");
    ASSERT_TRUE(result);
    result_ = result->Clone();
    in_dispatch_ = false;
    if (*id && *id == waiting_for_command_result_id_) {
      waiting_for_command_result_id_ = 0;
      std::move(run_loop_quit_closure_).Run();
    }
  } else {
    std::string* notification = parsed_message->FindStringPath("method");
    ASSERT_TRUE(notification);
    notifications_.push_back(*notification);
    base::Value* params = parsed_message->FindPath("params");
    notification_params_.push_back(params ? params->Clone() : base::Value());
    if (waiting_for_notification_ == *notification &&
        (waiting_for_notification_matcher_.is_null() ||
         waiting_for_notification_matcher_.Run(notification_params_.back()))) {
      waiting_for_notification_.clear();
      waiting_for_notification_matcher_ = NotificationMatcher();
      waiting_for_notification_params_ = notification_params_.back().Clone();
      std::move(run_loop_quit_closure_).Run();
    }
  }
}

void DevToolsProtocolTestBase::SendCommand(const std::string& method,
                                           base::Value params,
                                           bool synchronous) {
  in_dispatch_ = true;
  base::Value command(base::Value::Type::DICTIONARY);
  command.SetKey(kIdParam, base::Value(++last_sent_id_));
  command.SetKey(kMethodParam, base::Value(method));
  if (!params.is_none())
    command.SetPath(kParamsParam, std::move(params));
  std::string json_command;
  base::JSONWriter::Write(command, &json_command);
  agent_host_->DispatchProtocolMessage(
      this, base::as_bytes(base::make_span(json_command)));
  // Some messages are dispatched synchronously.
  // Only run loop if we are not finished yet.
  if (in_dispatch_ && synchronous)
    WaitForResponse();
  in_dispatch_ = false;
}

void DevToolsProtocolTestBase::WaitForResponse() {
  waiting_for_command_result_id_ = last_sent_id_;
  RunLoopUpdatingQuitClosure();
}

void DevToolsProtocolTestBase::RunLoopUpdatingQuitClosure() {
  base::RunLoop run_loop;
  CHECK(!run_loop_quit_closure_);
  run_loop_quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

void DevToolsProtocolTestBase::AttachToBrowser() {
  agent_host_ = content::DevToolsAgentHost::CreateForBrowser(
      nullptr, content::DevToolsAgentHost::CreateServerSocketCallback());
  agent_host_->AttachClient(this);
}

void DevToolsProtocolTestBase::Attach() {
  agent_host_ = content::DevToolsAgentHost::GetOrCreateFor(web_contents());
  agent_host_->AttachClient(this);
}

void DevToolsProtocolTestBase::Detach() {
  if (agent_host_) {
    agent_host_->DetachClient(this);
    agent_host_ = nullptr;
  }
}

content::WebContents* DevToolsProtocolTestBase::web_contents() {
  return browser()->tab_strip_model()->GetWebContentsAt(0);
}

base::Value DevToolsProtocolTestBase::WaitForNotification(
    const std::string& notification) {
  auto always_match =
      base::BindRepeating([](const base::Value&) { return true; });
  return WaitForMatchingNotification(notification, always_match);
}

base::Value DevToolsProtocolTestBase::WaitForMatchingNotification(
    const std::string& notification,
    const NotificationMatcher& matcher) {
  for (size_t i = 0; i < notifications_.size(); ++i) {
    if (notifications_[i] == notification &&
        matcher.Run(notification_params_[i])) {
      base::Value result = std::move(notification_params_[i]);
      notifications_.erase(notifications_.begin() + i);
      notification_params_.erase(notification_params_.begin() + i);
      return result;
    }
  }
  waiting_for_notification_ = notification;
  waiting_for_notification_matcher_ = matcher;
  RunLoopUpdatingQuitClosure();
  return std::move(waiting_for_notification_params_);
}

void DevToolsProtocolTestBase::AgentHostClosed(
    content::DevToolsAgentHost* agent_host) {}
