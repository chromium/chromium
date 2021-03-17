// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BRAILLE_DISPLAY_PRIVATE_BRAILLE_DISPLAY_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_BRAILLE_DISPLAY_PRIVATE_BRAILLE_DISPLAY_PRIVATE_API_H_

#include "base/scoped_observer.h"
#include "chrome/browser/extensions/api/braille_display_private/braille_controller.h"
#include "chrome/common/extensions/api/braille_display_private.h"
#include "extensions/browser/api/async_api_function.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"

class Profile;

namespace extensions {
namespace api {
namespace braille_display_private {
class BrailleDisplayPrivateAPIUserTest;
}  // namespace braille_display_private
}  // namespace api

// Implementation of the chrome.brailleDisplayPrivate API.
class BrailleDisplayPrivateAPI : public BrowserContextKeyedAPI,
                                 api::braille_display_private::BrailleObserver,
                                 EventRouter::Observer {
 public:
  explicit BrailleDisplayPrivateAPI(content::BrowserContext* context);
  ~BrailleDisplayPrivateAPI() override;

  // ProfileKeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<BrailleDisplayPrivateAPI>*
      GetFactoryInstance();

  // BrailleObserver implementation.
  void OnBrailleDisplayStateChanged(
      const api::braille_display_private::DisplayState& display_state) override;
  void OnBrailleKeyEvent(
      const api::braille_display_private::KeyEvent& keyEvent) override;

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<BrailleDisplayPrivateAPI>;
  friend class api::braille_display_private::BrailleDisplayPrivateAPIUserTest;

  class EventDelegate {
   public:
    virtual ~EventDelegate() {}
    virtual void BroadcastEvent(std::unique_ptr<Event> event) = 0;
    virtual bool HasListener() = 0;
  };

  class DefaultEventDelegate;

  // Returns whether the profile that this API was created for is currently
  // the active profile.
  bool IsProfileActive();

  void SetEventDelegateForTest(std::unique_ptr<EventDelegate> delegate);

  Profile* profile_;
  ScopedObserver<api::braille_display_private::BrailleController,
                 BrailleObserver> scoped_observer_;
  std::unique_ptr<EventDelegate> event_delegate_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "BrailleDisplayPrivateAPI";
  }
  // Override the default so the service is not created in tests.
  static const bool kServiceIsNULLWhileTesting = true;
};

namespace api {

class BrailleDisplayPrivateGetDisplayStateFunction : public AsyncApiFunction {
  DECLARE_EXTENSION_FUNCTION("brailleDisplayPrivate.getDisplayState",
                             BRAILLEDISPLAYPRIVATE_GETDISPLAYSTATE)
 protected:
  ~BrailleDisplayPrivateGetDisplayStateFunction() override {}
  bool Prepare() override;
  void Work() override;
  bool Respond() override;
};

class BrailleDisplayPrivateWriteDotsFunction : public AsyncApiFunction {
  DECLARE_EXTENSION_FUNCTION("brailleDisplayPrivate.writeDots",
                             BRAILLEDISPLAYPRIVATE_WRITEDOTS)
 public:
  BrailleDisplayPrivateWriteDotsFunction();

 protected:
  ~BrailleDisplayPrivateWriteDotsFunction() override;
  bool Prepare() override;
  void Work() override;
  bool Respond() override;

 private:
  std::unique_ptr<braille_display_private::WriteDots::Params> params_;
};

class BrailleDisplayPrivateUpdateBluetoothBrailleDisplayAddressFunction
    : public ExtensionFunction {
  ~BrailleDisplayPrivateUpdateBluetoothBrailleDisplayAddressFunction()
      override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION(
      "brailleDisplayPrivate.updateBluetoothBrailleDisplayAddress",
      BRAILLEDISPLAYPRIVATE_UPDATEBLUETOOTHBRAILLEDISPLAYADDRESS)
};

}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BRAILLE_DISPLAY_PRIVATE_BRAILLE_DISPLAY_PRIVATE_API_H_
