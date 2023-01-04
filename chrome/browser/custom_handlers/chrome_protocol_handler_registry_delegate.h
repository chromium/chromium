// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CUSTOM_HANDLERS_CHROME_PROTOCOL_HANDLER_REGISTRY_DELEGATE_H_
#define CHROME_BROWSER_CUSTOM_HANDLERS_CHROME_PROTOCOL_HANDLER_REGISTRY_DELEGATE_H_

#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/shell_integration.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/pref_registry/pref_registry_syncable.h"

// This class implements the ProtocolHandlerRegistry::Delegate
// abstract class to provide an OS dependent implementation to handle
// the user preferences and deal with the //shell_integration module.
class ChromeProtocolHandlerRegistryDelegate
    : public custom_handlers::ProtocolHandlerRegistry::Delegate {
 public:
  ChromeProtocolHandlerRegistryDelegate();
  ~ChromeProtocolHandlerRegistryDelegate() override;

  ChromeProtocolHandlerRegistryDelegate(
      const ChromeProtocolHandlerRegistryDelegate& other) = delete;
  ChromeProtocolHandlerRegistryDelegate& operator=(
      const ChromeProtocolHandlerRegistryDelegate& other) = delete;

  // ProtocolHandlerRegistry::Delegate:
  void RegisterExternalHandler(const std::string& protocol) override;
  bool IsExternalHandlerRegistered(const std::string& protocol) override;
  void RegisterWithOSAsDefaultClient(const std::string& protocol,
                                     DefaultClientCallback callback) override;
  void CheckDefaultClientWithOS(const std::string& protocol,
                                DefaultClientCallback callback) override;
  bool ShouldRemoveHandlersNotInOS() override;

 private:
  // Gets the callback for DefaultSchemeClientWorker.
  shell_integration::DefaultWebClientWorkerCallback GetDefaultWebClientCallback(
      const std::string& protocol,
      DefaultClientCallback callback);

  // Called with the default state when the default protocol client worker is
  // done.
  void OnSetAsDefaultClientForSchemeFinished(
      const std::string& protocol,
      DefaultClientCallback callback,
      shell_integration::DefaultWebClientState state);

  // Makes it possible to invalidate the callback for the
  // DefaultSchemeClientWorker.
  base::WeakPtrFactory<ChromeProtocolHandlerRegistryDelegate> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_CUSTOM_HANDLERS_CHROME_PROTOCOL_HANDLER_REGISTRY_DELEGATE_H_
