// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/custom_handlers/test_protocol_handler_registry_delegate.h"

#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/threading/thread_task_runner_handle.h"

TestProtocolHandlerRegistryDelegate::TestProtocolHandlerRegistryDelegate() =
    default;
TestProtocolHandlerRegistryDelegate::~TestProtocolHandlerRegistryDelegate() =
    default;

// ProtocolHandlerRegistry::Delegate:
void TestProtocolHandlerRegistryDelegate::RegisterExternalHandler(
    const std::string& protocol) {
  bool inserted = registered_protocols_.insert(protocol).second;
  DCHECK(inserted);
}

void TestProtocolHandlerRegistryDelegate::DeregisterExternalHandler(
    const std::string& protocol) {
  size_t removed = registered_protocols_.erase(protocol);
  DCHECK_EQ(removed, 1u);
}

bool TestProtocolHandlerRegistryDelegate::IsExternalHandlerRegistered(
    const std::string& protocol) {
  return registered_protocols_.find(protocol) != registered_protocols_.end();
}

void TestProtocolHandlerRegistryDelegate::RegisterWithOSAsDefaultClient(
    const std::string& protocol,
    shell_integration::DefaultWebClientWorkerCallback callback) {
  // Respond asynchronously to mimic the real behavior.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), shell_integration::IS_DEFAULT));
}

void TestProtocolHandlerRegistryDelegate::CheckDefaultClientWithOS(
    const std::string& protocol,
    shell_integration::DefaultWebClientWorkerCallback callback) {
  // Respond asynchronously to mimic the real behavior.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), shell_integration::IS_DEFAULT));
}
