// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CUSTOM_HANDLERS_TEST_PROTOCOL_HANDLER_REGISTRY_DELEGATE_H_
#define CHROME_BROWSER_CUSTOM_HANDLERS_TEST_PROTOCOL_HANDLER_REGISTRY_DELEGATE_H_

#include <set>
#include <string>

#include "chrome/browser/custom_handlers/protocol_handler_registry.h"

// A test ProtocolHandlerRegistry::Delegate implementation that keeps track of
// registered protocols and doesn't change any OS settings.
class TestProtocolHandlerRegistryDelegate
    : public ProtocolHandlerRegistry::Delegate {
 public:
  TestProtocolHandlerRegistryDelegate();
  ~TestProtocolHandlerRegistryDelegate() override;

  TestProtocolHandlerRegistryDelegate(
      const TestProtocolHandlerRegistryDelegate& other) = delete;
  TestProtocolHandlerRegistryDelegate& operator=(
      const TestProtocolHandlerRegistryDelegate& other) = delete;

  // ProtocolHandlerRegistry::Delegate:
  void RegisterExternalHandler(const std::string& protocol) override;
  void DeregisterExternalHandler(const std::string& protocol) override;
  bool IsExternalHandlerRegistered(const std::string& protocol) override;
  void RegisterWithOSAsDefaultClient(
      const std::string& protocol,
      shell_integration::DefaultWebClientWorkerCallback callback) override;
  void CheckDefaultClientWithOS(
      const std::string& protocol,
      shell_integration::DefaultWebClientWorkerCallback callback) override;

 private:
  // Holds registered protocols.
  std::set<std::string> registered_protocols_;
};

#endif  // CHROME_BROWSER_CUSTOM_HANDLERS_TEST_PROTOCOL_HANDLER_REGISTRY_DELEGATE_H_
