// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_MESSAGE_BRIDGE_FACTORY_H_
#define CHROME_BROWSER_SHARING_SHARING_MESSAGE_BRIDGE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

class SharingMessageBridge;

// Factory for sharing message bridge. We need this factory to prevent cyclic
// dependency between SharingServiceFactory and SyncServiceFactory.
class SharingMessageBridgeFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns singleton instance of SharingMessageBridgeFactory.
  static SharingMessageBridgeFactory* GetInstance();

  // Returns the SharingMessageBridge associated with |context|.
  static SharingMessageBridge* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend base::NoDestructor<SharingMessageBridgeFactory>;

  SharingMessageBridgeFactory();
  ~SharingMessageBridgeFactory() override;
  SharingMessageBridgeFactory(const SharingMessageBridgeFactory&) = delete;
  SharingMessageBridgeFactory& operator=(const SharingMessageBridgeFactory&) =
      delete;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SHARING_SHARING_MESSAGE_BRIDGE_FACTORY_H_
