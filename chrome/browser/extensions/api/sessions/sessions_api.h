// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SESSIONS_SESSIONS_API_H__
#define CHROME_BROWSER_EXTENSIONS_API_SESSIONS_SESSIONS_API_H__

#include <string>

#include "base/macros.h"
#include "chrome/common/extensions/api/sessions.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/extensions/api/windows.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_observer.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"

class Browser;
class Profile;

namespace sync_sessions {
struct SyncedSession;
}

namespace extensions {

class SessionId;

class SessionsGetRecentlyClosedFunction : public ExtensionFunction {
 protected:
  ~SessionsGetRecentlyClosedFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("sessions.getRecentlyClosed",
                             SESSIONS_GETRECENTLYCLOSED)

 private:
  api::tabs::Tab CreateTabModel(const sessions::TabRestoreService::Tab& tab,
                                bool active);
  std::unique_ptr<api::windows::Window> CreateWindowModel(
      const sessions::TabRestoreService::Window& window);
  std::unique_ptr<api::sessions::Session> CreateSessionModel(
      const sessions::TabRestoreService::Entry& entry);
};

class SessionsGetDevicesFunction : public ExtensionFunction {
 protected:
  ~SessionsGetDevicesFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("sessions.getDevices", SESSIONS_GETDEVICES)

 private:
  api::tabs::Tab CreateTabModel(const std::string& session_tag,
                                const sessions::SessionTab& tab,
                                int tab_index,
                                bool active);
  std::unique_ptr<api::windows::Window> CreateWindowModel(
      const sessions::SessionWindow& window,
      const std::string& session_tag);
  std::unique_ptr<api::sessions::Session> CreateSessionModel(
      const sessions::SessionWindow& window,
      const std::string& session_tag);
  api::sessions::Device CreateDeviceModel(
      const sync_sessions::SyncedSession* session);
};

class SessionsRestoreFunction : public ExtensionFunction {
 protected:
  ~SessionsRestoreFunction() override {}
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("sessions.restore", SESSIONS_RESTORE)

 private:
  ResponseValue GetRestoredTabResult(content::WebContents* contents);
  ResponseValue GetRestoredWindowResult(int window_id);
  ResponseValue RestoreMostRecentlyClosed(Browser* browser);
  ResponseValue RestoreLocalSession(const SessionId& session_id,
                                    Browser* browser);
  ResponseValue RestoreForeignSession(const SessionId& session_id,
                                      Browser* browser);
};

class SessionsEventRouter : public sessions::TabRestoreServiceObserver {
 public:
  explicit SessionsEventRouter(Profile* profile);
  ~SessionsEventRouter() override;

  // Observer callback for TabRestoreServiceObserver. Sends data on
  // recently closed tabs to the javascript side of this page to
  // display to the user.
  void TabRestoreServiceChanged(sessions::TabRestoreService* service) override;

  // Observer callback to notice when our associated TabRestoreService
  // is destroyed.
  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override;

 private:
  Profile* profile_;

  // TabRestoreService that we are observing.
  sessions::TabRestoreService* tab_restore_service_;

  DISALLOW_COPY_AND_ASSIGN(SessionsEventRouter);
};

class SessionsAPI : public BrowserContextKeyedAPI,
                    public extensions::EventRouter::Observer {
 public:
  explicit SessionsAPI(content::BrowserContext* context);
  ~SessionsAPI() override;

  // BrowserContextKeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<SessionsAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const extensions::EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<SessionsAPI>;

  content::BrowserContext* browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "SessionsAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<SessionsEventRouter> sessions_event_router_;

  DISALLOW_COPY_AND_ASSIGN(SessionsAPI);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SESSIONS_SESSIONS_API_H__
