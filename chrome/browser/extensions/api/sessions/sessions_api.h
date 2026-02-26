// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SESSIONS_SESSIONS_API_H__
#define CHROME_BROWSER_EXTENSIONS_API_SESSIONS_SESSIONS_API_H__

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/common/extensions/api/sessions.h"
#include "chrome/common/extensions/api/tab_groups.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/extensions/api/windows.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_observer.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class BrowserWindowInterface;
class Profile;

namespace sync_sessions {
struct SyncedSession;
}

namespace extensions {

class SessionId;

class SessionsGetRecentlyClosedFunction : public ExtensionFunction {
 public:
  SessionsGetRecentlyClosedFunction();
  SessionsGetRecentlyClosedFunction(const SessionsGetRecentlyClosedFunction&) =
      delete;
  SessionsGetRecentlyClosedFunction& operator=(
      const SessionsGetRecentlyClosedFunction&) = delete;

  void set_window_last_modified_for_test(int last_modified) {
    window_last_modified_for_test_ = last_modified;
  }

 protected:
  // Ref-counted so protected destructor.
  ~SessionsGetRecentlyClosedFunction() override;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("sessions.getRecentlyClosed",
                             SESSIONS_GETRECENTLYCLOSED)

 private:
  api::tabs::Tab CreateTabModel(const sessions::tab_restore::Tab& tab,
                                bool active);
  api::windows::Window CreateWindowModel(
      const sessions::tab_restore::Window& window);
  api::tab_groups::TabGroup CreateGroupModel(
      const sessions::tab_restore::Group& group);
  api::sessions::Session CreateSessionModel(
      const sessions::tab_restore::Entry& entry);
#if BUILDFLAG(IS_ANDROID)
  void OnGetRecentlyClosedWindow(
      const base::android::JavaRef<jobject>& j_tab_model);
#endif  // BUILDFLAG(IS_ANDROID)

  size_t max_results_ = api::sessions::MAX_SESSION_RESULTS;
  std::vector<api::sessions::Session> result_;

  // If non-zero, used as the last modified time (in seconds from epoch) for
  // the session for a window close. Used to ensure timestamps don't have the
  // same value (since they only have second-level precision).
  int window_last_modified_for_test_ = 0;
};

class SessionsGetDevicesFunction : public ExtensionFunction {
 protected:
  ~SessionsGetDevicesFunction() override = default;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("sessions.getDevices", SESSIONS_GETDEVICES)

 private:
  api::tabs::Tab CreateTabModel(const std::string& session_tag,
                                const sessions::SessionTab& tab,
                                int tab_index,
                                bool active);
  std::optional<api::windows::Window> CreateWindowModel(
      const sessions::SessionWindow& window,
      const std::string& session_tag);
  std::optional<api::sessions::Session> CreateSessionModel(
      const sessions::SessionWindow& window,
      const std::string& session_tag);
  api::sessions::Device CreateDeviceModel(
      const sync_sessions::SyncedSession* session);
};

class SessionsRestoreFunction : public ExtensionFunction {
 public:
  SessionsRestoreFunction();
  SessionsRestoreFunction(const SessionsRestoreFunction&) = delete;
  SessionsRestoreFunction& operator=(const SessionsRestoreFunction&) = delete;

 protected:
  // Ref-counted so protected destructor.
  ~SessionsRestoreFunction() override;
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("sessions.restore", SESSIONS_RESTORE)

 private:
  ResponseValue GetRestoredTabResult(content::WebContents* contents);
  ResponseValue GetRestoredWindowResult(int window_id);
  ResponseAction RestoreMostRecentlyClosed(BrowserWindowInterface* browser);
  ResponseAction RestoreLocalSession(const SessionId& session_id,
                                     BrowserWindowInterface* browser);
  ResponseAction RestoreForeignSession(const SessionId& session_id,
                                       BrowserWindowInterface* browser);
  void OnRestoreForeignSessionWindows(
      std::vector<BrowserWindowInterface*> browsers);

#if BUILDFLAG(IS_ANDROID)
  // Uses JNI to query Java `RecentlyClosedEntitiesManager` for recently closed
  // windows. If instance_id is kInvalidWindowId it returns the most closed.
  // Otherwise it only returns a window with a matching instance id.
  ResponseAction QueryRecentlyClosedEntitiesManager(int instance_id);

  // Callback for `QueryRecentlyClosedEntitiesManager()`.
  void OnGetRecentlyClosedWindow(
      const base::android::JavaRef<jobject>& j_tab_model);

  // Callback for browser window creation.
  void OnBrowserWindowCreated(BrowserWindowInterface* browser);

  // A global reference to `TabModel` so it stays alive across callbacks.
  base::android::ScopedJavaGlobalRef<jobject> global_ref_tab_model_;
#endif  // BUILDFLAG(IS_ANDROID)
};

class SessionsEventRouter : public sessions::TabRestoreServiceObserver {
 public:
  explicit SessionsEventRouter(Profile* profile);

  SessionsEventRouter(const SessionsEventRouter&) = delete;
  SessionsEventRouter& operator=(const SessionsEventRouter&) = delete;

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
#if BUILDFLAG(IS_ANDROID)
  // Callback for when the recently closed list is updated on the Java side.
  void OnRecentlyClosedUpdated(int64_t j_browser_context);
#endif  // BUILDFLAG(IS_ANDROID)

  // Broadcasts the API OnChanged event to JS.
  static void BroadcastOnChangedEvent(Profile* profile);

  raw_ptr<Profile> profile_;

  // TabRestoreService that we are observing.
  raw_ptr<sessions::TabRestoreService> tab_restore_service_;
};

class SessionsAPI : public BrowserContextKeyedAPI,
                    public extensions::EventRouter::Observer {
 public:
  explicit SessionsAPI(content::BrowserContext* context);

  SessionsAPI(const SessionsAPI&) = delete;
  SessionsAPI& operator=(const SessionsAPI&) = delete;

  ~SessionsAPI() override;

  // BrowserContextKeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<SessionsAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const extensions::EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<SessionsAPI>;

  raw_ptr<content::BrowserContext> browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "SessionsAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<SessionsEventRouter> sessions_event_router_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SESSIONS_SESSIONS_API_H__
